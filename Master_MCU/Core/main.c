#include "stm32f10x.h"
#include "Delay.h"
#include "mpu6050.h"
#include "attitude_estimator.h"
#include "motor_pwm.h"
#include "pid_controller.h"
#include "board_led.h"
#include "bluetooth_serial.h"
#include "kalman_filter.h"
#include "watchdog.h"
#include "crsf.h"
#include "slave_link.h"
#include "app_scheduler.h"
#include "flight_safety.h"
#include "board_config.h"
#include "control_timers.h"
#include "millisecond_clock.h"
#include "inter_mcu_protocol.h"

/* Application state, owned by the main loop unless marked volatile. */
volatile float roll,pitch,yaw;
volatile float roll_rate,pitch_rate,yaw_rate;
uint8_t control_throttle_percent;

uint8_t rc_frames_received;

volatile uint32_t system_time_ms = 0;   /* 单调时基，允许自然回绕 */

/* 显式飞行安全状态：启动锁、运行、失联、恢复锁。 */
static flight_safety_context_t flight_safety;

/* 蓝牙调试发送 */
uint8_t telemetry_buffer[32];

/* 遥控通道映射结果 */
uint8_t servo_status = 0;   /* CH4 开关: 0=关 1=开 */
uint8_t mag_calibration_switch     = 0;   /* CH5 开关: 0=关 1=开 */
int16_t rc_roll  = 0;       /* CH0 映射 ±100 */
int16_t rc_pitch = 0;       /* CH1 映射 ±100 */
uint8_t rc_throttle_percent   = 0;       /* CH2 映射 0~100 */
int16_t rc_yaw   = 0;       /* CH3 映射 ±900 */

/* CH6 辅助模式：只有开关、油门、安全状态和有效新帧均满足才接入导航 PID。
 * 默认关闭；第一次进入时捕获当前气压高度，退出时清除导航积分。
 */
static uint8_t nav_requested = 0u;
static uint8_t nav_active = 0u;
static uint32_t last_nav_frame_ms = 0u;
static uint32_t last_nav_pid_ms = 0u;

/* estimated_yaw_deg 摇杆是角速度命令，积分为航向目标；松杆后保持最后目标航向。 */
static float yaw_target_deg = 0.0f;
static uint8_t yaw_target_initialized = 0u;

/* 从控传感器数据副本，由 slave_sensor_data.updated 触发刷新 */
int32_t  slave_flow_x, slave_flow_y;       /* 光流 X/Y 原始值 */
uint16_t slave_flow_distance_mm;              /* 光流测距 (mm) */
float    slave_flow_altitude_cm;               /* 光流测距高度 (cm) */
float    slave_baro_altitude_cm;               /* 气压高度 (cm) */
float    slave_mag_yaw_deg;                /* 磁力计航向 (0~360°) */


static int16_t clamp_int16(int32_t value, int16_t min_value, int16_t max_value)
{
	if(value < min_value) return min_value;
	if(value > max_value) return max_value;
	return (int16_t)value;
}

static float clamp_float(float value, float min_value, float max_value)
{
	if(value < min_value) return min_value;
	if(value > max_value) return max_value;
	return value;
}

static float wrap_yaw360(float angle)
{
	while(angle >= 360.0f) angle -= 360.0f;
	while(angle < 0.0f) angle += 360.0f;
	return angle;
}

static void flight_control_stop_navigation(void)
{
	nav_active = 0u;
	last_nav_frame_ms = 0u;
	last_nav_pid_ms = 0u;
	pid_reset_navigation();
}

/* 将手动杆量和导航修正合成一次目标，再交给原有姿态/PWM 链路。
 * 光流轴与机体 estimated_roll_deg/estimated_pitch_deg 的符号必须通过无桨台架核对。
 */
static void flight_control_apply_targets(void)
{
	float duty;
	float roll_target;
	float pitch_target;

	if(!flight_safety_motors_allowed(&flight_safety)) return;

	duty = (float)rc_throttle_percent;
	roll_target = (float)rc_roll / 10.0f;
	pitch_target = (float)rc_pitch / 10.0f;
	if(nav_active)
	{
		duty += pid_get_altitude_output();
		roll_target += pid_get_position_roll_target();
		pitch_target += pid_get_position_pitch_target();
	}

	pid_set_base_duty(clamp_float(duty, 0.0f, 100.0f));
	pid_set_roll_target(clamp_float(roll_target, -30.0f, 30.0f));
	pid_set_pitch_target(clamp_float(pitch_target, -30.0f, 30.0f));
}

