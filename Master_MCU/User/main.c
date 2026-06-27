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

/* 中断和主循环共享的数据 */
uint8_t  C=0;
int16_t AX, AY, AZ, GX, GY, GZ;
float roll,pitch,yaw;
float rollRate,pitchRate,yawRate;
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
volatile uint8_t Receive_loss;    /* 遥控失联计数 */
uint16_t down_cnt = 0;            /* 降油门周期计数 */
#define DOWN_PERIOD 4             /* 越大降油门越慢 */

/* 周期任务标志 */
volatile uint8_t angle_update_flag = 0;
volatile uint8_t angle_rate_update_flag=0;
volatile uint8_t crsf_tick = 0;         /* TIM1 触发 CRSF 解析 */
uint8_t PWM_Flag=0;						/* PWM 更新标志 */

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
float    s_baro_alt;               /* 气压高度 (cm) */
float    s_mag_yaw;                /* 磁力计航向 (0~360°) */

char buf[32];


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


int main(void)
{
	Delay_us(500);   /* 给电源和传感器预留稳定时间 */
	SystemClock_Config();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
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

	while (1)
	{
		IWDG_ReloadCounter();
		if(angle_update_flag == 1)
		{
			Drone_Outer_Angle_PID_Control(roll,pitch,yaw);
			angle_update_flag = 0; 
			Receive_loss++;
		}
		else
		{
//			LED1_OFF();
		}
		if(angle_rate_update_flag == 1)
		{
			Drone_Inner_Rate_PID_Control(rollRate,pitchRate,yawRate);
			angle_rate_update_flag = 0;
		}
		if(PWM_Flag==1)
		{			
LED1_ON();
						send_div_cnt++;
						if(send_div_cnt >= SEND_DIV_NUM)
						{
							send_div_cnt = 0;
							uint16_t idx = 0;
							int16_t ang_val;
						
							/* 拼接蓝牙绘图帧头：[plot, */
							send_buff[idx++] = '[';
							send_buff[idx++] = 'p';
							send_buff[idx++] = 'l';
							send_buff[idx++] = 'o';
							send_buff[idx++] = 't';
							send_buff[idx++] = ',';
						
							/* pitch 放大 10 倍后转为整数发送 */
							ang_val = (int16_t)(pitch * 10.0f);
						
							/* 处理符号位 */
							if(ang_val < 0)
							{
								send_buff[idx++] = '-';
								ang_val = (uint16_t)(-ang_val);
							}
						
							/* 逐位拆分数字 */
							if(ang_val >= 1000) send_buff[idx++] = ang_val / 1000 + '0'; ang_val %= 1000;
							if(ang_val >= 100)  send_buff[idx++] = ang_val / 100  + '0'; ang_val %= 100;
							if(ang_val >= 10)   send_buff[idx++] = ang_val / 10   + '0'; ang_val %= 10;
							send_buff[idx++] = ang_val + '0';
						
							/* 帧尾 */
							send_buff[idx++] = ']';
							BlueSerial_SendBuff(send_buff, idx);
						}LED1_OFF();
			PWM_Flag=0;
			PWM4_SetCompare3(Get_Motor_Duty_FrontLeft());     /* 左前 */
			PWM4_SetCompare2(Get_Motor_Duty_FrontRight());    /* 右前 */
			PWM4_SetCompare4(Get_Motor_Duty_BackRight());     /* 右后 */
			PWM4_SetCompare1(Get_Motor_Duty_BackLeft());      /* 左后 */
			
						
		}
		
	if(crsf_tick)
	{
		crsf_tick = 0;
		CRSF_Process();   
	}
	if(crsf_frame_received)
		{
				 
			crsf_frame_received = 0;
			ReceiveSuccessCount++;
			
			/* 遥控通道映射 */
			rc_roll  = (int16_t)(rcChannels[0] - 1500) / 5;      /* ±100 */
			rc_pitch = (int16_t)(rcChannels[1] - 1500) / 5;      /* ±100 */
			rc_thr   = (uint8_t)((rcChannels[2] - 1000) / 10);   /* 0~100 */
			rc_yaw   = (int16_t)(rcChannels[3] - 1500) * 9 / 5;  /* ±900 */
			servo_status = (rcChannels[4] > 1500) ? 1 : 0;
			MAG_intf     = (rcChannels[5] > 1500) ? 1 : 0;
			
			 Contrl =rc_thr ;
            Set_Base_Duty(Contrl);
			
            Pitch_aim_Get(rc_pitch/10.0f);
            Roll_aim_Get(rc_roll/10.0f);
		}
	if(servo_status==1)
	{
	LED2_ON();	
	}
	else
	{
		LED2_OFF();
	}
		
	if(MAG_intf==1)
	{
	LED3_ON();
	}
	else
	{
		LED3_OFF();
	}
	/* 从控数据刷新：主循环每轮检查一次 */
	if(slave.updated)
	{
		slave.updated = 0;
		s_flow_x   = slave.flow_x;
		s_flow_y   = slave.flow_y;
		s_flow_dist = slave.flow_distance;
		s_baro_alt = slave.baro_altitude;
		s_mag_yaw  = slave.mag_yaw;
	}

		PID_Param_Parse();
		Pitch_Kp_Get(Pitch_Back_Kp());
		Pitch_Ki_Get(Pitch_Back_Ki());
		Pitch_Kd_Get(Pitch_Back_Kd()*0.01);
		Roll_Kp_Get(Roll_Back_Kp());
		Roll_Ki_Get(Roll_Back_Ki());
		Roll_Kd_Get(Roll_Back_Kd()*0.01);
		Yaw_Kp_Get(Yaw_Back_Kp());
		Yaw_Ki_Get(Yaw_Back_Ki());
		Yaw_Kd_Get(Yaw_Back_Kd());
	}
}

/* TIM2：2ms 姿态采样与角速度环数据更新 */
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		CompFilter_Simple();             
		Get_Gyro(&rollRate,&pitchRate,&yawRate);
		angle_rate_update_flag = 1;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}

/* TIM3：10ms 角度外环数据更新 */
void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		Get_Angle(&roll,&pitch,&yaw);
		angle_update_flag = 1;
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	}
}

/* TIM1：5ms 置位 CRSF 解析标志，实际解析在主循环完成 */
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
	{
		crsf_tick = 1;
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}

/* TIM4：20ms PWM 输出周期，置位电机 PWM 刷新标志 */
void TIM4_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
	{
		PWM_Flag=1;
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
