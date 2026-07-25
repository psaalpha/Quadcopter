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

/* TIM4 ESC output policy. */
#define BOARD_MOTOR_PWM_MIN_COMPARE          500u

#endif
