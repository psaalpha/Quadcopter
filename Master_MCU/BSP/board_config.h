#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* Master control-loop timing. */
#define BOARD_IMU_TASK_PERIOD_MS             2u
#define BOARD_RC_TASK_PERIOD_MS              5u
#define BOARD_ANGLE_TASK_PERIOD_MS           10u
#define BOARD_MOTOR_TASK_PERIOD_MS           20u

/* RC safety policy. */
#define BOARD_RC_FAILSAFE_TIMEOUT_MS         300u
#define BOARD_RC_FAILSAFE_TIMEOUT_TICKS      \
    (BOARD_RC_FAILSAFE_TIMEOUT_MS / BOARD_RC_TASK_PERIOD_MS)
#define BOARD_RC_THROTTLE_UNLOCK_PERCENT     5u

/* CRSF channel assignment. */
#define BOARD_RC_CHANNEL_ROLL                0u
#define BOARD_RC_CHANNEL_PITCH               1u
#define BOARD_RC_CHANNEL_THROTTLE            2u
#define BOARD_RC_CHANNEL_YAW                 3u
#define BOARD_RC_CHANNEL_SERVO               4u
#define BOARD_RC_CHANNEL_MAG                 5u
/* CH6 独立请求高度/光流辅助控制；接收机未配置该通道时保持关闭。 */
#define BOARD_RC_CHANNEL_NAV_MODE            6u
#define BOARD_RC_YAW_DEADBAND                 30   /* 映射后的 ±3°/s 死区 */

/* 辅助控制只在有效新帧、足够油门和有效测距条件下工作。 */
#define BOARD_NAV_MIN_THROTTLE_PERCENT       10u
#define BOARD_NAV_SENSOR_TIMEOUT_MS          200u
#define BOARD_NAV_SENSOR_TIMEOUT_TICKS       \
    (BOARD_NAV_SENSOR_TIMEOUT_MS / BOARD_RC_TASK_PERIOD_MS)
#define BOARD_NAV_FLOW_QUALITY_MIN           20u
#define BOARD_NAV_DISTANCE_MIN_MM            100u
#define BOARD_NAV_DISTANCE_MAX_MM            3000u

/* TIM4 ESC output policy. */
#define BOARD_MOTOR_PWM_MIN_COMPARE          500u

#endif
