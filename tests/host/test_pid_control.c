#include "pid_controller.h"

#include <assert.h>
#include <stdio.h>

static void AssertYawShortestPathAndMixer(void)
{
    pid_stop_motors();
    pid_set_base_duty(50.0f);
    pid_set_yaw_angle_kp(1.0f);
    pid_set_yaw_kp(1.0f);
    pid_set_yaw_ki(0.0f);
    pid_set_yaw_kd(0.0f);
    pid_set_yaw_target(359.0f);

    pid_update_angle_loop(0.0f, 0.0f, 1.0f);
    assert(pid_get_yaw_error() == -2.0f); /* 359°→1° 应走 -2°，不是 +358°。 */
    pid_update_rate_loop(0.0f, 0.0f, 0.0f);
    assert(pid_get_motor_compare_front_left() < pid_get_motor_compare_front_right());

    pid_stop_motors();
    assert(pid_get_motor_compare_front_left() == 500u);
    assert(pid_get_motor_compare_front_right() == 500u);
}

static void AssertNavigationOutputAndReset(void)
{
    pid_stop_motors();
    pid_set_base_duty(50.0f);
    pid_set_altitude_target(100.0f);
    pid_set_position_target(100, 200);
    pid_set_altitude_kp(0.5f);
    pid_set_altitude_ki(0.0f);
    pid_set_altitude_kd(0.0f);
    pid_set_position_x_kp(0.1f);
    pid_set_position_x_ki(0.0f);
    pid_set_position_x_kd(0.0f);
    pid_set_position_y_kp(0.1f);
    pid_set_position_y_ki(0.0f);
    pid_set_position_y_kd(0.0f);

    pid_update_navigation(90.0f, 90, 210, 0.05f);
    assert(pid_get_altitude_output() == 5.0f);
    assert(pid_get_position_roll_target() == 1.0f);
    assert(pid_get_position_pitch_target() == -1.0f);

    /* 帧间隔超过控制器允许值时清空旧修正，避免下一次突跳。 */
    pid_update_navigation(80.0f, 80, 220, 0.20f);
    assert(pid_get_altitude_output() == 0.0f);
    assert(pid_get_position_roll_target() == 0.0f);
    assert(pid_get_position_pitch_target() == 0.0f);
}

int main(void)
{
    AssertYawShortestPathAndMixer();
    AssertNavigationOutputAndReset();
    puts("pid_control_test: PASS");
    return 0;
}
