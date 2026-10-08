#include "slave_app.h"
#include "slave_scheduler.h"
#include "slave_board.h"
#include "master_link.h"
#include "optical_flow.h"
#include "board_inputs.h"
#include "bmp3.h"
#include "bmp390.h"
#include "qmc5883p.h"
#include "oled.h"
#include "at7456e.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static board_input_events_t inputs;
static uint8_t key_level = 1u;
static uint8_t link_busy;
static uint32_t sends, adc_requests, optical_calls, oled_calls, osd_calls;
static uint32_t calibration_collects, calibration_saves, pauses, resumes;
static uint16_t servo_pulse;
static inter_mcu_sensor_data_t latest_packet;
float qmc5883p_yaw_deg = 10.0f;
void slave_board_init_interrupts(void) {}
void slave_board_init(void) { inputs.servo_changed = 1u; }
void slave_board_start_tick(void) {}
void slave_board_feed_watchdog(void) {}
void slave_board_set_servo(uint16_t pulse) {servo_pulse = pulse;}
void slave_board_set_low_battery(uint8_t low) {(void)low;}
void slave_board_request_battery(uint32_t now) {(void)now;adc_requests++;}
void slave_board_process_battery(uint32_t now) {(void)now;}
float slave_board_battery_voltage(void) {return 11.1f;}
uint8_t slave_board_battery_valid(void) {return 1u;}
void master_link_init(void) {}
void master_link_process(uint32_t now) {(void)now;}
uint8_t master_link_is_busy(void) {return link_busy;}
uint8_t master_link_send(const inter_mcu_sensor_data_t *packet, uint32_t now)
{
    (void)now;
    latest_packet = *packet;
    sends++;
    return link_busy == 0u;
}
void board_inputs_take(board_input_events_t *events) {*events=inputs;memset(&inputs,0,sizeof(inputs));}
uint8_t board_inputs_servo_level(void) {return 1u;}
uint8_t board_inputs_key_level(void) {return key_level;}
void optical_flow_init(void) {}
void optical_flow_process(uint32_t now) {(void)now;optical_calls++;}
void optical_flow_get_snapshot(optical_flow_data_t *data) {memset(data,0,sizeof(*data));}
void optical_flow_pause(void) {pauses++;}
void optical_flow_resume(void) {resumes++;}
uint8_t qmc5883p_init(void) {return 0u;}
uint8_t qmc5883p_update_yaw(void) {return 0u;}
void qmc5883p_calibrate_start(void) {}
void qmc5883p_calibrate_collect(void) {calibration_collects++;}
uint8_t qmc5883p_calibrate_end(void) {calibration_saves++;return 0u;}
void bmp390_spi_init(void) {}
int8_t bmp390_spi_read(uint8_t reg,uint8_t *data,uint32_t len,void *ctx)
{(void)reg;(void)data;(void)len;(void)ctx;return 0;}
int8_t bmp390_spi_write(uint8_t reg,const uint8_t *data,uint32_t len,void *ctx)
{(void)reg;(void)data;(void)len;(void)ctx;return 0;}
void bmp390_delay_us(uint32_t period,void *ctx) {(void)period;(void)ctx;}
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
void oled_init(void) {}
void oled_clear(void) {}
void oled_show_char(uint8_t row,uint8_t column,char character)
{assert(row>=1u&&row<=4u&&column>=1u&&column<=16u);assert(character>=' ');oled_calls++;}
void at7456e_init(void) {}
void osd_init(void) {}
void osd_display_int_padded(uint8_t row,uint8_t column,uint8_t length,int32_t number)
{(void)row;(void)column;(void)length;(void)number;osd_calls++;}
void osd_display_float(uint8_t row,uint8_t column,uint8_t digits,uint8_t fraction,float number)
{(void)row;(void)column;(void)digits;(void)fraction;(void)number;osd_calls++;}
static void RunOneMillisecond(void)
{
    uint32_t before_oled = oled_calls;
    uint32_t before_osd = osd_calls;
    uint32_t before_optical = optical_calls;
    slave_scheduler_tick_from_isr(1u);
    slave_app_process();
    assert(oled_calls - before_oled + osd_calls - before_osd <= 1u);
    assert(optical_calls == before_optical + 1u);
}
int main(void)
{
    uint32_t i;
    slave_app_stats_t stats;
    slave_app_init();
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
    slave_app_get_stats(&stats);
    assert(stats.calibration_saved==1u && stats.calibration_errors==0u);
    assert(stats.display_restarts==0u);
    puts("slave application tests passed");
    return 0;
}
