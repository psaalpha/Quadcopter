#include "stm32f10x.h"
#include "Delay.h"
#include "MPU6050.h"
#include "hubu.h"
#include "PWM1.h"
#include "Timer1.h"
#include "PWM3.h"
#include "PWM4.h"
#include "QMC5883P.h"
#include "Pid.h"
#include "NRF24L01.h"
#include "LED.h"
#include "BlueSerial.h"
#include "Kalman.h"     
#include "IWDG.h"       
#include "crsf.h"
#include "SlaveMCU.h"
#include "app_scheduler.h"
#include "flight_safety.h"
#include "board_config.h"

/* 中断和主循环共享的数据 */
uint8_t  C=0;
int16_t AX, AY, AZ, GX, GY, GZ;
volatile float roll,pitch,yaw;
volatile float rollRate,pitchRate,yawRate;
float temp;
long press;
float absAlt, relAlt;
extern float HMC5883L_Yaw;
uint8_t Contrl;

uint8_t SendFlag;								/* 发送标志 */
uint8_t SendSuccessCount, SendFailedCount;		/* 发送成功/失败计数 */
uint8_t ReceiveFlag;							/* 接收标志 */
uint8_t ReceiveSuccessCount, ReceiveFailedCount;/* 接收成功/失败计数 */

float p0,d0;
extern volatile uint8_t NRF24L01_RxIrqFlag;

volatile uint32_t system_tick_5ms = 0;   /* 单调时基，允许自然回绕 */

/* 显式飞行安全状态：启动锁、运行、失联、恢复锁。 */
static FlightSafetyContext flight_safety;

/* 主控侧 QMC5883P 变量：当前主运行路径中磁力计数据来自从控 */
uint8_t qmc_init_ok = 0;        /* QMC 初始化状态：0=失败，1=成功 */
uint8_t qmc_calibrated = 0;     /* QMC 校准状态：0=未校准，1=已校准 */
uint8_t yaw_update_flag = 0; 
float qmc_yaw = 0.0f;           /* QMC 航向角，独立于 MPU yaw */


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

/* 从控传感器数据副本，由 slave.updated 触发刷新 */
int32_t  s_flow_x, s_flow_y;       /* 光流 X/Y 原始值 */
uint16_t s_flow_dist;              /* 光流测距 (mm) */
float    s_flow_alt;               /* 光流测距高度 (cm) */
float    s_baro_alt;               /* 气压高度 (cm) */
float    s_mag_yaw;                /* 磁力计航向 (0~360°) */

char buf[32];

static int16_t Clamp_Int16(int32_t value, int16_t min_value, int16_t max_value)
{
	if(value < min_value) return min_value;
	if(value > max_value) return max_value;
	return (int16_t)value;
}

/* 软件状态和硬件PWM同时归零，避免旧混控结果在下一周期重新输出。 */
static void FlightControl_HoldSafe(void)
{
	Contrl = 0;
	Set_Base_Duty(0.0f);
	Roll_aim_Get(0.0f);
	Pitch_aim_Get(0.0f);
	Yaw_aim_Get(0.0f);
	Drone_Motors_Stop();

	PWM4_SetCompare1(BOARD_MOTOR_PWM_MIN_COMPARE);
	PWM4_SetCompare2(BOARD_MOTOR_PWM_MIN_COMPARE);
	PWM4_SetCompare3(BOARD_MOTOR_PWM_MIN_COMPARE);
	PWM4_SetCompare4(BOARD_MOTOR_PWM_MIN_COMPARE);
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
}

static void FlightControl_RunAngleTask(void)
{
	float roll_value;
	float pitch_value;
	float yaw_value;

	Get_Angle(&roll_value, &pitch_value, &yaw_value);

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

static void FlightControl_RunMotorTask(void)
{
	LED1_ON();

	send_div_cnt++;
	if(send_div_cnt >= SEND_DIV_NUM)
	{
		send_div_cnt = 0u;
		Telemetry_SendPitch();
	}

	if(!FlightSafety_MotorsAllowed(&flight_safety))
	{
		PWM4_SetCompare1(BOARD_MOTOR_PWM_MIN_COMPARE);
		PWM4_SetCompare2(BOARD_MOTOR_PWM_MIN_COMPARE);
		PWM4_SetCompare3(BOARD_MOTOR_PWM_MIN_COMPARE);
		PWM4_SetCompare4(BOARD_MOTOR_PWM_MIN_COMPARE);
	}
	else
	{
		PWM4_SetCompare3(Get_Motor_Duty_FrontLeft());
		PWM4_SetCompare2(Get_Motor_Duty_FrontRight());
		PWM4_SetCompare4(Get_Motor_Duty_BackRight());
		PWM4_SetCompare1(Get_Motor_Duty_BackLeft());
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

	FlightSafety_OnValidRcFrame(
		&flight_safety,
		system_tick_5ms,
		rc_thr,
		BOARD_RC_THROTTLE_UNLOCK_PERCENT);

	if(FlightSafety_MotorsAllowed(&flight_safety))
	{
		Contrl = rc_thr;
		Set_Base_Duty(Contrl);
		Pitch_aim_Get(rc_pitch / 10.0f);
		Roll_aim_Get(rc_roll / 10.0f);
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

	if(!slave.updated)
	{
		return;
	}

	__disable_irq();
	slave.updated = 0u;
	flow_x_snapshot = slave.flow_x;
	flow_y_snapshot = slave.flow_y;
	flow_dist_snapshot = slave.flow_distance;
	flow_alt_snapshot = slave.flow_altitude;
	baro_alt_snapshot = slave.baro_altitude;
	mag_yaw_snapshot = slave.mag_yaw;
	__enable_irq();

	s_flow_x = flow_x_snapshot;
	s_flow_y = flow_y_snapshot;
	s_flow_dist = flow_dist_snapshot;
	s_flow_alt = flow_alt_snapshot;
	s_baro_alt = baro_alt_snapshot;
	s_mag_yaw = mag_yaw_snapshot;

	Drone_Altitude_Position_PID_Control(
		s_flow_alt, s_flow_x, s_flow_y);
}

static void FlightControl_UpdatePidTuning(void)
{
	uint32_t pid_update_mask = PID_Param_Parse();

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
	PWM1_Init();
	PWM3_Init();
	PWM4_Init();
	TIM1_Init_1S_IRQ();
	LED_Init();	
	MPU6050_Init();		
	Kalman_Roll_Init();
	Kalman_Pitch_Init();
	BlueSerial_Init();
	IWDG_Init();
	CRSF_Init();
	SlaveMCU_Init();
	/* 丢弃外设初始化期间累积的周期任务，从实时边界开始调度。 */
	AppScheduler_Init();
	FlightControl_HoldSafe();

	while (1)
	{
		IWDG_ReloadCounter();

		if(AppScheduler_Take(APP_TASK_RC_SERVICE))
		{
			FlightControl_ServiceRc();
		}
		FlightControl_CheckFailsafe();

		if(AppScheduler_Take(APP_TASK_IMU_UPDATE))
		{
			FlightControl_RunImuTask();
		}

		if(AppScheduler_Take(APP_TASK_ANGLE_CONTROL))
		{
			FlightControl_RunAngleTask();
		}

		if(AppScheduler_Take(APP_TASK_MOTOR_OUTPUT))
		{
			FlightControl_RunMotorTask();
		}
		FlightControl_UpdateIndicators();
		FlightControl_RefreshSlaveData();
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

/* TIM4：20ms 只发布电机输出任务。 */
void TIM4_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
	{
		AppScheduler_NotifyFromIsr(APP_TASK_MOTOR_OUTPUT);
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
