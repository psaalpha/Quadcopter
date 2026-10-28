#include "flight_controller.h"
#include "flight_safety.h"
#include "app_scheduler.h"
#include "board_config.h"
#include "mpu6050.h"
#include "attitude_estimator.h"
#include "kalman_filter.h"
#include "pid_controller.h"
#include "navigation_estimator.h"
#include "crsf.h"
#include "slave_link.h"
#include "board_led.h"
#include "motor_pwm.h"
#include "flash_config.h"
#include "inter_mcu_protocol.h"
#include "stm32f10x.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
extern volatile uint32_t system_time_ms;
extern float roll_mount_offset_deg,pitch_mount_offset_deg,yaw_mount_offset_deg;
static flight_parameters_t parameters;
static flight_safety_context_t safety;
static navigation_estimator_t navigation;
static float roll,pitch,yaw,roll_rate,pitch_rate,yaw_rate,yaw_target;
static int16_t roll_command,pitch_command,yaw_command;
static uint8_t throttle,servo,calibration_switch,nav_requested,nav_active,yaw_ready;
static uint8_t imu_failures,imu_good,gyro_cal_reported,bench_enabled;
static uint8_t source_initialized,battery_level;
static uint16_t source_sequence,last_flags,battery_mv;
static uint16_t motor_compare[4]={500u,500u,500u,500u};
static uint32_t source_timestamp,last_slave_ms,last_imu_ms,last_imu_retry,last_bench_ms;
static uint32_t duplicate_packets,calibration_hold_until;
static uint32_t reset_flags;
static volatile uint8_t maintenance;
static float clamp(float x,float low,float high){return x<low?low:x>high?high:x;}
static float wrap(float x){while(x>=360)x-=360;while(x<0)x+=360;return x;}
static int32_t fixed(float x,float scale)
{return x==x?(int32_t)(clamp(x,-1000000,1000000)*scale):0;}
static uint8_t armed(void){return flight_safety_motors_allowed(&safety);}
static uint8_t slave_fresh(void)
{return (uint8_t)(source_initialized && (uint32_t)(system_time_ms-last_slave_ms)<200u);}
static void stop_navigation(void)
{nav_active=0u;pid_reset_navigation();navigation_estimator_reset(&navigation);}
static void hold_safe(void)
{
    unsigned i;
    for(i=0u;i<4u;i++)motor_compare[i]=500u;
    stop_navigation();yaw_ready=0u;pid_stop_motors();motor_pwm_set_minimum_output();
}
void flight_controller_emergency_stop(void)
{unsigned i;for(i=0u;i<4u;i++)motor_compare[i]=500u;motor_pwm_set_minimum_output();}
uint32_t flight_controller_last_imu_step(void){return last_imu_ms;}
uint8_t flight_controller_maintenance(void){return maintenance;}
void flight_controller_set_reset_flags(uint32_t flags){reset_flags=flags;}
void flight_controller_finish_maintenance(void)
{
    if(maintenance){app_scheduler_init();last_imu_ms=system_time_ms;maintenance=0u;}
}
static void apply_parameters(void)
{
    const float *v=parameters.value;
    pid_set_pitch_kp(v[FP_PKP]);pid_set_pitch_ki(v[FP_PKI]);pid_set_pitch_kd(v[FP_PKD]*.01f);
    pid_set_roll_kp(v[FP_RKP]);pid_set_roll_ki(v[FP_RKI]);pid_set_roll_kd(v[FP_RKD]*.01f);
    pid_set_yaw_kp(v[FP_YKP]);pid_set_yaw_ki(v[FP_YKI]);pid_set_yaw_kd(v[FP_YKD]);
    pid_set_pitch_angle_kp(v[FP_PAKP]);pid_set_roll_angle_kp(v[FP_RAKP]);pid_set_yaw_angle_kp(v[FP_YAKP]);
    pid_set_altitude_kp(v[FP_AKP]);pid_set_altitude_ki(v[FP_AKI]);pid_set_altitude_kd(v[FP_AKD]);
    pid_set_position_x_kp(v[FP_XKP]);pid_set_position_x_ki(v[FP_XKI]);pid_set_position_x_kd(v[FP_XKD]);
    pid_set_position_y_kp(v[FP_YXKP]);pid_set_position_y_ki(v[FP_YXKI]);pid_set_position_y_kd(v[FP_YXKD]);
    roll_mount_offset_deg=v[FP_ROLL_OFFSET];pitch_mount_offset_deg=v[FP_PITCH_OFFSET];
    yaw_mount_offset_deg=v[FP_DECLINATION];
    attitude_estimator_set_bias(v[FP_GYRO_X],v[FP_GYRO_Y],v[FP_GYRO_Z]);
}
void flight_controller_init(void)
{
    uint8_t image[FLIGHT_PARAMETERS_BYTES];
    flight_parameters_defaults(&parameters);
    if(flash_config_load(0x1001u,image,sizeof(image)))flight_parameters_decode(&parameters,image,sizeof(image));
    flight_safety_init(&safety);navigation_estimator_reset(&navigation);
    board_led_init();mpu6050_init();kalman_init_roll();kalman_init_pitch();
    crsf_init();slave_link_init();motor_pwm_init();apply_parameters();hold_safe();
}
static void apply_targets(void)
{
    float duty=(float)throttle,rt=(float)roll_command*.1f+parameters.value[FP_RAIM];
    float pt=(float)pitch_command*.1f+parameters.value[FP_PAIM];
    if(!armed())return;
    if(bench_enabled)duty=0.0f;
    if(nav_active) {
        float a=yaw*.01745329252f,wx=pid_get_position_roll_target(),wy=pid_get_position_pitch_target();
        duty=parameters.value[FP_HOVER]+pid_get_altitude_output();
        rt+=wx*cosf(a)+wy*sinf(a);pt+=-wx*sinf(a)+wy*cosf(a);
    }
    pid_set_base_duty(clamp(duty,0,100));pid_set_roll_target(clamp(rt,-30,30));pid_set_pitch_target(clamp(pt,-30,30));
}
static void service_rc(void)
{
    uint8_t arm,preflight;
    crsf_process();if(!crsf_frame_received)return;crsf_frame_received=0u;
    roll_command=(int16_t)clamp(((float)rc_channels[BOARD_RC_CHANNEL_ROLL]-1500-parameters.value[FP_MID])/5,-100,100);
    pitch_command=(int16_t)clamp(((float)rc_channels[BOARD_RC_CHANNEL_PITCH]-1500-parameters.value[FP_MID])/5,-100,100);
    yaw_command=(int16_t)clamp(((float)rc_channels[BOARD_RC_CHANNEL_YAW]-1500)*1.8f,-900,900);
    throttle=(uint8_t)clamp(((float)rc_channels[BOARD_RC_CHANNEL_THROTTLE]-1000)/10,0,100);
    servo=(uint8_t)(rc_channels[BOARD_RC_CHANNEL_SERVO]>1500);
    calibration_switch=(uint8_t)(rc_channels[BOARD_RC_CHANNEL_MAG]>1500);
    nav_requested=(uint8_t)(rc_channels[BOARD_RC_CHANNEL_NAV_MODE]>1500);
    arm=rc_channels[BOARD_RC_CHANNEL_ARM]>1700?1u:rc_channels[BOARD_RC_CHANNEL_ARM]<1300?0u:2u;
    if(calibration_switch && !armed())calibration_hold_until=system_time_ms+21000u;
    preflight=(uint8_t)(imu_good>=100u && imu_failures==0u &&
        attitude_estimator_gravity_ok() && fabsf(roll)<=15 && fabsf(pitch)<=15 &&
        fabsf(roll_rate)<10 && fabsf(pitch_rate)<10 && fabsf(yaw_rate)<10 &&
        attitude_estimator_calibration_status()!=1u && slave_fresh() &&
        (last_flags&INTER_MCU_SENSOR_FLAG_BATTERY_VALID)!=0u && battery_level==0u &&
        (last_flags&INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING)==0u &&
        (calibration_hold_until==0u || (int32_t)(system_time_ms-calibration_hold_until)>=0));
    flight_safety_on_valid_rc_frame(&safety,system_time_ms,throttle,BOARD_RC_THROTTLE_UNLOCK_PERCENT,arm,preflight);
    if(!armed()){hold_safe();return;}
    if(!nav_requested || throttle<BOARD_NAV_MIN_THROTTLE_PERCENT || bench_enabled)stop_navigation();
    apply_targets();
}
static void run_imu(void)
{
    uint8_t status;last_imu_ms=system_time_ms;
    if(!attitude_estimator_update()) {
        imu_good=0u;if(imu_failures<255u)imu_failures++;
        if(imu_failures>=3u){flight_safety_latch_fault(&safety);hold_safe();}
        return;
    }
    imu_failures=0u;if(imu_good<100u)imu_good++;
    attitude_estimator_get_rates(&roll_rate,&pitch_rate,&yaw_rate);
    navigation_estimator_gyro(&navigation,roll_rate,pitch_rate,.002f);
    status=attitude_estimator_calibration_status();
    if(status==2u && !gyro_cal_reported) {
        float x,y,z;attitude_estimator_get_bias(&x,&y,&z);
        flight_parameter_set(&parameters,FP_GYRO_X,x);flight_parameter_set(&parameters,FP_GYRO_Y,y);
        flight_parameter_set(&parameters,FP_GYRO_Z,z);gyro_cal_reported=1u;
    }
    pid_update_rate_loop(roll_rate,pitch_rate,yaw_rate);
    if(!armed()){motor_pwm_set_minimum_output();return;}
    if(bench_enabled) {
        unsigned i;
        uint16_t compare=(uint16_t)(500.0f+clamp(parameters.value[FP_THROTTLE],0,20)*5.0f);
        for(i=0u;i<4u;i++)motor_compare[i]=compare;
    } else {
        motor_compare[0]=pid_get_motor_compare_back_left();motor_compare[1]=pid_get_motor_compare_front_right();
        motor_compare[2]=pid_get_motor_compare_front_left();motor_compare[3]=pid_get_motor_compare_back_right();
    }
    TIM_UpdateDisableConfig(TIM4,ENABLE);
    motor_pwm_set_compare3(motor_compare[2]);motor_pwm_set_compare2(motor_compare[1]);
    motor_pwm_set_compare4(motor_compare[3]);motor_pwm_set_compare1(motor_compare[0]);
    TIM_UpdateDisableConfig(TIM4,DISABLE);
}
static void run_angle(void)
{
    attitude_estimator_get_angles(&roll,&pitch,&yaw);
    if(armed()) {
        if(!yaw_ready){yaw_target=yaw;yaw_ready=1u;}
        if(yaw_command>BOARD_RC_YAW_DEADBAND || yaw_command< -BOARD_RC_YAW_DEADBAND)
            yaw_target=wrap(yaw_target+(float)yaw_command*.001f);
        pid_set_yaw_target(yaw_target);
    }
    pid_update_angle_loop(roll,pitch,yaw);
}
static void refresh_slave(void)
{
    uint8_t motion;uint16_t step,previous_flags=last_flags;uint32_t source_delta;float dt;
    if(!slave_sensor_data.updated)return;
    slave_sensor_data.updated=0u;
    step=(uint16_t)(slave_sensor_data.sequence-source_sequence);
    source_delta=slave_sensor_data.timestamp_ms-source_timestamp;
    if(source_initialized && (step==0u || source_delta==0u ||
       ((step>=0x8000u || source_delta>=0x80000000u) &&
        ((uint32_t)(system_time_ms-last_slave_ms)<200u ||
         slave_sensor_data.sequence>20u || slave_sensor_data.timestamp_ms>1000u)))) {
        duplicate_packets++;return;
    }
    dt=source_initialized?(float)source_delta*.001f:.05f;
    source_initialized=1u;source_sequence=slave_sensor_data.sequence;source_timestamp=slave_sensor_data.timestamp_ms;
    last_slave_ms=system_time_ms;last_flags=slave_sensor_data.status_flags;battery_mv=slave_sensor_data.battery_mv;
    if(!armed() && (previous_flags&INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING) &&
       !(last_flags&INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING)) {
        attitude_estimator_reset_yaw();yaw_ready=0u;
    }
    battery_level=2u;
    if(last_flags&INTER_MCU_SENSOR_FLAG_BATTERY_VALID)
        battery_level=battery_mv<(uint16_t)(parameters.value[FP_BAT_CRIT]*1000)?2u:
                      battery_mv<(uint16_t)(parameters.value[FP_BAT_WARN]*1000)?1u:0u;
    if((last_flags&(INTER_MCU_SENSOR_FLAG_MAG_RAW_VALID|INTER_MCU_SENSOR_FLAG_MAG_VALID))==
        (INTER_MCU_SENSOR_FLAG_MAG_RAW_VALID|INTER_MCU_SENSOR_FLAG_MAG_VALID) &&
        !(last_flags&INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING)) {
        if(attitude_estimator_apply_mag_vector((float)slave_sensor_data.mag_x,(float)slave_sensor_data.mag_y,
            (float)slave_sensor_data.mag_z)==1u) {
            attitude_estimator_get_angles(&roll,&pitch,&yaw);yaw_target=yaw;pid_set_yaw_target(yaw_target);yaw_ready=1u;
        }
    }
    if(!armed() || !nav_requested || bench_enabled || parameters.value[FP_NAV_ENABLE]!=1 ||
       throttle<BOARD_NAV_MIN_THROTTLE_PERCENT ||
       (last_flags&(INTER_MCU_SENSOR_FLAG_BARO_VALID|INTER_MCU_SENSOR_FLAG_FLOW_VALID|INTER_MCU_SENSOR_FLAG_FLOW_CHECKED))!=
       (INTER_MCU_SENSOR_FLAG_BARO_VALID|INTER_MCU_SENSOR_FLAG_FLOW_VALID|INTER_MCU_SENSOR_FLAG_FLOW_CHECKED) ||
       slave_sensor_data.flow_quality<BOARD_NAV_FLOW_QUALITY_MIN ||
       slave_sensor_data.flow_distance<BOARD_NAV_DISTANCE_MIN_MM ||
       slave_sensor_data.flow_distance>BOARD_NAV_DISTANCE_MAX_MM) {stop_navigation();apply_targets();return;}
    motion=(uint8_t)navigation_estimator_update(&navigation,slave_sensor_data.flow_x,slave_sensor_data.flow_y,
        slave_sensor_data.flow_integration_us,slave_sensor_data.flow_epoch,slave_sensor_data.flow_distance,
        roll,pitch,yaw,parameters.value[FP_FLOW_X_SCALE],parameters.value[FP_FLOW_Y_SCALE],parameters.value[FP_FLOW_YAW]);
    if(!motion){nav_active=0u;pid_reset_navigation();apply_targets();return;}
    if(!nav_active) {
        pid_set_altitude_target(navigation.height_cm);
        pid_set_position_target((int32_t)navigation.x_mm,(int32_t)navigation.y_mm);
        pid_reset_navigation();nav_active=1u;
    } else if(dt>0 && dt<.2f) {
        pid_update_navigation(navigation.height_cm,(int32_t)navigation.x_mm,(int32_t)navigation.y_mm,dt);
    } else stop_navigation();
    apply_targets();
}
void flight_controller_step(void)
{
    if(flight_safety_check_timeout(&safety,system_time_ms,BOARD_RC_FAILSAFE_TIMEOUT_MS)) {
        bench_enabled=servo=calibration_switch=0u;hold_safe();
    }
    if(app_scheduler_take(APP_TASK_RC_SERVICE))service_rc();
    if(app_scheduler_take(APP_TASK_IMU_UPDATE))run_imu();
    if(app_scheduler_take(APP_TASK_ANGLE_CONTROL))run_angle();
    slave_link_process();refresh_slave();
    if(!slave_fresh()){battery_level=2u;if(nav_active){stop_navigation();apply_targets();}}
    if(bench_enabled && ((uint32_t)(system_time_ms-last_bench_ms)>=1000u ||
       !slave_fresh() || throttle>5u || nav_requested)) {
        bench_enabled=0u;parameters.value[FP_THROTTLE]=0;flight_safety_disarm(&safety);hold_safe();
    }
    if(!armed() && (!mpu6050_is_ready() || imu_failures>=3u) && (uint32_t)(system_time_ms-last_imu_retry)>=1000u) {
        last_imu_retry=system_time_ms;maintenance=1u;hold_safe();mpu6050_init();
    }
    if(servo)board_led2_on();else board_led2_off();
    if(calibration_switch && !armed() && throttle<=5u)board_led3_on();else board_led3_off();
}

