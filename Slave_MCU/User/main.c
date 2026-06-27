/**
 * ============================================================
 *  Slave_MCU — 无人机从机（STM32F103C8T6）
 *
 *  功能：
 *   1. 气压计 BMP390 —— SPI1 读取（50ms 周期 20Hz）
 *   2. 光流传感器   —— USART1 中断接收（自动更新）
 *   3. 磁力计 QMC5883P —— 软件 I2C（PB10/PB11）
 *   4. 舵机控制     —— PA1 电平 → PB0 PWM
 *   5. 校准触发     —— PA8 脉冲(1→0→1) → 10秒倒计时校准磁力计
 *   6. 数据汇总     —— 打包后通过 USART2 发送给主飞控
 *   7. OLED 显示    —— I2C（PB8/PB9）
 *   8. AT7456E OSD   —— SPI2（PB12-15）图传叠加
 *
 *  引脚占用总览：
 *    PA0:  EXTI 按键归零（exti.c）
 *    PA1:  舵机电平检测输入（IPU）
 *    PA3:  有源蜂鸣器（低电平响）
 *    PA2:  USART2_TX → 上位机
 *    PA4:  SPI1_CS  → BMP390
 *    PA5:  SPI1_SCK
 *    PA6:  SPI1_MISO
 *    PA7:  SPI1_MOSI
 *    PA8:  校准触发输入（IPU, 检测 0→1→0 脉冲）
 *    PA9:  USART1_TX → 光流模块
 *    PA10: USART1_RX ← 光流模块
 *    PB1:  ADC1_IN9 → 电池电压检测
 *    PB0:  TIM3_CH3 → 舵机 PWM
 *    PB8:  OLED_SCL
 *    PB9:  OLED_SDA
 *    PB10: QMC5883P_SCL
 *    PB11: QMC5883P_SDA
 *    PB12: SPI2_CS  → AT7456E（OSD图传）
 *    PB13: SPI2_SCK
 *    PB14: SPI2_MISO
 *    PB15: SPI2_MOSI
 * ============================================================
 */
#include "stm32f10x.h"
#include "stm32f10x_iwdg.h"
#include "stm32f10x_dbgmcu.h"
#include "bmp3.h"
#include <stdio.h>
#include <math.h>
#include "Delay.h"
#include "OLED.h"
#include "exti.h"
#include "QMC5883P.h"
#include "OpticalFlow.h"
#include "AT7456E.h"

// ============================================================
// BMP390 SPI 驱动函数声明（实现在 Hardware/BMP390.c）
// ============================================================
extern void    SPI1_Init(void);
extern int8_t  bmp3_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
extern int8_t  bmp3_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
extern void    bmp3_delay_us(uint32_t period, void *intf_ptr);
extern uint8_t SPI_ReadWrite(uint8_t tx_data);

// ============================================================
// 外部变量
// ============================================================
extern volatile uint8_t key_flag;   // exti.c：PA0 下降沿 → 归零相对高度

// ============================================================
// 引脚宏定义
// ============================================================
#define SERVO_LEVEL_PORT   GPIOA
#define SERVO_LEVEL_PIN    GPIO_Pin_1
#define CALIB_TRIG_PORT    GPIOA
#define CALIB_TRIG_PIN     GPIO_Pin_8

// ============================================================
// 舵机 PWM 角度预设（脉宽 us，500=0°, 1500=90°, 2500=180°）
// ============================================================
#define SERVO_ANGLE_HIGH   2000    // 高电平 → 约 135°
#define SERVO_ANGLE_LOW    1000    // 低电平 → 约 45°

// ============================================================
// 校准参数
// ============================================================
#define CALIB_DURATION_SEC  10     // 校准倒计时（秒）
#define ADC_VOLT_DIVIDER    4.0f  // 电池分压比（R1+R2)/R2，按实际电路修改
#define ADC_VREF            3.30f // 基准电压
#define LOW_BATT_THRESHOLD  10.50f // 低电量预警阈值 (V), 3.5V/节

