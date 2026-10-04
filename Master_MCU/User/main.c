#include "stm32f10x.h"
#include "Delay.h"
#include "MPU6050.h"
#include "hubu.h"
#include "PWM4.h"
#include "Pid.h"
#include "LED.h"
#include "BlueSerial.h"
#include "Kalman.h"     
#include "IWDG.h"       
#include "crsf.h"
#include "SlaveMCU.h"
#include "app_scheduler.h"
#include "flight_safety.h"
#include "board_config.h"
#include "control_timers.h"
#include "inter_mcu_protocol.h"

/* Application state, owned by the main loop unless marked volatile. */
volatile float roll,pitch,yaw;
volatile float rollRate,pitchRate,yawRate;
uint8_t Contrl;

uint8_t ReceiveSuccessCount;

volatile uint32_t system_tick_5ms = 0;   /* 单调时基，允许自然回绕 */

/* 显式飞行安全状态：启动锁、运行、失联、恢复锁。 */
static FlightSafetyContext flight_safety;

/* 蓝牙调试发送 */
uint8_t send_div_cnt = 0;
#define SEND_DIV_NUM 5   
uint8_t send_buff[32];

/* 遥控通道映射结果 */
uint8_t servo_status = 0;   /* CH4 开关: 0=关 1=开 */
uint8_t MAG_intf     = 0;   /* CH5 开关: 0=关 1=开 */
int16_t rc_roll  = 0;       /* CH0 映射 ±100 */
int16_t rc_pitch = 0;       /* CH1 映射 ±100 */
uint8_t rc_thr   = 0;       /* CH2 映射 0~100 */
int16_t rc_yaw   = 0;       /* CH3 映射 ±900 */

/* CH6 辅助模式：只有开关、油门、安全状态和有效新帧均满足才接入导航 PID。
 * 默认关闭；第一次进入时捕获当前气压高度，退出时清除导航积分。
 */
static uint8_t nav_requested = 0u;
static uint8_t nav_active = 0u;
static uint32_t last_nav_frame_tick = 0u;
static uint32_t last_nav_pid_tick = 0u;

/* Yaw 摇杆是角速度命令，积分为航向目标；松杆后保持最后目标航向。 */
static float yaw_target_deg = 0.0f;
static uint8_t yaw_target_initialized = 0u;

/* 从控传感器数据副本，由 slave.updated 触发刷新 */
int32_t  s_flow_x, s_flow_y;       /* 光流 X/Y 原始值 */
uint16_t s_flow_dist;              /* 光流测距 (mm) */
float    s_flow_alt;               /* 光流测距高度 (cm) */
float    s_baro_alt;               /* 气压高度 (cm) */
float    s_mag_yaw;                /* 磁力计航向 (0~360°) */


static int16_t Clamp_Int16(int32_t value, int16_t min_value, int16_t max_value)
{
	if(value < min_value) return min_value;
	if(value > max_value) return max_value;
	return (int16_t)value;
}

static float Clamp_Float(float value, float min_value, float max_value)
{
	if(value < min_value) return min_value;
	if(value > max_value) return max_value;
	return value;
}

static float Wrap_Yaw360(float angle)
{
	while(angle >= 360.0f) angle -= 360.0f;
	while(angle < 0.0f) angle += 360.0f;
	return angle;
}

static void FlightControl_StopNavigation(void)
{
	nav_active = 0u;
	last_nav_frame_tick = 0u;
	last_nav_pid_tick = 0u;
	Drone_Navigation_PID_Reset();
}

/* 将手动杆量和导航修正合成一次目标，再交给原有姿态/PWM 链路。
 * 光流轴与机体 Roll/Pitch 的符号必须通过无桨台架核对。
 */
static void FlightControl_ApplyTargets(void)
{
	float duty;
	float roll_target;
	float pitch_target;

	if(!FlightSafety_MotorsAllowed(&flight_safety)) return;

	duty = (float)rc_thr;
	roll_target = (float)rc_roll / 10.0f;
	pitch_target = (float)rc_pitch / 10.0f;
	if(nav_active)
	{
		duty += Altitude_pid_Get();
		roll_target += Position_roll_aim_Get();
		pitch_target += Position_pitch_aim_Get();
	}

	Set_Base_Duty(Clamp_Float(duty, 0.0f, 100.0f));
	Roll_aim_Get(Clamp_Float(roll_target, -30.0f, 30.0f));
	Pitch_aim_Get(Clamp_Float(pitch_target, -30.0f, 30.0f));
}

/* 软件状态和硬件PWM同时归零，避免旧混控结果在下一周期重新输出。 */
static void FlightControl_HoldSafe(void)
{
	Contrl = 0;
	nav_requested = 0u;
	yaw_target_initialized = 0u;
	FlightControl_StopNavigation();
	Set_Base_Duty(0.0f);
	Roll_aim_Get(0.0f);
	Pitch_aim_Get(0.0f);
	Yaw_aim_Get(0.0f);
	Drone_Motors_Stop();

	PWM4_SetMinimumOutput();
}