void flight_controller_command(const flight_command_t *c,char *out,unsigned capacity)
{
    const flight_parameter_info_t *info;
    uint8_t image[FLIGHT_PARAMETERS_BYTES];
    int ok=0;
    if(c==0 || out==0 || capacity==0u)return;
    if(c->op==FC_GET || c->op==FC_SET) {
        info=flight_parameter_info(c->id);
        if(info==0){snprintf(out,capacity,"[error,unknown_parameter]");return;}
        if(c->op==FC_GET)ok=1;
        else if(c->id==FP_THROTTLE) {
            if(bench_enabled && throttle<=5u && !nav_requested && safety.link_ok) {
                ok=flight_parameter_set(&parameters,c->id,c->value);last_bench_ms=system_time_ms;
            }
        } else if((c->id<FP_FLOW_X_SCALE || !armed()) &&
                  !(armed() && c->id==FP_NAV_ENABLE && c->value!=0)) {
            ok=flight_parameter_set(&parameters,c->id,c->value);
        }
        if(ok) {
            long value=(long)(parameters.value[c->id]*1000000.0f);
            unsigned long magnitude=(unsigned long)(value<0?-value:value);
            if(c->op==FC_SET){
                if(c->id==FP_NAV_ENABLE && c->value==0)stop_navigation();
                apply_parameters();apply_targets();
            }
            snprintf(out,capacity,"[value,%s,%s%lu.%06lu]",info->name,value<0?"-":"",magnitude/1000000u,magnitude%1000000u);
        } else snprintf(out,capacity,"[error,rejected,%s]",info->name);
        return;
    }
    if(c->op==FC_STATUS){flight_controller_telemetry(out,capacity,1u);return;}
    if(armed() || throttle>5u){snprintf(out,capacity,"[error,disarm_required]");return;}
    switch(c->op) {
    case FC_SAVE:
        maintenance=1u;hold_safe();flight_parameters_encode(&parameters,image);
        ok=flash_config_save(0x1001u,image,sizeof(image));if(ok)parameters.dirty=0u;break;
    case FC_LOAD:
        ok=flash_config_load(0x1001u,image,sizeof(image)) && flight_parameters_decode(&parameters,image,sizeof(image));
        if(ok){bench_enabled=0u;stop_navigation();apply_parameters();}break;
    case FC_DEFAULTS:
        flight_parameters_defaults(&parameters);parameters.dirty=1u;bench_enabled=0u;stop_navigation();apply_parameters();ok=1;break;
    case FC_GYRO_CAL:
        if(imu_good>=100u){attitude_estimator_calibrate_start();gyro_cal_reported=0u;ok=1;}break;
    case FC_YAW_ZERO:
        attitude_estimator_zero_heading();yaw_ready=0u;ok=1;break;
    case FC_BENCH:
        if(c->value==1 && (!safety.link_ok || nav_requested || !slave_fresh() || imu_good<100u))break;
        bench_enabled=(uint8_t)(c->value==1);last_bench_ms=system_time_ms;
        parameters.value[FP_THROTTLE]=0;flight_safety_disarm(&safety);hold_safe();ok=1;break;
    default: break;
    }
    snprintf(out,capacity,"[%s,%u]",ok?"ok":"error",(unsigned)c->op);
}