#define BUZZER_PORT         GPIOA
#define BUZZER_PIN          GPIO_Pin_3    // PA3, 有源蜂鸣器，低电平响

// ============================================================
// 数据包定义（USART2 发送给主飞控）
// ============================================================
#define PACKET_HEADER  0xA5

typedef __packed struct {
    uint8_t  header;           // 0xA5
    uint8_t  length;           // 负载长度（不含 header/checksum）
    float    pressure;         // 气压 (Pa)
    float    temperature;      // 温度 (°C)
    float    altitude;         // 相对高度 (cm)
    float    yaw;              // 磁力计航向 (0~360°)
    int32_t  flow_x;           // 光流 X 原始值
    int32_t  flow_y;           // 光流 Y 原始值
    uint16_t flow_distance;    // 测距 (mm)
    uint8_t  flow_quality;     // 信号强度 (0~100)
    uint8_t  checksum;         // 异或校验
} SensorPacket_t;

// ============================================================
// 看门狗
// ============================================================
#define IWDG_RELOAD_COUNT  625    // LSI 40kHz / 64 = 625Hz, 625 / 625 = 1s 超时

static void IWDG_Init(void)
{
    /* 调试时冻结 IWDG，避免断点导致复位 */
    DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

    /* 使能 LSI，等待就绪 */
    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

    /* 解锁 IWDG 寄存器 */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    /* 配置：LSI/64 = 625Hz, 重装载 625 → 1 秒超时 */
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(IWDG_RELOAD_COUNT);

    /* 喂狗并启动 */
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/** 喂狗，主循环中调用 */
static void IWDG_Feed(void)
{
    IWDG_ReloadCounter();
}

// ============================================================
// 定时器 50ms 中断标志（20Hz）
// ============================================================
volatile uint8_t timer_100ms_flag = 0;
static float   battery_voltage = 0.0f;   // 电池电压

// ============================================================
// ==== 滤波模块（保留原有双级滤波）====
// ============================================================

/* ---- 滑动均值滤波 ---- */
#define FILTER_WINDOW_SIZE  8

static double filter_buffer[FILTER_WINDOW_SIZE] = {0};
static int    filter_index  = 0;
static int    filter_filled = 0;

static double moving_average_filter(double new_val)
{
    filter_buffer[filter_index] = new_val;
    filter_index = (filter_index + 1) % FILTER_WINDOW_SIZE;
    if (filter_index == 0) filter_filled = 1;

    int    count = filter_filled ? FILTER_WINDOW_SIZE : filter_index;
    double sum   = 0.0;
    for (int i = 0; i < count; i++) sum += filter_buffer[i];
    return sum / count;
}

/* ---- 长时间常数高通滤波（滤除天气漂移）---- */
#define BASELINE_ALPHA  0.00025

static double altitude_baseline   = 0.0;
static int    baseline_initialized = 0;
static double ref_altitude        = 0.0;

static double drift_filter(double raw_altitude)
{
    if (!baseline_initialized) {
        altitude_baseline   = raw_altitude;
        baseline_initialized = 1;
        return raw_altitude;
    }
    altitude_baseline += BASELINE_ALPHA * (raw_altitude - altitude_baseline);
    return raw_altitude - altitude_baseline;
}

// ============================================================
// ==== TIM2 初始化：50ms 中断（20Hz）====
// ============================================================
static void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Prescaler         = 7200 - 1;    // 10kHz
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period            = 500 - 1;     // 50ms
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel    = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

// ============================================================
// ==== TIM3 PWM 初始化：舵机（PB0, 50Hz, 20ms 周期）====
// ============================================================
static void Servo_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // PB0 → TIM3_CH3 复用推挽
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // TIM3: 72MHz / 72 = 1MHz, ARR=20000-1 → 20ms
    TIM_TimeBaseStructure.TIM_Prescaler         = 72 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period            = 20000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 1500;     // 默认中立位 1.5ms
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

static void Servo_SetPulse(uint16_t pulse_us)
{
    if (pulse_us < 500)  pulse_us = 500;
    if (pulse_us > 2500) pulse_us = 2500;
    TIM_SetCompare3(TIM3, pulse_us);
}

// ============================================================
// ==== GPIO 输入初始化 ====
// ============================================================
static void GPIO_Inputs_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA1: 舵机电平检测（上拉输入）
    GPIO_InitStructure.GPIO_Pin   = SERVO_LEVEL_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SERVO_LEVEL_PORT, &GPIO_InitStructure);

    // PA8: 校准触发（上拉输入）
    GPIO_InitStructure.GPIO_Pin = CALIB_TRIG_PIN;
    GPIO_Init(CALIB_TRIG_PORT, &GPIO_InitStructure);
}