/* 软件状态和硬件PWM同时归零，避免旧混控结果在下一周期重新输出。 */
static void flight_control_hold_safe(void)
{
	control_throttle_percent = 0;
	nav_requested = 0u;
	yaw_target_initialized = 0u;
	flight_control_stop_navigation();
	pid_set_base_duty(0.0f);
	pid_set_roll_target(0.0f);
	pid_set_pitch_target(0.0f);
	pid_set_yaw_target(0.0f);
	pid_stop_motors();

	motor_pwm_set_minimum_output();
}


void system_clock_configure(void)
{
	uint8_t retry = 0;

	/* 复位时钟配置 */
	RCC_DeInit();

	/* 启动外部高速晶振 HSE */
	RCC_HSEConfig(RCC_HSE_ON);

	/* 等待 HSE 稳定，超时后重新拉起，降低长按复位后的起振风险 */
	while (RCC_WaitForHSEStartUp() != SUCCESS)
	{
		retry++;
		if(retry > 10)
		{
			RCC_HSEConfig(RCC_HSE_OFF);
			Delay_ms(10);
			RCC_HSEConfig(RCC_HSE_ON);
			retry = 0;
		}
	}

	/* 72MHz 下必须配置 Flash 预取和等待周期 */
	FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
	FLASH_SetLatency(FLASH_Latency_2);

	/* 总线分频：AHB=72MHz，APB1=36MHz，APB2=72MHz */
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	RCC_PCLK1Config(RCC_HCLK_Div2);
	RCC_PCLK2Config(RCC_HCLK_Div1);

	/* PLL = 8MHz * 9 = 72MHz */
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
	RCC_PLLCmd(ENABLE);

	/* 等待 PLL 锁定 */
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

	/* 切换系统时钟到 PLL */
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	while (RCC_GetSYSCLKSource() != 0x08);
}

static void flight_control_run_imu_task(void)
{
	float roll_rate_value;
	float pitch_rate_value;
	float yaw_rate_value;

	attitude_estimator_update();
	attitude_estimator_get_rates(&roll_rate_value, &pitch_rate_value, &yaw_rate_value);

	roll_rate = roll_rate_value;
	pitch_rate = pitch_rate_value;
	yaw_rate = yaw_rate_value;
	pid_update_rate_loop(
		roll_rate_value, pitch_rate_value, yaw_rate_value);
	if(!flight_safety_motors_allowed(&flight_safety))
	{
		motor_pwm_set_minimum_output();
	}
	else
	{
		/* Publish all four results together; preload latches them at 50Hz.
		 * UDIS prevents an update event between the four CCR writes.
		 */
		TIM_UpdateDisableConfig(TIM4, ENABLE);
		motor_pwm_set_compare3(pid_get_motor_compare_front_left());
		motor_pwm_set_compare2(pid_get_motor_compare_front_right());
		motor_pwm_set_compare4(pid_get_motor_compare_back_right());
		motor_pwm_set_compare1(pid_get_motor_compare_back_left());
		TIM_UpdateDisableConfig(TIM4, DISABLE);
	}
}

static void flight_control_run_angle_task(void)
{
	float roll_value;
	float pitch_value;
	float yaw_value;

	attitude_estimator_get_angles(&roll_value, &pitch_value, &yaw_value);
	if(flight_safety_motors_allowed(&flight_safety))
	{
		if(yaw_target_initialized == 0u)
		{
			yaw_target_deg = yaw_value;
			yaw_target_initialized = 1u;
		}
		/* rc_yaw 为 ±900，对应 ±90°/s；本任务每 10ms 积分一次。 */
		if((rc_yaw > BOARD_RC_YAW_DEADBAND) ||
		   (rc_yaw < -BOARD_RC_YAW_DEADBAND))
		{
			yaw_target_deg = wrap_yaw360(
				yaw_target_deg + (float)rc_yaw * 0.001f);
		}
		pid_set_yaw_target(yaw_target_deg);
	}

	roll = roll_value;
	pitch = pitch_value;
	yaw = yaw_value;
	pid_update_angle_loop(roll_value, pitch_value, yaw_value);
}

