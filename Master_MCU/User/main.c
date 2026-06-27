#include "stm32f10x.h"
#include "Delay.h"
//#include "OLED.h"   
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

// 全局变量：用于中断和主循环共享数据
uint8_t  C=0;
int16_t AX, AY, AZ, GX, GY, GZ;
float roll,pitch,yaw;
float rollRate,pitchRate,yawRate;
float temp;
long press;
float absAlt, relAlt;
extern float HMC5883L_Yaw;
uint8_t Contrl;

uint8_t SendFlag;								//发送标志位
uint8_t SendSuccessCount, SendFailedCount;		//发送成功计次，发送失败计次
uint8_t ReceiveFlag;							//接收标志位
uint8_t ReceiveSuccessCount, ReceiveFailedCount;//接收成功计次，接收失败

float p0,d0;
extern volatile uint8_t NRF24L01_RxIrqFlag;
volatile uint8_t Receive_loss;    // 失联计数器
uint16_t down_cnt = 0;    // 减速周期计数器
#define DOWN_PERIOD 4     // 越大减速越慢
//标志位
volatile uint8_t angle_update_flag = 0;
volatile uint8_t angle_rate_update_flag=0;
volatile uint8_t crsf_tick = 0;         /* TIM1 触发 CRSF 解析 */
uint8_t PWM_Flag=0;							//PWM更新标志位

// QMC5883P相关全局变量
uint8_t qmc_init_ok = 0;        // QMC初始化状态（0=失败，1=成功）
uint8_t qmc_calibrated = 0;     // QMC校准完成标记（0=未校准，1=已校准）
uint8_t yaw_update_flag = 0; 
float qmc_yaw = 0.0f;           // QMC航向角（独立存储，避免和MPU的yaw冲突）


//蓝牙串口
uint8_t send_div_cnt = 0;
#define SEND_DIV_NUM 5   
uint8_t send_buff[32];

//遥控
uint8_t servo_status = 0;   /* CH4 开关: 0=关 1=开 */
uint8_t MAG_intf     = 0;   /* CH5 开关: 0=关 1=开 */
int16_t rc_roll  = 0;       /* CH0 映射 ±100 */
int16_t rc_pitch = 0;       /* CH1 映射 ±100 */
uint8_t rc_thr   = 0;       /* CH2 映射 0~100 */
int16_t rc_yaw   = 0;       /* CH3 映射 ±900 */

//从机传感器数据副本主循环使用，由 slave.updated 触发刷新
int32_t  s_flow_x, s_flow_y;       /* 光流 X/Y 原始值 */
uint16_t s_flow_dist;              /* 光流测距 (mm) */
float    s_baro_alt;               /* 气压高度 (cm) */
float    s_mag_yaw;                /* 磁力计航向 (0~360°) */

char buf[32];


void SystemClock_Config(void)
{
	uint8_t retry = 0;
	
	// 1. 复位时钟
	RCC_DeInit();
	
	// 2. 打开外部晶振 HSE
	RCC_HSEConfig(RCC_HSE_ON);
	
	// 3. 等待 HSE 稳定，超时重试（解决长按复位问题）
	while (RCC_WaitForHSEStartUp() != SUCCESS)
	{
		retry++;
		if(retry > 10)  // 多次重试，保证起振
		{
			RCC_HSEConfig(RCC_HSE_OFF);
			Delay_ms(10);
			RCC_HSEConfig(RCC_HSE_ON);
			retry = 0;
		}
	}
	
	// 4. 配置 FLASH 延迟（必须！72MHz 需要）
	FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
	FLASH_SetLatency(FLASH_Latency_2);
	
	// 5. 分频配置
	RCC_HCLKConfig(RCC_SYSCLK_Div1);    // AHB  = 72MHz
	RCC_PCLK1Config(RCC_HCLK_Div2);     // APB1 = 36MHz
	RCC_PCLK2Config(RCC_HCLK_Div1);     // APB2 = 72MHz
	
	// 6. PLL = 8MHz ×9 =72MHz
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
	RCC_PLLCmd(ENABLE);
	
	// 7. 等待 PLL 锁定（超时自动重新配置）
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
	
	// 8. 切换系统时钟到 PLL
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	while (RCC_GetSYSCLKSource() != 0x08);
}


int main(void)
{
	Delay_us(500);   // 给电源、传感器稳定时间
	SystemClock_Config();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);				//配置NVIC为分组2
	PWM1_Init();
	PWM3_Init();
	PWM4_Init();       // TIM2中断在此初始化
	TIM1_Init_1S_IRQ();
	LED_Init();	
//	OLED_Init();	/* 调试用，已注释 */
	MPU6050_Init();		
	Kalman_Roll_Init();
	Kalman_Pitch_Init();
	BlueSerial_Init();
	IWDG_Init();
	CRSF_Init();
	SlaveMCU_Init();
	// ========== 新增：QMC5883P初始化 + 校准 ==========