// ============================================================
// ==== 蜂鸣器初始化：PA3，低电平驱动 ====
// ============================================================
static void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);  // 默认高电平 = 蜂鸣器不响
}

// ============================================================
// ==== ADC1 初始化：PB1 电池电压检测 ====
// ============================================================
static void ADC_Battery_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_ADC1, ENABLE);

    // PB1 = 模拟输入
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ADC1 配置：单次转换，软件触发
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_55Cycles5);
    ADC_Cmd(ADC1, ENABLE);

    // 校准
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/** @brief 读取电池电压，返回 V */
static float ADC_ReadBattery(void)
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    uint16_t adc_val = ADC_GetConversionValue(ADC1);
    return (float)adc_val / 4095.0f * ADC_VREF * ADC_VOLT_DIVIDER;
}

// ============================================================
// ==== USART2 初始化：向上位机发送数据（仅 TX）====
// ============================================================
static void USART2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // PA2 = TX
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

// ============================================================
// ==== USART2 发送一包数据 ====
// ============================================================
static void USART2_SendPacket(SensorPacket_t *pkt)
{
    uint8_t i;
    uint8_t *raw  = (uint8_t *)pkt;
    uint8_t  csum = 0;
    
    pkt->header = PACKET_HEADER;
    pkt->length = sizeof(SensorPacket_t) - 2;

    for (i = 0; i < (uint8_t)sizeof(SensorPacket_t) - 1; i++) {
        csum ^= raw[i];
    }
    pkt->checksum = csum;

    for (i = 0; i < (uint8_t)sizeof(SensorPacket_t); i++) {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, raw[i]);
    }
}

// ============================================================
// 校准状态机类型（文件作用域）
// ============================================================
typedef enum { CALIB_IDLE, CALIB_COUNTDOWN } CalibState_t;