void flight_controller_telemetry(char *out,unsigned capacity,unsigned page)
{
    uint8_t health=(uint8_t)((imu_good>=100u?1u:0u)|(slave_fresh()?2u:0u)|
        (nav_active?4u:0u)|(bench_enabled?8u:0u));
    switch(page%5u) {
    case 0u: snprintf(out,capacity,"[att,%ld,%ld,%ld,%ld,%ld,%ld]",
        (long)fixed(roll,100),(long)fixed(pitch,100),(long)fixed(yaw,100),
        (long)fixed(roll_rate,100),(long)fixed(pitch_rate,100),(long)fixed(yaw_rate,100));break;
    case 1u: snprintf(out,capacity,"[state,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu]",
        (unsigned)safety.state,(unsigned)battery_mv,(unsigned)battery_level,(unsigned)health,
        (unsigned)parameters.dirty,(unsigned)attitude_estimator_calibration_status(),
        (unsigned long)attitude_estimator_mag_rejections(),(unsigned long)duplicate_packets,
        (unsigned long)reset_flags);break;
    case 2u: snprintf(out,capacity,"[motor,%u,%u,%u,%u]",(unsigned)motor_compare[2],
        (unsigned)motor_compare[1],(unsigned)motor_compare[0],(unsigned)motor_compare[3]);break;
    case 3u: snprintf(out,capacity,"[nav,%ld,%ld,%ld,%ld,%ld]",(long)fixed(navigation.x_mm,.1f),
        (long)fixed(navigation.y_mm,.1f),(long)fixed(navigation.height_cm,1),
        (long)fixed(navigation.velocity_x_cm_s,1),(long)fixed(navigation.velocity_y_cm_s,1));break;
    default: snprintf(out,capacity,"[plot,%ld]",(long)fixed(pitch,10));break;
    }
}