void SystemClock_Config(void)
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

static void FlightControl_RunImuTask(void)
{
	float roll_rate_value;
	float pitch_rate_value;
	float yaw_rate_value;

	CompFilter_Simple();
	Get_Gyro(&roll_rate_value, &pitch_rate_value, &yaw_rate_value);

	rollRate = roll_rate_value;
	pitchRate = pitch_rate_value;
	yawRate = yaw_rate_value;
	Drone_Inner_Rate_PID_Control(
		roll_rate_value, pitch_rate_value, yaw_rate_value);
	if(!FlightSafety_MotorsAllowed(&flight_safety))
	{
		PWM4_SetMinimumOutput();
	}
	else
	{
		/* Publish all four results together; preload latches them at 50Hz.
		 * UDIS prevents an update event between the four CCR writes.
		 */
		TIM_UpdateDisableConfig(TIM4, ENABLE);
		PWM4_SetCompare3(Get_Motor_Duty_FrontLeft());
		PWM4_SetCompare2(Get_Motor_Duty_FrontRight());
		PWM4_SetCompare4(Get_Motor_Duty_BackRight());
		PWM4_SetCompare1(Get_Motor_Duty_BackLeft());
		TIM_UpdateDisableConfig(TIM4, DISABLE);
	}
}

static void FlightControl_RunAngleTask(void)
{
	float roll_value;
	float pitch_value;
	float yaw_value;

	Get_Angle(&roll_value, &pitch_value, &yaw_value);
	if(FlightSafety_MotorsAllowed(&flight_safety))
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
			yaw_target_deg = Wrap_Yaw360(
				yaw_target_deg + (float)rc_yaw * 0.001f);
		}
		Yaw_aim_Get(yaw_target_deg);
	}

	roll = roll_value;
	pitch = pitch_value;
	yaw = yaw_value;
	Drone_Outer_Angle_PID_Control(roll_value, pitch_value, yaw_value);
}

static void Telemetry_SendPitch(void)
{
	uint16_t index = 0u;
	int32_t angle_value = (int32_t)(pitch * 10.0f);

	if(angle_value > 9999) angle_value = 9999;
	if(angle_value < -9999) angle_value = -9999;

	send_buff[index++] = '[';
	send_buff[index++] = 'p';
	send_buff[index++] = 'l';
	send_buff[index++] = 'o';
	send_buff[index++] = 't';
	send_buff[index++] = ',';

	if(angle_value < 0)
	{
		send_buff[index++] = '-';
		angle_value = -angle_value;
	}

	if(angle_value >= 1000)
	{
		send_buff[index++] = (uint8_t)(angle_value / 1000 + '0');
		angle_value %= 1000;
	}
	if(angle_value >= 100)
	{
		send_buff[index++] = (uint8_t)(angle_value / 100 + '0');
		angle_value %= 100;
	}
	if(angle_value >= 10)
	{
		send_buff[index++] = (uint8_t)(angle_value / 10 + '0');
		angle_value %= 10;
	}
	send_buff[index++] = (uint8_t)(angle_value + '0');
	send_buff[index++] = ']';

	BlueSerial_SendBuff(send_buff, index);
}

/* TIM4 still publishes the 20ms telemetry task; motor CCRs are written
 * by the 2ms IMU/inner-loop task. */
static void FlightControl_RunTelemetryTask(void)
{
	LED1_ON();

	send_div_cnt++;
	if(send_div_cnt >= SEND_DIV_NUM)
	{
		send_div_cnt = 0u;
		Telemetry_SendPitch();
	}

	LED1_OFF();
}

static void FlightControl_HandleRcFrame(void)
{
	int32_t mapped_value;

	crsf_frame_received = 0u;
	ReceiveSuccessCount++;

	mapped_value =
		(int32_t)(rcChannels[BOARD_RC_CHANNEL_ROLL] - 1500) / 5;
	rc_roll = Clamp_Int16(mapped_value, -100, 100);
	mapped_value =
		(int32_t)(rcChannels[BOARD_RC_CHANNEL_PITCH] - 1500) / 5;
	rc_pitch = Clamp_Int16(mapped_value, -100, 100);
	mapped_value =
		(int32_t)(rcChannels[BOARD_RC_CHANNEL_THROTTLE] - 1000) / 10;
	rc_thr = (uint8_t)Clamp_Int16(mapped_value, 0, 100);
	mapped_value =
		(int32_t)(rcChannels[BOARD_RC_CHANNEL_YAW] - 1500) * 9 / 5;
	rc_yaw = Clamp_Int16(mapped_value, -900, 900);
	servo_status =
		(rcChannels[BOARD_RC_CHANNEL_SERVO] > 1500) ? 1u : 0u;
	MAG_intf =
		(rcChannels[BOARD_RC_CHANNEL_MAG] > 1500) ? 1u : 0u;
	nav_requested =
		(rcChannels[BOARD_RC_CHANNEL_NAV_MODE] > 1500) ? 1u : 0u;

	FlightSafety_OnValidRcFrame(
		&flight_safety,
		system_tick_5ms,
		rc_thr,
		BOARD_RC_THROTTLE_UNLOCK_PERCENT);

	if(FlightSafety_MotorsAllowed(&flight_safety))
	{
		Contrl = rc_thr;
		if((nav_requested == 0u) ||
		   (rc_thr < BOARD_NAV_MIN_THROTTLE_PERCENT))
		{
			FlightControl_StopNavigation();
		}
		FlightControl_ApplyTargets();
	}
	else
	{
		FlightControl_HoldSafe();
	}
}

