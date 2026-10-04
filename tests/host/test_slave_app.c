#include "slave_app.h"
#include "slave_scheduler.h"
#include "slave_board.h"
#include "slave_link.h"
#include "OpticalFlow.h"
#include "exti.h"
#include "bmp3.h"
#include "BMP390.h"
#include "QMC5883P.h"
#include "OLED.h"
#include "AT7456E.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static SlaveInputEvents inputs;
static uint8_t key_level = 1u;
static uint8_t link_busy;
static uint32_t sends, adc_requests, optical_calls, oled_calls, osd_calls;
static uint32_t calibration_collects, calibration_saves, pauses, resumes;
static uint16_t servo_pulse;
static InterMcuSensorData latest_packet;
float QMC5883P_Yaw = 10.0f;
void SlaveBoard_InitInterrupts(void) {}
void SlaveBoard_Init(void) { inputs.servo_changed = 1u; }
void SlaveBoard_StartTick(void) {}
void SlaveBoard_FeedWatchdog(void) {}
void SlaveBoard_SetServo(uint16_t pulse) {servo_pulse = pulse;}
void SlaveBoard_SetLowBattery(uint8_t low) {(void)low;}
void SlaveBoard_RequestBattery(uint32_t now) {(void)now;adc_requests++;}
void SlaveBoard_ProcessBattery(uint32_t now) {(void)now;}
float SlaveBoard_BatteryVoltage(void) {return 11.1f;}
uint8_t SlaveBoard_BatteryValid(void) {return 1u;}
void SlaveLink_Init(void) {}
void SlaveLink_Process(uint32_t now) {(void)now;}
uint8_t SlaveLink_IsBusy(void) {return link_busy;}
uint8_t SlaveLink_Send(const InterMcuSensorData *packet, uint32_t now)
{
    (void)now;
    latest_packet = *packet;
    sends++;
    return link_busy == 0u;
}
void EXTI_Inputs_Take(SlaveInputEvents *events) {*events=inputs;memset(&inputs,0,sizeof(inputs));}
uint8_t EXTI_ServoLevel(void) {return 1u;}
uint8_t EXTI_KeyLevel(void) {return key_level;}
void OpticalFlow_Init(void) {}
void OpticalFlow_Process(uint32_t now) {(void)now;optical_calls++;}
void OpticalFlow_GetSnapshot(OpticalFlow_Data_t *data) {memset(data,0,sizeof(*data));}
void OpticalFlow_Pause(void) {pauses++;}
void OpticalFlow_Resume(void) {resumes++;}
uint8_t QMC5883P_Init(void) {return 0u;}
uint8_t QMC5883P_UpdateYaw(void) {return 0u;}
void QMC5883P_Calibrate_Start(void) {}
void QMC5883P_Calibrate_Collect(void) {calibration_collects++;}
uint8_t QMC5883P_Calibrate_End(void) {calibration_saves++;return 0u;}
void SPI1_Init(void) {}
int8_t bmp3_spi_read(uint8_t reg,uint8_t *data,uint32_t len,void *ctx)
{(void)reg;(void)data;(void)len;(void)ctx;return 0;}
int8_t bmp3_spi_write(uint8_t reg,const uint8_t *data,uint32_t len,void *ctx)
{(void)reg;(void)data;(void)len;(void)ctx;return 0;}
void BMP390_DelayUs(uint32_t period,void *ctx) {(void)period;(void)ctx;}
void Delay_ms(uint32_t ms) {(void)ms;}
int8_t bmp3_init(struct bmp3_dev *dev) {(void)dev;return BMP3_OK;}
int8_t bmp3_get_sensor_settings(struct bmp3_settings *settings, struct bmp3_dev *dev)
{(void)dev;memset(settings,0,sizeof(*settings));return BMP3_OK;}
int8_t bmp3_set_sensor_settings(uint32_t desired,struct bmp3_settings *settings,struct bmp3_dev *dev)
{(void)desired;(void)settings;(void)dev;return BMP3_OK;}
int8_t bmp3_set_op_mode(struct bmp3_settings *settings,struct bmp3_dev *dev)
{(void)settings;(void)dev;return BMP3_OK;}
int8_t bmp3_get_sensor_data(uint8_t selection,struct bmp3_data *data,struct bmp3_dev *dev)
{(void)selection;(void)dev;data->pressure=101325.0;data->temperature=25.0;return BMP3_OK;}
void OLED_Init(void) {}
void OLED_Clear(void) {}
void OLED_ShowChar(uint8_t row,uint8_t column,char character)
{assert(row>=1u&&row<=4u&&column>=1u&&column<=16u);assert(character>=' ');oled_calls++;}
void AT7456E_Init(void) {}
void OSD_Init(void) {}
void OSD_DisplayInt(uint8_t row,uint8_t column,uint8_t length,int32_t number)
{(void)row;(void)column;(void)length;(void)number;osd_calls++;}
void OSD_DisplayFloat(uint8_t row,uint8_t column,uint8_t digits,uint8_t fraction,float number)
{(void)row;(void)column;(void)digits;(void)fraction;(void)number;osd_calls++;}
static void RunOneMillisecond(void)
{
    uint32_t before_oled = oled_calls;
    uint32_t before_osd = osd_calls;
    uint32_t before_optical = optical_calls;
    SlaveScheduler_TickFromIsr(1u);
    SlaveApp_Process();
    assert(oled_calls - before_oled + osd_calls - before_osd <= 1u);
    assert(optical_calls == before_optical + 1u);
}
int main(void)
{
    uint32_t i;
    SlaveAppStats stats;
    SlaveApp_Init();
    for(i=0u;i<1000u;++i) RunOneMillisecond();
    assert(sends==20u && adc_requests==20u && osd_calls==17u);
    assert(servo_pulse==SLAVE_SERVO_HIGH_US);
    assert(oled_calls<=64u);
    /* A key bounce shorter than 20ms must not be accepted. */
    inputs.key=1u;key_level=0u;RunOneMillisecond();
    for(i=0u;i<10u;++i) RunOneMillisecond();
    key_level=1u;
    for(i=0u;i<20u;++i) RunOneMillisecond();
    /* Capture calibration once; reads remain on the 50ms task. */
    inputs.calibration_pulse=1u;RunOneMillisecond();
    for(i=0u;i<9999u;++i) RunOneMillisecond();
    assert(calibration_saves==0u && calibration_collects==200u);
    link_busy=1u;
    RunOneMillisecond();
    assert(calibration_saves==0u);
    assert((latest_packet.flags & INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING)!=0u);
    link_busy=0u;
    RunOneMillisecond();
    assert(calibration_saves==1u && pauses==1u && resumes==1u);
    for(i=0u;i<50u;++i) RunOneMillisecond();
    assert((latest_packet.flags & INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING)==0u);
    SlaveApp_GetStats(&stats);
    assert(stats.calibration_saved==1u && stats.calibration_errors==0u);
    assert(stats.display_restarts==0u);
    puts("slave application tests passed");
    return 0;
}