static void telemetry_send_pitch(void)
{
	uint16_t index = 0u;
	int32_t angle_value = (int32_t)(pitch * 10.0f);

	if(angle_value > 9999) angle_value = 9999;
	if(angle_value < -9999) angle_value = -9999;

	telemetry_buffer[index++] = '[';
	telemetry_buffer[index++] = 'p';
	telemetry_buffer[index++] = 'l';
	telemetry_buffer[index++] = 'o';
	telemetry_buffer[index++] = 't';
	telemetry_buffer[index++] = ',';

	if(angle_value < 0)
	{
		telemetry_buffer[index++] = '-';
		angle_value = -angle_value;
	}

	if(angle_value >= 1000)
	{
		telemetry_buffer[index++] = (uint8_t)(angle_value / 1000 + '0');
		angle_value %= 1000;
	}
	if(angle_value >= 100)
	{
		telemetry_buffer[index++] = (uint8_t)(angle_value / 100 + '0');
		angle_value %= 100;
	}
	if(angle_value >= 10)
	{
		telemetry_buffer[index++] = (uint8_t)(angle_value / 10 + '0');
		angle_value %= 10;
	}
	telemetry_buffer[index++] = (uint8_t)(angle_value + '0');
	telemetry_buffer[index++] = ']';

	bluetooth_serial_send_dma(telemetry_buffer, index);
}

/* 100ms telemetry task, independent of the motor PWM timer. */
static void flight_control_run_telemetry_task(void)
{
    board_led1_on();
    telemetry_send_pitch();
    board_led1_off();
}

static void flight_control_handle_rc_frame(void)
{
	int32_t mapped_value;

	crsf_frame_received = 0u;
	rc_frames_received++;

	mapped_value =
		(int32_t)(rc_channels[BOARD_RC_CHANNEL_ROLL] - 1500) / 5;
	rc_roll = clamp_int16(mapped_value, -100, 100);
	mapped_value =
		(int32_t)(rc_channels[BOARD_RC_CHANNEL_PITCH] - 1500) / 5;
	rc_pitch = clamp_int16(mapped_value, -100, 100);
	mapped_value =
		(int32_t)(rc_channels[BOARD_RC_CHANNEL_THROTTLE] - 1000) / 10;
	rc_throttle_percent = (uint8_t)clamp_int16(mapped_value, 0, 100);
	mapped_value =
		(int32_t)(rc_channels[BOARD_RC_CHANNEL_YAW] - 1500) * 9 / 5;
	rc_yaw = clamp_int16(mapped_value, -900, 900);
	servo_status =
		(rc_channels[BOARD_RC_CHANNEL_SERVO] > 1500) ? 1u : 0u;
	mag_calibration_switch =
		(rc_channels[BOARD_RC_CHANNEL_MAG] > 1500) ? 1u : 0u;
	nav_requested =
		(rc_channels[BOARD_RC_CHANNEL_NAV_MODE] > 1500) ? 1u : 0u;

	flight_safety_on_valid_rc_frame(
		&flight_safety,
		system_time_ms,
		rc_throttle_percent,
		BOARD_RC_THROTTLE_UNLOCK_PERCENT);

	if(flight_safety_motors_allowed(&flight_safety))
	{
		control_throttle_percent = rc_throttle_percent;
		if((nav_requested == 0u) ||
		   (rc_throttle_percent < BOARD_NAV_MIN_THROTTLE_PERCENT))
		{
			flight_control_stop_navigation();
		}
		flight_control_apply_targets();
	}
	else
	{
		flight_control_hold_safe();
	}
}

static void flight_control_service_rc(void)
{
	crsf_process();
	if(crsf_frame_received)
	{
		flight_control_handle_rc_frame();
	}
}

static void flight_control_check_failsafe(void)
{
	if(flight_safety_check_timeout(
			&flight_safety,
			system_time_ms,
			BOARD_RC_FAILSAFE_TIMEOUT_MS))
	{
		servo_status = 0u;
		mag_calibration_switch = 0u;
		flight_control_hold_safe();
	}
}