static void FlightControl_ServiceRc(void)
{
	CRSF_Process();
	if(crsf_frame_received)
	{
		FlightControl_HandleRcFrame();
	}
}

static void FlightControl_CheckFailsafe(void)
{
	if(FlightSafety_CheckTimeout(
			&flight_safety,
			system_tick_5ms,
			BOARD_RC_FAILSAFE_TIMEOUT_TICKS))
	{
		servo_status = 0u;
		MAG_intf = 0u;
		FlightControl_HoldSafe();
	}
}

static void FlightControl_CheckNavigationTimeout(void)
{
	/* 主从链路断开后，旧 PID 输出最多保留 200ms，然后回到手动目标。 */
	if(nav_active &&
	   (uint32_t)(system_tick_5ms - last_nav_frame_tick) >=
		BOARD_NAV_SENSOR_TIMEOUT_TICKS)
	{
		FlightControl_StopNavigation();
		FlightControl_ApplyTargets();
	}
}

static void FlightControl_UpdateIndicators(void)
{
	if(servo_status)
	{
		LED2_ON();
	}
	else
	{
		LED2_OFF();
	}

	if(MAG_intf)
	{
		LED3_ON();
	}
	else
	{
		LED3_OFF();
	}
}

static void FlightControl_RefreshSlaveData(void)
{
	int32_t flow_x_snapshot;
	int32_t flow_y_snapshot;
	uint16_t flow_dist_snapshot;
	float flow_alt_snapshot;
	float baro_alt_snapshot;
	float mag_yaw_snapshot;
	uint8_t flow_quality_snapshot;
	uint16_t status_flags_snapshot;
	uint32_t now_tick;
	uint32_t elapsed_ticks;
	uint8_t sensors_valid;

	if(!slave.updated)
	{
		return;
	}

	/* SlaveMCU_Process publishes data in this same main-loop context. */
	slave.updated = 0u;
	flow_x_snapshot = slave.flow_x;
	flow_y_snapshot = slave.flow_y;
	flow_dist_snapshot = slave.flow_distance;
	flow_alt_snapshot = slave.flow_altitude;
	baro_alt_snapshot = slave.baro_altitude;
	mag_yaw_snapshot = slave.mag_yaw;
	flow_quality_snapshot = slave.flow_quality;
	status_flags_snapshot = slave.status_flags;

	s_flow_x = flow_x_snapshot;
	s_flow_y = flow_y_snapshot;
	s_flow_dist = flow_dist_snapshot;
	s_flow_alt = flow_alt_snapshot;
	s_baro_alt = baro_alt_snapshot;
	s_mag_yaw = mag_yaw_snapshot;

	/* 只有从控标记有效且不在校准时，才用新磁力计帧修正 Yaw。 */
	if((status_flags_snapshot & INTER_MCU_SENSOR_FLAG_MAG_VALID) &&
	   !(status_flags_snapshot & INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING))
	{
		if(Yaw_ApplyMagHeading(mag_yaw_snapshot) &&
		   FlightSafety_MotorsAllowed(&flight_safety))
		{
			float current_roll;
			float current_pitch;
			float current_yaw;
			/* 首帧绝对航向可能与陀螺仪积分零点不同；同步目标防止突跳。 */
			Get_Angle(&current_roll, &current_pitch, &current_yaw);
			yaw_target_deg = current_yaw;
			yaw_target_initialized = 1u;
			Yaw_aim_Get(yaw_target_deg);
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
	if(!FlightSafety_MotorsAllowed(&flight_safety) ||
	   !nav_requested ||
	   (rc_thr < BOARD_NAV_MIN_THROTTLE_PERCENT) ||
	   !sensors_valid)
	{
		FlightControl_StopNavigation();
		FlightControl_ApplyTargets();
		return;
	}

	now_tick = system_tick_5ms;
	if(nav_active == 0u)
	{
		/* 接入瞬间捕获当前高度和光流积分量，不让旧设定值造成阶跃。
		 * 光流原始积分量不是米制位置，比例/方向仍需实机标定。
		 */
		Altitude_aim_Get(baro_alt_snapshot);
		Position_aim_Get(flow_x_snapshot, flow_y_snapshot);
		Drone_Navigation_PID_Reset();
		nav_active = 1u;
		last_nav_pid_tick = now_tick;
	}
	else
	{
		elapsed_ticks = (uint32_t)(now_tick - last_nav_pid_tick);
		if((elapsed_ticks == 0u) ||
		   (elapsed_ticks >= BOARD_NAV_SENSOR_TIMEOUT_TICKS))
		{
			FlightControl_StopNavigation();
			FlightControl_ApplyTargets();
			return;
		}
		Drone_Altitude_Position_PID_Control(
			baro_alt_snapshot, flow_x_snapshot, flow_y_snapshot,
			(float)elapsed_ticks * 0.005f);
		last_nav_pid_tick = now_tick;
	}
	last_nav_frame_tick = now_tick;
	FlightControl_ApplyTargets();
}

static void FlightControl_UpdatePidTuning(void)
{
	uint32_t pid_update_mask;

	if(!BlueSerial_Process()) return;
	pid_update_mask = PID_Param_Parse();

	if(pid_update_mask & PID_PARAM_UPDATE_PKP) Pitch_Kp_Get(Pitch_Back_Kp());
	if(pid_update_mask & PID_PARAM_UPDATE_PKI) Pitch_Ki_Get(Pitch_Back_Ki());
	if(pid_update_mask & PID_PARAM_UPDATE_PKD) Pitch_Kd_Get(Pitch_Back_Kd() * 0.01f);
	if(pid_update_mask & PID_PARAM_UPDATE_RKP) Roll_Kp_Get(Roll_Back_Kp());
	if(pid_update_mask & PID_PARAM_UPDATE_RKI) Roll_Ki_Get(Roll_Back_Ki());
	if(pid_update_mask & PID_PARAM_UPDATE_RKD) Roll_Kd_Get(Roll_Back_Kd() * 0.01f);
	if(pid_update_mask & PID_PARAM_UPDATE_YKP) Yaw_Kp_Get(Yaw_Back_Kp());
	if(pid_update_mask & PID_PARAM_UPDATE_YKI) Yaw_Ki_Get(Yaw_Back_Ki());
	if(pid_update_mask & PID_PARAM_UPDATE_YKD) Yaw_Kd_Get(Yaw_Back_Kd());
}


int main(void)
{
	Delay_us(500);   /* 给电源和传感器预留稳定时间 */
	SystemClock_Config();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	AppScheduler_Init();
	FlightSafety_Init(&flight_safety);
	LED_Init();	
	MPU6050_Init();		
	Kalman_Roll_Init();
	Kalman_Pitch_Init();
	BlueSerial_Init();
	IWDG_Init();
	CRSF_Init();
	SlaveMCU_Init();
	PWM4_Init();
	FlightControl_HoldSafe();
	BoardControlTimers_Init();

	while (1)
	{
		IWDG_ReloadCounter();

		if(AppScheduler_Take(APP_TASK_RC_SERVICE))
		{
			FlightControl_ServiceRc();
		}
		FlightControl_CheckFailsafe();
		SlaveMCU_Process();
		FlightControl_RefreshSlaveData();
		FlightControl_CheckNavigationTimeout();

		if(AppScheduler_Take(APP_TASK_IMU_UPDATE))
		{
			FlightControl_RunImuTask();
		}

		if(AppScheduler_Take(APP_TASK_ANGLE_CONTROL))
		{
			FlightControl_RunAngleTask();
		}

		if(AppScheduler_Take(APP_TASK_TELEMETRY))
		{
			FlightControl_RunTelemetryTask();
		}
		FlightControl_UpdateIndicators();
		FlightControl_UpdatePidTuning();
	}
}

/* TIM2：2ms 只发布 IMU/内环任务，浮点计算在主循环执行。 */
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		AppScheduler_NotifyFromIsr(APP_TASK_IMU_UPDATE);
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}

/* TIM3：10ms 只发布角度外环任务。 */
void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		AppScheduler_NotifyFromIsr(APP_TASK_ANGLE_CONTROL);
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	}
}

/* TIM1：5ms 更新时间基并发布 CRSF 服务任务。 */
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
	{
		system_tick_5ms++;
		AppScheduler_NotifyFromIsr(APP_TASK_RC_SERVICE);
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}

/* TIM4：20ms 发布遥测服务任务，CCR 在 2ms 内环任务中更新。 */
void TIM4_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
	{
		AppScheduler_NotifyFromIsr(APP_TASK_TELEMETRY);
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
