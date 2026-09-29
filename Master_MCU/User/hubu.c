#include "stm32f10x.h"
#include "MPU6050.h"  
#include <math.h>
#include "Kalman.h"

/* 姿态解算参数 */
#define BETA 0.12f          /* 每次有效磁力计新帧对 Yaw 估计的校正比例 */
#define DT 0.002f           /* TIM2 采样周期：2ms */
#define GYRO_SCALE 16.4f    /* 陀螺仪 ±2000dps：16.4 LSB/(deg/s) */
#define ACC_SCALE 1024.0f   /* 加速度计 ±16g：1024 LSB/g */
#define HUBU_PI 3.14159265358979323846f
#define GYRO_FILTER_ALPHA 0.2f 

/* 角速度一阶滤波历史值 */
static float last_gx = 0.0f;
static float last_gy = 0.0f;
static uint8_t mag_heading_initialized = 0u;

/* 姿态解算结果 */
float Roll=0.0f;  
float Pitch = 0.0f; 
float Yaw = 0.0f;   

float gx=0.0f;
float gy=0.0f;
float gz=0.0f;

/* 机械安装误差补偿量，根据实机标定填写 */
float Roll_Aim=2.0f;  
float Pitch_Aim =-1.0f; 
float Yaw_Aim = +0.0f;   

/* 航向角采用 [0,360)；做差时采用 [-180,180] 的最短转向。 */
static float Yaw_Wrap360(float angle)
{
    while(angle >= 360.0f) angle -= 360.0f;
    while(angle < 0.0f) angle += 360.0f;
    return angle;
}

static float Yaw_Wrap180(float angle)
{
    while(angle > 180.0f) angle -= 360.0f;
    while(angle < -180.0f) angle += 360.0f;
    return angle;
}

/* 姿态解算主函数：读取 MPU6050，并更新 Roll/Pitch/Yaw 与角速度。 */
void CompFilter_Simple(void)
{
    /* 读取 MPU6050 原始数据 */
    int16_t AX, AY, AZ, GX, GY, GZ;
    MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);
    
    /* 用加速度计计算静态 Roll/Pitch 参考角 */
    float ax = (float)AX / ACC_SCALE;
    float ay = (float)AY / ACC_SCALE;
    float az = (float)AZ / ACC_SCALE;
	
	
    float roll_acc = atan2(ay, sqrt(ax*ax + az*az)) * 180/HUBU_PI;
    float pitch_acc = atan2(-ax, sqrt(ay*ay + az*az)) * 180/HUBU_PI;
    
    /* 陀螺仪零漂补偿，数值来自当前硬件标定 */
    GX += 64;
    GY += 29;
    GZ +=43;

    /* 原始值换算为角速度，单位 deg/s */
     gx = (float)GX / GYRO_SCALE;
     gy = (float)GY / GYRO_SCALE;
     gz = (float)GZ / GYRO_SCALE;
	/* 角速度一阶滤波 */
    gx = GYRO_FILTER_ALPHA * last_gx + (1 - GYRO_FILTER_ALPHA) * gx;
    gy = GYRO_FILTER_ALPHA * last_gy + (1 - GYRO_FILTER_ALPHA) * gy;
    last_gx = gx;
    last_gy = gy;
	
    /* 2ms 陀螺仪积分提供短时连续性；磁力计新帧在另一入口缓慢消除漂移。 */
    Yaw = Yaw_Wrap360(Yaw + gz * DT);
    
    /* Roll/Pitch 使用卡尔曼滤波融合陀螺仪与加速度计 */
	Roll = Kalman_Get_Roll(gx, roll_acc, 0.002f);
	Pitch = Kalman_Get_Pitch(gy, pitch_acc, 0.002f);
	
}

/* 磁力计来自从控新数据，不在 500Hz IMU 任务内重复使用同一帧。
 * 首帧直接建立绝对航向基准；后续沿最短角误差做小比例校正。
 * 这里只做平面航向修正，尚无倾斜补偿或磁异常检测。
 */
uint8_t Yaw_ApplyMagHeading(float heading_deg)
{
    if(!((heading_deg >= 0.0f) && (heading_deg < 360.0f)))
    {
        return 0u;
    }

    if(mag_heading_initialized == 0u)
    {
        Yaw = heading_deg;
        mag_heading_initialized = 1u;
        return 1u;
    }

    Yaw = Yaw_Wrap360(Yaw + BETA * Yaw_Wrap180(heading_deg - Yaw));
    return 0u;
}

/* 读取姿态角，并叠加安装误差补偿。 */
void Get_Angle(float *roll, float *pitch, float *yaw)
{
    *roll = Roll + Roll_Aim;
    *pitch = Pitch + Pitch_Aim;
    *yaw = Yaw_Wrap360(Yaw + Yaw_Aim);
}

/* 获取三轴角速度。 */
void Get_Gyro(float *rollRate, float *pitchRate, float *yawRate)
{
    *rollRate  = gx;
    *pitchRate = gy;
    *yawRate   =gz;
}

/* 手动清零 Yaw 积分角。 */
void Yaw_Calibrate(void)
{
    Yaw = 0.0f;
    mag_heading_initialized = 0u;
}
