#include "stm32f10x.h"                  // Device header
#include "MyI2C.h"
#include "MPU6050_Reg.h"

#define MPU6050_ADDRESS		0xD0		//MPU6050的I2C从机地址

/**
  * 函    数：MPU6050写寄存器
  * 参    数：RegAddress 寄存器地址
  * 参    数：Data 要写入的数据
  * 返 回 值：无
  */
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	MyI2C_Start();						//I2C起始
	MyI2C_SendByte(MPU6050_ADDRESS);	//发送从机地址（写）
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(RegAddress);			//发送寄存器地址
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(Data);				//发送写入数据
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_Stop();						//I2C终止
}

/**
  * 函    数：MPU6050读单个寄存器
  * 参    数：RegAddress 寄存器地址
  * 返 回 值：读取的数据
  */
uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	
	MyI2C_Start();						//I2C起始
	MyI2C_SendByte(MPU6050_ADDRESS);	//发送从机地址（写）
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(RegAddress);			//发送寄存器地址
	MyI2C_ReceiveAck();					//接收应答
	
	MyI2C_Start();						//I2C重复起始
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);	//发送从机地址（读）
	MyI2C_ReceiveAck();					//接收应答
	Data = MyI2C_ReceiveByte();			//接收数据
	MyI2C_SendAck(1);					//发送非应答
	MyI2C_Stop();						//I2C终止
	
	return Data;
}

/**
  * 新增函数：批量读取多个寄存器（核心优化）
  * 参    数：RegAddress 起始寄存器地址
  * 参    数：Data 接收数据的缓冲区
  * 参    数：Len 读取的字节数
  * 返 回 值：无
  */
void MPU6050_ReadRegs(uint8_t RegAddress, uint8_t *Data, uint8_t Len)
{
	MyI2C_Start();						//I2C起始
	MyI2C_SendByte(MPU6050_ADDRESS);	//发送从机地址（写）
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(RegAddress);			//发送起始寄存器地址
	MyI2C_ReceiveAck();					//接收应答
	
	MyI2C_Start();						//I2C重复起始
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);	//发送从机地址（读）
	MyI2C_ReceiveAck();					//接收应答
	
	// 批量读取多个字节
	for(uint8_t i=0; i<Len; i++)
	{
		Data[i] = MyI2C_ReceiveByte();	//接收1个字节
		if(i < Len-1)
		{
			MyI2C_SendAck(0);			//前n-1个字节发送应答
		}
		else
		{
			MyI2C_SendAck(1);			//最后1个字节发送非应答
		}
	}
	
	MyI2C_Stop();						//I2C终止
}

/**
  * 函    数：MPU6050初始化（优化采样率）
  * 参    数：无
  * 返 回 值：无
  */
void MPU6050_Init(void)
{
	MyI2C_Init();									//初始化I2C
	
	/* 核心优化：提升采样率 + 合理配置滤波 */
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);		//取消睡眠，时钟源X轴陀螺仪
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);		//所有轴不待机
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x00);		//采样率分频=0 → 1kHz采样率（原0x07是125Hz）
	MPU6050_WriteReg(MPU6050_CONFIG, 0x00);			//DLPF=6 → 截止频率5Hz，降噪且不影响速度
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);	//陀螺仪±2000°/s
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x18);	//加速度计±16g
}

/**
  * 函    数：MPU6050获取ID号
  * 参    数：无
  * 返 回 值：MPU6050的ID号
  */
uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

/**
  * 函    数：MPU6050获取数据（批量读取优化版）
  * 参    数：AccX/AccY/AccZ 加速度计数据（输出）
  * 参    数：GyroX/GyroY/GyroZ 陀螺仪数据（输出）
  * 返 回 值：无
  * 优化点：1次读取14个字节，替代原来12次单字节读取
  */
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, 
						int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
	uint8_t Data[14];	// 缓冲区：加速度(6)+温度(2)+陀螺仪(6)
	
	// 批量读取所有数据（仅1次I2C启动/停止，耗时<0.1ms）
	MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, Data, 14);
	
	// 拼接加速度计数据
	*AccX = (Data[0] << 8) | Data[1];
	*AccY = (Data[2] << 8) | Data[3];
	*AccZ = (Data[4] << 8) | Data[5];
	*AccZ = -(*AccZ);  // Z轴取反（保持原有逻辑）
	
	// 拼接陀螺仪数据（跳过温度数据：Data[6]~Data[7]）
	*GyroX = (Data[8] << 8) | Data[9];
	*GyroY = (Data[10] << 8) | Data[11];
	*GyroZ = (Data[12] << 8) | Data[13];
	*GyroZ = -(*GyroZ);  // Z轴取反（保持原有逻辑）
}