static void flight_control_check_navigation_timeout(void)
{
	/* 主从链路断开后，旧 PID 输出最多保留 200ms，然后回到手动目标。 */
	if(nav_active &&
	   (uint32_t)(system_time_ms - last_nav_frame_ms) >=
		BOARD_NAV_SENSOR_TIMEOUT_MS)
	{
		flight_control_stop_navigation();
		flight_control_apply_targets();
	}
}

static void flight_control_update_indicators(void)
{
	if(servo_status)
	{
		board_led2_on();
	}
	else
	{
		board_led2_off();
	}

	if(mag_calibration_switch)
	{
		board_led3_on();
	}
	else
	{
		board_led3_off();
	}
}

static void flight_control_refresh_slave_data(void)
{
	int32_t flow_x_snapshot;
	int32_t flow_y_snapshot;
	uint16_t flow_dist_snapshot;
	float flow_alt_snapshot;
	float baro_alt_snapshot;
	float mag_yaw_snapshot;
	uint8_t flow_quality_snapshot;
	uint16_t status_flags_snapshot;
	uint32_t now_ms;
	uint32_t elapsed_ms;
	uint8_t sensors_valid;

	if(!slave_sensor_data.updated)
	{
		return;
	}

	/* slave_link_process publishes data in this same main-loop context. */
	slave_sensor_data.updated = 0u;
	flow_x_snapshot = slave_sensor_data.flow_x;
	flow_y_snapshot = slave_sensor_data.flow_y;
	flow_dist_snapshot = slave_sensor_data.flow_distance;
	flow_alt_snapshot = slave_sensor_data.flow_altitude;
	baro_alt_snapshot = slave_sensor_data.baro_altitude;
	mag_yaw_snapshot = slave_sensor_data.mag_yaw;
	flow_quality_snapshot = slave_sensor_data.flow_quality;
	status_flags_snapshot = slave_sensor_data.status_flags;

	slave_flow_x = flow_x_snapshot;
	slave_flow_y = flow_y_snapshot;
	slave_flow_distance_mm = flow_dist_snapshot;
	slave_flow_altitude_cm = flow_alt_snapshot;
	slave_baro_altitude_cm = baro_alt_snapshot;
	slave_mag_yaw_deg = mag_yaw_snapshot;

	/* 只有从控标记有效且不在校准时，才用新磁力计帧修正 estimated_yaw_deg。 */
	if((status_flags_snapshot & INTER_MCU_SENSOR_FLAG_MAG_VALID) &&
	   !(status_flags_snapshot & INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING))
	{
		if(attitude_estimator_apply_mag_heading(mag_yaw_snapshot) &&
		   flight_safety_motors_allowed(&flight_safety))
		{
			float current_roll;
			float current_pitch;
			float current_yaw;
			/* 首帧绝对航向可能与陀螺仪积分零点不同；同步目标防止突跳。 */
			attitude_estimator_get_angles(&current_roll, &current_pitch, &current_yaw);
			yaw_target_deg = current_yaw;
			yaw_target_initialized = 1u;
			pid_set_yaw_target(yaw_target_deg);
		}
	}

	/* 光流提供相对运动原始量，气压计提供相对高度；两者均有效才进入
	 * CH6 辅助模式。有效位来自从控，测距和信号质量再加一道门槛。
	 */
	sensors_valid =
		((status_flags_snapshot & INTER_MCU_SENSOR_FLAG_BARO_VALID) != 0u) &&
		((status_flags_snapshot & INTER_MCU_SENSOR_FLAG_FLOW_VALID) != 0u) &&
		(flow_quality_snapshot >= BOARD_NAV_FLOW_QUALITY_MIN) &&
		(flow_dist_snapshot >= BOARD_NAV_DISTANCE_MIN_MM) &&
		(flow_dist_snapshot <= BOARD_NAV_DISTANCE_MAX_MM);
	if(!flight_safety_motors_allowed(&flight_safety) ||
	   !nav_requested ||
	   (rc_throttle_percent < BOARD_NAV_MIN_THROTTLE_PERCENT) ||
	   !sensors_valid)
	{
		flight_control_stop_navigation();
		flight_control_apply_targets();
		return;
	}

	now_ms = system_time_ms;
	if(nav_active == 0u)
	{
		/* 接入瞬间捕获当前高度和光流积分量，不让旧设定值造成阶跃。
		 * 光流原始积分量不是米制位置，比例/方向仍需实机标定。
		 */
		pid_set_altitude_target(baro_alt_snapshot);
		pid_set_position_target(flow_x_snapshot, flow_y_snapshot);
		pid_reset_navigation();
		nav_active = 1u;
		last_nav_pid_ms = now_ms;
	}
	else
	{
		elapsed_ms = (uint32_t)(now_ms - last_nav_pid_ms);
		if((elapsed_ms == 0u) ||
		   (elapsed_ms >= BOARD_NAV_SENSOR_TIMEOUT_MS))
		{
			flight_control_stop_navigation();
			flight_control_apply_targets();
			return;
		}
		pid_update_navigation(
			baro_alt_snapshot, flow_x_snapshot, flow_y_snapshot,
			(float)elapsed_ms * 0.001f);
		last_nav_pid_ms = now_ms;
	}
	last_nav_frame_ms = now_ms;
	flight_control_apply_targets();
}