//    //OLED_ShowString(4, 1, "QMC:Init...");
//    qmc_init_ok = (QMC5883P_Init() == 0) ? 1 : 0; // 初始化QMC5883P
//    if(qmc_init_ok)
//    {
//        //OLED_ShowString(4, 1, "QMC:Cal... ");  // 显示校准中
//		LED1_ON();
//        QMC5883P_Calibrate_Start();            // 开始校准
//        
//        // 校准流程：循环收集极值（约2秒，飞控上电后缓慢旋转360度）
//        // 注：校准放在主循环前执行，避免中断干扰
//        for(uint16_t i=0; i<500; i++)
//        {
//            QMC5883P_Calibrate_Collect();
//            //Delay_ms(10);
//        }
//        QMC5883P_Calibrate_End();  // 计算校准偏移
//        qmc_calibrated = 1;
//        //OLED_ShowString(4, 1, "OK     "); // 校准完成
//		LED1_OFF();
//    }
//    else
//    {
//        //OLED_ShowString(4, 1, "QMC:ERR    "); // QMC初始化失败
//    }
	
	
//	// 采样得到yaw目标角度
//	Delay_ms(500);
//	LED1_ON();
//	Delay_ms(500);
//	MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);
//	CompFilter_Simple();
//	Get_Angle(&roll, &pitch, &yaw);
//	Yaw_aim_Get(yaw);
//	LED1_OFF();


