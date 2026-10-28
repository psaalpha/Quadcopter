#include "flight_controller.h"
#include "flight_safety.h"
#include "app_scheduler.h"
#include "slave_link.h"
#include "board_config.h"
#include "inter_mcu_protocol.h"
#include "stm32f10x.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
volatile uint32_t system_time_ms;
TIM_TypeDef fake_tim4;
int16_t rc_channels[16];volatile uint8_t crsf_frame_received;
slave_sensor_data_t slave_sensor_data;
static uint8_t imu_valid=1u,source_enabled=1u,ready[APP_TASK_COUNT],saved;
static uint16_t motors[4],sequence,voltage=12000u;
static uint32_t flow_us;
static int16_t accel_x,accel_z=2048;
static uint8_t image[FLIGHT_PARAMETERS_BYTES];
void board_led_init(void){}
void board_led1_on(void){} void board_led1_off(void){}
void board_led2_on(void){} void board_led2_off(void){}
void board_led3_on(void){} void board_led3_off(void){}
void motor_pwm_init(void){}
void motor_pwm_set_minimum_output(void){unsigned i;for(i=0;i<4;i++)motors[i]=500u;}
void motor_pwm_set_compare1(uint16_t v){motors[0]=v;}
void motor_pwm_set_compare2(uint16_t v){motors[1]=v;}
void motor_pwm_set_compare3(uint16_t v){motors[2]=v;}
void motor_pwm_set_compare4(uint16_t v){motors[3]=v;}
void TIM_UpdateDisableConfig(TIM_TypeDef *t,FunctionalState s){(void)t;(void)s;}
void mpu6050_init(void){}
uint8_t mpu6050_last_read_ok(void){return imu_valid;}
uint8_t mpu6050_is_ready(void){return imu_valid;}
void mpu6050_get_data(int16_t *ax,int16_t *ay,int16_t *az,int16_t *gx,int16_t *gy,int16_t *gz)
{*ax=accel_x;*ay=0;*az=accel_z;*gx=-64;*gy=-29;*gz=-43;}
void kalman_init_roll(void){} void kalman_init_pitch(void){}
float kalman_update_roll(float g,float a,float dt){(void)g;(void)dt;return a;}
float kalman_update_pitch(float g,float a,float dt){(void)g;(void)dt;return a;}
void crsf_init(void){} void crsf_process(void){}
void slave_link_init(void){} void slave_link_process(void){}
void app_scheduler_init(void){memset(ready,0,sizeof(ready));}
uint8_t app_scheduler_take(app_task_id_t task){uint8_t p=ready[task];ready[task]=0;return p;}
int flash_config_load(uint16_t schema,void *data,unsigned n)
{assert(schema==0x1001u && n==sizeof(image));if(!saved)return 0;memcpy(data,image,n);return 1;}
int flash_config_save(uint16_t schema,const void *data,unsigned n)
{assert(schema==0x1001u && n==sizeof(image));memcpy(image,data,n);saved=1u;return 1;}
static void tick(unsigned ms)
{
    unsigned i;
    for(i=0;i<ms;i++) {
        system_time_ms++;
        ready[APP_TASK_IMU_UPDATE]=(uint8_t)(system_time_ms%2u==0u);
        ready[APP_TASK_RC_SERVICE]=(uint8_t)(system_time_ms%5u==0u);
        ready[APP_TASK_ANGLE_CONTROL]=(uint8_t)(system_time_ms%10u==0u);
        crsf_frame_received=1u;
        if(source_enabled && system_time_ms%50u==0u) {
            slave_sensor_data.sequence=++sequence;slave_sensor_data.timestamp_ms=system_time_ms;
            slave_sensor_data.status_flags=INTER_MCU_SENSOR_FLAG_BARO_VALID|INTER_MCU_SENSOR_FLAG_FLOW_VALID|
                INTER_MCU_SENSOR_FLAG_FLOW_CHECKED|INTER_MCU_SENSOR_FLAG_MAG_VALID|INTER_MCU_SENSOR_FLAG_MAG_RAW_VALID|
                INTER_MCU_SENSOR_FLAG_BATTERY_VALID;
            slave_sensor_data.battery_mv=voltage;slave_sensor_data.flow_distance=1000u;
            slave_sensor_data.flow_quality=100u;slave_sensor_data.mag_x=1000;slave_sensor_data.mag_y=0;
            slave_sensor_data.mag_z=300;slave_sensor_data.flow_integration_us=(flow_us+=50000u);
            slave_sensor_data.updated=1u;
        }
        flight_controller_step();flight_controller_finish_maintenance();
    }
}
static void command(const char *text,const char *expected)
{
    flight_command_t c;char response[128];
    assert(flight_command_parse(text,&c));flight_controller_command(&c,response,sizeof(response));
    if(strstr(response,expected)==0)fprintf(stderr,"%s -> %s (expected %s)\n",text,response,expected);
    assert(strstr(response,expected)!=0);
}
static unsigned state(void)
{
    char t[128];flight_controller_telemetry(t,sizeof(t),1);
    return (unsigned)strtoul(t+7,0,10);
}
static unsigned health(void)
{
    char t[128];unsigned s,b,v,h;
    flight_controller_telemetry(t,sizeof(t),1);
    assert(sscanf(t,"[state,%u,%u,%u,%u",&s,&b,&v,&h)==4);return h;
}
static void stop(void)
{rc_channels[BOARD_RC_CHANNEL_ARM]=1000;rc_channels[BOARD_RC_CHANNEL_THROTTLE]=1000;tick(10);assert(motors[0]==500);}
static void arm(void)
{
    rc_channels[BOARD_RC_CHANNEL_ARM]=2000;tick(10);
    if(state()!=FLIGHT_SAFETY_ACTIVE)fprintf(stderr,"arm rejected at %lu state %u health %u\n",(unsigned long)system_time_ms,state(),health());
    assert(state()==FLIGHT_SAFETY_ACTIVE);
}
int main(void)
{
    unsigned i;for(i=0;i<16;i++)rc_channels[i]=1500;
    rc_channels[BOARD_RC_CHANNEL_THROTTLE]=1000;rc_channels[BOARD_RC_CHANNEL_ARM]=1000;
    rc_channels[BOARD_RC_CHANNEL_MAG]=1000;rc_channels[BOARD_RC_CHANNEL_NAV_MODE]=1000;
    flight_controller_init();tick(1000);assert(state()==FLIGHT_SAFETY_DISARMED);
    command("slider,PAKp,9","value,PAKp,9.000");command("get,PAKp","9.000");
    accel_x=-1024;accel_z=1774;tick(100);
    rc_channels[BOARD_RC_CHANNEL_ARM]=2000;tick(10);assert(state()!=FLIGHT_SAFETY_ACTIVE);
    accel_x=0;accel_z=2048;tick(100);assert(state()!=FLIGHT_SAFETY_ACTIVE);stop();
    arm();rc_channels[BOARD_RC_CHANNEL_THROTTLE]=1200;tick(20);assert(motors[0]>500);
    command("save","disarm_required");command("set,Contrl_Speed,10","rejected");
    voltage=9300;tick(100);assert(state()==FLIGHT_SAFETY_ACTIVE); /* Warning doesn't stop airborne motors. */
    stop();rc_channels[BOARD_RC_CHANNEL_ARM]=2000;tick(10);assert(state()!=FLIGHT_SAFETY_ACTIVE);
    voltage=12000;tick(300);assert(state()!=FLIGHT_SAFETY_ACTIVE); /* No delayed automatic arming. */
    stop();command("save","ok");assert(saved);command("slider,PAKp,7","7.000");command("load","ok");
    command("get,PAKp","9.000");command("set,PAIM,3","3.000");command("load","ok");command("get,PAIM","0.000");
    command("set,AKp,0.5","value");command("set,XKp,0.1","value");command("set,YKpPos,0.1","value");
    command("set,NavEnable,1","value");arm();rc_channels[BOARD_RC_CHANNEL_THROTTLE]=1200;
    rc_channels[BOARD_RC_CHANNEL_NAV_MODE]=2000;tick(200);
    assert(state()==FLIGHT_SAFETY_ACTIVE && (health()&4u));
    source_enabled=0;slave_sensor_data.updated=1u;flight_controller_step();tick(250);
    assert(state()==FLIGHT_SAFETY_ACTIVE && !(health()&4u));
    slave_sensor_data.updated=1u;flight_controller_step(); /* Exact replay after timeout stays stale. */
    assert(!(health()&2u));
    source_enabled=1;tick(150);
    imu_valid=0;tick(10);assert(state()==FLIGHT_SAFETY_SENSOR_FAULT && motors[0]==500);
    imu_valid=1;tick(500);assert(state()!=FLIGHT_SAFETY_ACTIVE);
    rc_channels[BOARD_RC_CHANNEL_NAV_MODE]=1000;stop();
    command("bench,1","ok");command("set,Contrl_Speed,10","value");tick(10);arm();tick(10);
    for(i=0;i<4;i++)assert(motors[i]==550u);
    tick(1001);assert(state()==FLIGHT_SAFETY_DISARMED && motors[0]==500);
    command("calibrate,gyro","ok");tick(600);
    {
        char out[128];unsigned s,b,v,h,dirty,cal;
        flight_controller_telemetry(out,sizeof(out),1);
        assert(sscanf(out,"[state,%u,%u,%u,%u,%u,%u",&s,&b,&v,&h,&dirty,&cal)==6 && cal==2u);
    }
    source_enabled=0;tick(300);stop();rc_channels[BOARD_RC_CHANNEL_ARM]=2000;tick(20);
    assert(state()!=FLIGHT_SAFETY_ACTIVE && motors[0]==500);
    return 0;
}