static void flight_control_update_pid_tuning(void)
{
	uint32_t pid_update_mask;

	if(!bluetooth_serial_process()) return;
	pid_update_mask = bluetooth_serial_parse_parameters();

	if(pid_update_mask & PID_PARAM_UPDATE_PKP) pid_set_pitch_kp(bluetooth_serial_get_pitch_kp());
	if(pid_update_mask & PID_PARAM_UPDATE_PKI) pid_set_pitch_ki(bluetooth_serial_get_pitch_ki());
	if(pid_update_mask & PID_PARAM_UPDATE_PKD) pid_set_pitch_kd(bluetooth_serial_get_pitch_kd() * 0.01f);
	if(pid_update_mask & PID_PARAM_UPDATE_RKP) pid_set_roll_kp(bluetooth_serial_get_roll_kp());
	if(pid_update_mask & PID_PARAM_UPDATE_RKI) pid_set_roll_ki(bluetooth_serial_get_roll_ki());
	if(pid_update_mask & PID_PARAM_UPDATE_RKD) pid_set_roll_kd(bluetooth_serial_get_roll_kd() * 0.01f);
	if(pid_update_mask & PID_PARAM_UPDATE_YKP) pid_set_yaw_kp(bluetooth_serial_get_yaw_kp());
	if(pid_update_mask & PID_PARAM_UPDATE_YKI) pid_set_yaw_ki(bluetooth_serial_get_yaw_ki());
	if(pid_update_mask & PID_PARAM_UPDATE_YKD) pid_set_yaw_kd(bluetooth_serial_get_yaw_kd());
}


int main(void)
{
	Delay_us(500);   /* 给电源和传感器预留稳定时间 */
	system_clock_configure();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	app_scheduler_init();
	flight_safety_init(&flight_safety);
	board_led_init();
	mpu6050_init();
	kalman_init_roll();
	kalman_init_pitch();
	bluetooth_serial_init();
	watchdog_init();
	crsf_init();
	slave_link_init();
	motor_pwm_init();
	flight_control_hold_safe();
	board_control_timers_init();

	while (1)
	{
		IWDG_ReloadCounter();

		if(app_scheduler_take(APP_TASK_RC_SERVICE))
		{
			flight_control_service_rc();
		}
		flight_control_check_failsafe();
		slave_link_process();
		flight_control_refresh_slave_data();
		flight_control_check_navigation_timeout();

		if(app_scheduler_take(APP_TASK_IMU_UPDATE))
		{
			flight_control_run_imu_task();
		}

		if(app_scheduler_take(APP_TASK_ANGLE_CONTROL))
		{
			flight_control_run_angle_task();
		}

		if(app_scheduler_take(APP_TASK_TELEMETRY))
		{
			flight_control_run_telemetry_task();
		}
		flight_control_update_indicators();
		flight_control_update_pid_tuning();
	}
}

/* 1ms ISR: timestamp accounting and task notification only. */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        uint32_t elapsed_ms;
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        elapsed_ms = millisecond_clock_elapsed_ms();
        system_time_ms += elapsed_ms;
        app_scheduler_tick_from_isr(elapsed_ms);
    }
}