//	
//	NRF24L01_Init();
//	NRF24L01_IRQ_Init();


	while (1)
	{
		IWDG_ReloadCounter();
		if(angle_update_flag == 1)
		{
//			LED1_ON();	/* 外环更新心跳 */
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
			Drone_Inner_Rate_PID_Control(rollRate,pitchRate,yawRate);          //计算角速度环PID
			angle_rate_update_flag = 0;
		}
		if(PWM_Flag==1)
		{			
LED1_ON();
						//蓝牙串口
						send_div_cnt++;
						if(send_div_cnt >= SEND_DIV_NUM)
						{
							send_div_cnt = 0;
							uint16_t idx = 0;
							int16_t ang_val;
						
							// 拼接帧头 [plot,
							send_buff[idx++] = '[';
							send_buff[idx++] = 'p';
							send_buff[idx++] = 'l';
							send_buff[idx++] = 'o';
							send_buff[idx++] = 't';
							send_buff[idx++] = ',';
						
							// 浮点转整型，截断小数部分（向下取整）扩大10倍
							ang_val = (int16_t)(pitch * 10.0f);
						
							// 处理正负号
							if(ang_val < 0)
							{
								send_buff[idx++] = '-';
								ang_val = (uint16_t)(-ang_val);
							}
						
							// 逐位拆分数字
							if(ang_val >= 1000) send_buff[idx++] = ang_val / 1000 + '0'; ang_val %= 1000;
							if(ang_val >= 100)  send_buff[idx++] = ang_val / 100  + '0'; ang_val %= 100;
							if(ang_val >= 10)   send_buff[idx++] = ang_val / 10   + '0'; ang_val %= 10;
							send_buff[idx++] = ang_val + '0';
						
							// 帧尾
							send_buff[idx++] = ']';
							// 调用新增的批量发送函数
							BlueSerial_SendBuff(send_buff, idx);
						}LED1_OFF();
			PWM_Flag=0;
			PWM4_SetCompare3(Get_Motor_Duty_FrontLeft());                     //左前
			PWM4_SetCompare2(Get_Motor_Duty_FrontRight());                        //右前
			PWM4_SetCompare4(Get_Motor_Duty_BackRight());                     //  右后
			PWM4_SetCompare1(Get_Motor_Duty_BackLeft());                //左后  
			
						
		}
//		if(Receive_loss>=15)                      //增加遥控丢失缓慢降落逻辑
//		{
//			down_cnt++;
//			if(down_cnt >= DOWN_PERIOD)
//			{
//				down_cnt = 0;
//				if(Contrl > 0)
//				{
//					Contrl =Contrl-1;
//					if(Contrl < 0) Contrl = 0;
//				}
//			}
//			Set_Base_Duty(Contrl);
////			Pitch_aim_Get(0);
////			Roll_aim_Get(0);
//		}
//		
		
	if(crsf_tick)
	{
		crsf_tick = 0;
		CRSF_Process();   
	}
	if(crsf_frame_received)
		{
				 
			crsf_frame_received = 0;
			ReceiveSuccessCount++;
			
			/* 遥控值映射到变量 */
			rc_roll  = (int16_t)(rcChannels[0] - 1500) / 5;      /* ±100 */
			rc_pitch = (int16_t)(rcChannels[1] - 1500) / 5;      /* ±100 */
			rc_thr   = (uint8_t)((rcChannels[2] - 1000) / 10);   /* 0~100 */
			rc_yaw   = (int16_t)(rcChannels[3] - 1500) * 9 / 5;  /* ±900 */
			servo_status = (rcChannels[4] > 1500) ? 1 : 0;
			MAG_intf     = (rcChannels[5] > 1500) ? 1 : 0;
			
			 Contrl =rc_thr ;
            Set_Base_Duty(Contrl);               //设置油门
			
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
//		  sprintf(buf, "R %+4d  S%d", rc_roll, servo_status);
//            OLED_ShowString(1, 1, buf);
//            sprintf(buf, "P %+4d  M%d", rc_pitch, MAG_intf);
//            OLED_ShowString(2, 1, buf);
//            sprintf(buf, "T %3u   ",  rc_thr);
//            OLED_ShowString(3, 1, buf);
//            sprintf(buf, "Y %+4d   ", rc_yaw);
//            OLED_ShowString(4, 1, buf);

	/* ── 从机数据赋值（实时，每轮循环检查）── */
	if(slave.updated)
	{
		slave.updated = 0;
		s_flow_x   = slave.flow_x;
		s_flow_y   = slave.flow_y;
		s_flow_dist = slave.flow_distance;
		s_baro_alt = slave.baro_altitude;
		s_mag_yaw  = slave.mag_yaw;
	}

	/* ── OLED 轮流刷行（调试用，已注释）──
	{
		static uint8_t oled_line = 0;
		switch (oled_line)
		{
		case 0:
			sprintf(buf, "X%+4d Y%+4d     ", s_flow_x, s_flow_y);
			OLED_ShowString(1, 1, buf);
			break;
		case 1:
			sprintf(buf, "FH%-3dmm         ", s_flow_dist);
			OLED_ShowString(2, 1, buf);
			break;
		case 2:
			sprintf(buf, "BH%-4.0fcm        ", s_baro_alt);
			OLED_ShowString(3, 1, buf);
			break;
		case 3:
			sprintf(buf, "Y%-4.0f           ", s_mag_yaw);
			OLED_ShowString(4, 1, buf);
			break;
		}
		oled_line = (oled_line + 1) & 3;
	}
	*/
//	OLED_ShowString(1,1,"A");
//		if(NRF24L01_RxIrqFlag)                  //正常接收遥控
//		{
//			NRF24L01_RxIrqFlag = 0; // 清标志，防止重复进
//			
//			
//			uint8_t ReceiveFlag = NRF24L01_Receive(); // 真正读一包

//			if (ReceiveFlag == 1) // 成功收到
//			{
//			Receive_loss=0;
//            ReceiveSuccessCount++;
//            Contrl = NRF24L01_RxPacket[0];
//            Set_Base_Duty(Contrl);
//            Pitch_aim_Get(((int8_t)NRF24L01_RxPacket[1]-50.0f)/10.0f);
//            Roll_aim_Get(((int8_t)NRF24L01_RxPacket[2]-50.0f)/10.0f);
//			}
//			else
//			{
//            ReceiveFailedCount++;
//			}
//		}
//		

//		BlueSerial_Printf("[plot,%d]",Contrl);
//    蓝牙调试部分//	
		PID_Param_Parse();     //使用中断标志位
//		Set_Base_Duty(Back_Base_Duty());
		Pitch_Kp_Get(Pitch_Back_Kp());
		Pitch_Ki_Get(Pitch_Back_Ki());
		Pitch_Kd_Get(Pitch_Back_Kd()*0.01);
		Roll_Kp_Get(Roll_Back_Kp());
		Roll_Ki_Get(Roll_Back_Ki());
		Roll_Kd_Get(Roll_Back_Kd()*0.01);
		Yaw_Kp_Get(Yaw_Back_Kp());
		Yaw_Ki_Get(Yaw_Back_Ki());
		Yaw_Kd_Get(Yaw_Back_Kd());
//		Pitch_Angle_Kp_Get(Pitch_Angle_Back_Kp());
//		Roll_Angle_Kp_Get(Roll_Angle_Back_Kp());
//		Yaw_Angle_Kp_Get(Yaw_Angle_Back_Kp());
//		Roll_aim_Get(Roll_Back_Aim());
//		Pitch_aim_Get(Pitch_Back_Aim());
//		absAlt=Pitch_err_Get();
//		relAlt=Pitch_Back_Aim();
	
//		Drone_RollPitchYaw_PID_Control(roll, pitch,yaw,rollRate,pitchRate,yawRate);
	}
}

// TIM2中断服务函数（角速度环数据更新）1,1
void TIM2_IRQHandler(void)
{
	// 检查更新中断标志位
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
//		MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);		
		CompFilter_Simple();             
		Get_Gyro(&rollRate,&pitchRate,&yawRate);
		angle_rate_update_flag = 1;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);    //清除中断标志位
	}
}
// TIM3中断服务函数（角度环数据更新）1,3
void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		Get_Angle(&roll,&pitch,&yaw);
		angle_update_flag = 1;
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	}
}
// TIM1中断（5ms → 仅置标志，CRSF 解析在主循环处理）2,2
void TIM1_UP_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
	{
		crsf_tick = 1;
		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
	}
}
// TIM4中断PWM更新中断3，1
void TIM4_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
	{
		PWM_Flag=1;
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
