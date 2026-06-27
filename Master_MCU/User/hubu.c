#include "stm32f10x.h"
#include "MPU6050.h"  
#include "QMC5883P.h"
#include <math.h>
#include "Kalman.h"                  // Device header

// 滤波参数（可根据实际调试调整）
#define ALPHA 0.79f    // Roll/Pitch：陀螺仪权重（越大越平滑）
#define BETA 0.12f     // Yaw融合系数：磁力计权重（0.01~0.05，越小越稳定越慢）
#define DT 0.002f      // 采样周期（TIM2中断周期=2ms）
#define GYRO_SCALE 16.4f  // 陀螺仪灵敏度（±2000°/s对应16.4 LSB/(°/s)）
#define ACC_SCALE 1024.0f // 加速度计灵敏度（±16g对应1024 LSB/g）
#define M_PI 3.14159265358979323846f
#define GYRO_FILTER_ALPHA 0.2f 



// 角速度滤波 上一值（静态全局变量）
static float last_gx = 0.0f;
static float last_gy = 0.0f;

// 全局角度变量（融合后最终值）
float Roll=0.0f;  
float Pitch = 0.0f; 
float Yaw = 0.0f;   

float gx=0.0f;
float gy=0.0f;
float gz=0.0f;

// 固有误差补偿（根据实际硬件校准）
float Roll_Aim=2.0f;  
float Pitch_Aim =-1.0f; 
float Yaw_Aim = +0.0f;   

/**
 * 互补滤波主函数（内置磁力计Yaw融合，解决零漂）
 */
void CompFilter_Simple(void)
{
    // 1. 读取MPU6050原始数据
    int16_t AX, AY, AZ, GX, GY, GZ;
    MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);
    
    // 2. 加速度计解算Roll/Pitch（静态基准）
    float ax = (float)AX / ACC_SCALE;
    float ay = (float)AY / ACC_SCALE;
    float az = (float)AZ / ACC_SCALE;
	
	
    float roll_acc = atan2(ay, sqrt(ax*ax + az*az)) * 180/M_PI;
    float pitch_acc = atan2(-ax, sqrt(ay*ay + az*az)) * 180/M_PI;
    
    // 3. 陀螺仪零漂补偿（根据实际硬件校准，示例值）
    GX += 64;  // X轴零漂补偿
    GY += 29;   // Y轴零漂补偿
    GZ +=43;  // Z轴零漂补偿

    // 4. 陀螺仪积分计算角度（动态基准）
     gx = (float)GX / GYRO_SCALE;  // Roll角速度（°/s）
     gy = (float)GY / GYRO_SCALE;  // Pitch角速度（°/s）
     gz = (float)GZ / GYRO_SCALE;  // Yaw角速度（°/s）
	// ====================== 角速度互补滤波 ======================
    gx = GYRO_FILTER_ALPHA * last_gx + (1 - GYRO_FILTER_ALPHA) * gx;
    gy = GYRO_FILTER_ALPHA * last_gy + (1 - GYRO_FILTER_ALPHA) * gy;
    last_gx = gx; // 更新上一帧值
    last_gy = gy;
	
    Yaw += gz * DT;  // 陀螺仪原始Yaw（未融合，有零漂）
    
    // 5. Roll/Pitch卡尔曼滤波（陀螺仪+加速度计）
    // dt=0.002f 匹配 TIM2 中断周期 2ms
	Roll = Kalman_Get_Roll(gx, roll_acc, 0.002f);
	Pitch = Kalman_Get_Pitch(gy, pitch_acc, 0.002f);
	
    // ========== 核心新增：Yaw融合（陀螺仪+磁力计） ==========
//    extern uint8_t qmc_init_ok, qmc_calibrated; // 引用主程序的全局变量
//    extern float qmc_yaw;                       // 引用磁力计Yaw
//    
//    if(qmc_init_ok && qmc_calibrated)
//    {
//        // 磁力计Yaw预处理：统一角度范围（-180~180 → 0~360）
//        float mag_yaw = qmc_yaw;
//        if(mag_yaw < 0) mag_yaw += 360.0f;
//        
//        // 陀螺仪Yaw预处理：防止角度溢出（超过360°取模）
//        if(Yaw > 360.0f) Yaw -= 360.0f;
//        if(Yaw < 0) Yaw += 360.0f;
//        
//        // 互补滤波融合：用磁力计修正陀螺仪零漂
//       // Yaw = (1 - BETA) * Yaw + BETA * mag_yaw;
//    }
}

/**
 * 读取融合后的最终角度（含误差补偿）
 */
void Get_Angle(float *roll, float *pitch, float *yaw)
{
    *roll = Roll + Roll_Aim;
    *pitch = Pitch + Pitch_Aim;
    *yaw = Yaw+ Yaw_Aim; // 输出融合后的Yaw（无零漂）
}

// 获取三轴角速度
void Get_Gyro(float *rollRate, float *pitchRate, float *yawRate)
{
    *rollRate  = gx;
    *pitchRate = gy;
    *yawRate   =gz;
}

/**
 * 可选：Yaw手动清零（校准初始角度）
 */
void Yaw_Calibrate(void)
{
    Yaw = 0.0f;
}