// ============================================================
// ==== 主函数 ====
// ============================================================
int main(void)
{
    // ========== 阶段1：硬件初始化 ==========
    SPI1_Init();                // SPI1 → BMP390 气压计
    OpticalFlow_Init();         // USART1 → 光流传感器（中断接收）
    USART2_Init();              // USART2 → 主飞控通信
    Servo_Init();               // TIM3_CH3(PB0) → 舵机 PWM
    ADC_Battery_Init();         // PB1 → ADC1_IN9 电池电压检测
    GPIO_Inputs_Init();         // PA1(舵机检测) + PA8(校准触发)
    Buzzer_Init();              // PA3 → 有源蜂鸣器（低电平响）
    AT7456E_Init();             // AT7456E OSD 图传芯片（软件 SPI）
    OSD_Init();                 // OSD 显示框架初始化
    OLED_Init();
    OLED_Clear();
    EXTI_Key_Init();            // PA0 EXTI（exti.c: 归零相对高度）

    // ========== 阶段2：BMP390 气压计初始化 ==========
    struct bmp3_dev      dev;
    struct bmp3_settings settings;
    int8_t rslt;

    dev.intf     = BMP3_SPI_INTF;
    dev.read     = bmp3_spi_read;
    dev.write    = bmp3_spi_write;
    dev.delay_us = bmp3_delay_us;
    dev.intf_ptr = &dev;

    rslt = bmp3_init(&dev);
    bmp3_get_sensor_settings(&settings, &dev);

    settings.press_en            = BMP3_ENABLE;
    settings.temp_en             = BMP3_ENABLE;
    settings.odr_filter.press_os = 3;
    settings.odr_filter.temp_os  = 1;
    settings.odr_filter.iir_filter = 3;
    settings.odr_filter.odr      = BMP3_ODR_25_HZ;

    uint32_t desired = BMP3_SEL_PRESS_EN | BMP3_SEL_TEMP_EN |
                       BMP3_SEL_PRESS_OS  | BMP3_SEL_TEMP_OS |
                       BMP3_SEL_IIR_FILTER | BMP3_SEL_ODR;
    bmp3_set_sensor_settings(desired, &settings, &dev);

    settings.op_mode = BMP3_MODE_NORMAL;
    bmp3_set_op_mode(&settings, &dev);
    Delay_ms(200);

    // ========== 阶段3：QMC5883P 磁力计初始化 ==========
    uint8_t mag_ok = (QMC5883P_Init() == 0);

    // ========== 阶段4：启动看门狗（所有硬件初始化完成后）==========
    IWDG_Init();

    // ========== 阶段5：启动定时器 ==========
    TIM2_Init();    // 100ms 周期中断

    // ========== 阶段6：状态变量 ==========
    struct bmp3_data data;
    char buf[20];

    // ---- 校准状态机 ----
    CalibState_t calib_state           = CALIB_IDLE;
    uint16_t     calib_timer           = 0;       // 倒计时（×50ms）
    uint8_t      calib_prev_level        = 1;       // PA8 上一轮电平
    uint8_t      calib_falling_detected  = 0;       // 是否已捕获下降沿

    // ========== 阶段7：主循环 ==========
    while (1)
    {
        IWDG_Feed();   // 喂狗，每轮一次

        // ====================================================
        // ① 舵机电平检测（PA1: 高→角度A, 低→角度B）
        // ====================================================
        uint8_t servo_level = GPIO_ReadInputDataBit(SERVO_LEVEL_PORT, SERVO_LEVEL_PIN);
        Servo_SetPulse(servo_level ? SERVO_ANGLE_HIGH : SERVO_ANGLE_LOW);

        // ====================================================
        // ② 校准触发检测（PA8: 1→0→1 脉冲 → 进入校准）
        // ====================================================
        uint8_t calib_level = GPIO_ReadInputDataBit(CALIB_TRIG_PORT, CALIB_TRIG_PIN);

        if (calib_prev_level == 1 && calib_level == 0) {
            calib_falling_detected = 1;             // 捕获下降沿
        }
        else if (calib_falling_detected && calib_prev_level == 0 && calib_level == 1) {
            // 下降沿之后出现上升沿 → 完整 1→0→1 脉冲
            if (calib_state == CALIB_IDLE) {
                calib_state = CALIB_COUNTDOWN;
                calib_timer = CALIB_DURATION_SEC * 20;   // 10秒 × 20(每50ms)
                QMC5883P_Calibrate_Start();
            }
            calib_falling_detected = 0;
        }
        calib_prev_level = calib_level;

        // ====================================================
        // ③ 校准采集（每个循环都采，不等待定时器）
        // ====================================================
        if (calib_state == CALIB_COUNTDOWN) {
            QMC5883P_Calibrate_Collect();
        }

        // ====================================================
        // ④ 50ms 定时 → 读取传感器 + 打包 + 发送 + 显示
        // ====================================================
        if (timer_100ms_flag) {
            timer_100ms_flag = 0;

            // ---- 校准倒计时 ----
            if (calib_state == CALIB_COUNTDOWN) {
                if (--calib_timer == 0) {
                    if (QMC5883P_Calibrate_End() == 0) {
                        // Flash 保存成功，可通过 OLED/蜂鸣器提示
                    }
                    calib_state = CALIB_IDLE;
                }
            }

            // ---- 光流超时检测 ----
            OpticalFlow_TimeoutCheck();

            // ---- 读取电池电压 + 低电量预警 ----
            battery_voltage = ADC_ReadBattery();
            if (battery_voltage < LOW_BATT_THRESHOLD && battery_voltage > 0.5f) {
                GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);  // 低电平 → 蜂鸣
            } else {
                GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);    // 高电平 → 静音
            }

            // ---- 读取 BMP390 气压计 ----
            float pressure = 0.0f, temperature = 0.0f;
            float altitude = 0.0f, rela_altitude = 0.0f;

            rslt = bmp3_get_sensor_data(BMP3_PRESS_TEMP, &data, &dev);
            if (rslt == BMP3_OK) {
                temperature = data.temperature;
                pressure    = data.pressure;

                // 双级滤波
                double filtered_pressure = moving_average_filter(pressure);
                altitude = 44330.0 * (1.0 - pow(filtered_pressure / 101325.0, 0.190295));
                double drift_free_alt = drift_filter(altitude);
                rela_altitude = drift_free_alt - ref_altitude;

                // PA0 按键 → 归零相对高度
                if (key_flag) {
                    ref_altitude = drift_free_alt;
                    key_flag = 0;
                }
            }

            // ---- 读取磁力计（校准期间不额外读）----
            if (calib_state == CALIB_IDLE && mag_ok) {
                QMC5883P_UpdateYaw();
            }

            // ---- 打包 + 发送给主飞控 ----
            SensorPacket_t pkt;
            pkt.pressure      = pressure;
            pkt.temperature   = temperature;
            pkt.altitude      = rela_altitude * 100.0f;  // 转为 cm
            pkt.yaw           = QMC5883P_Yaw;
            pkt.flow_x        = OpticalFlow_Data.flow_x;
            pkt.flow_y        = OpticalFlow_Data.flow_y;
            pkt.flow_distance = OpticalFlow_Data.distance;
            pkt.flow_quality  = OpticalFlow_Data.signal_strength;
            USART2_SendPacket(&pkt);

            // ---- OSD 图传数据更新 ----
            OSD_DisplayInt(0, 7, 5, OpticalFlow_Data.distance);  // D:距离
            OSD_DisplayFloat(1, 4, 3, 1, QMC5883P_Yaw); // YAW:航向
            OSD_DisplayInt(15, 13, 4, (int)(temperature * 10));  // T:温度(x10)
            OSD_DisplayFloat(14, 3, 1, 1, battery_voltage);  // V:电池电压

            // ---- OLED 显示（BMP390错误也显示其他数据）----
            if (rslt == BMP3_OK) {
                sprintf(buf, "T:%.1fC",  temperature);
                OLED_ShowString(1, 1, buf);
                sprintf(buf, "H:%.1fcm", rela_altitude * 100.0);
                OLED_ShowString(2, 1, buf);
            } else {
                OLED_ShowString(1, 1, "BMP390 ERR    ");
            }
            
            sprintf(buf, "Yaw:%.1f", QMC5883P_Yaw);
            OLED_ShowString(3, 1, buf);

            if (calib_state == CALIB_COUNTDOWN) {
                uint8_t sec = (calib_timer + 9) / 10;
                sprintf(buf, "CAL:%d s", sec);
                OLED_ShowString(4, 1, buf);
            } else {
                sprintf(buf, "D:%.1fcm F:%ld",
                        OpticalFlow_Data.distance / 10.0f,
                        (long)OpticalFlow_Data.flow_x);
                OLED_ShowString(4, 1, buf);
            }
        }
    }
}