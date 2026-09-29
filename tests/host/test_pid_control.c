#include "Pid.h"

#include <assert.h>
#include <stdio.h>

static void AssertYawShortestPathAndMixer(void)
{
    Drone_Motors_Stop();
    Set_Base_Duty(50.0f);
    Yaw_Angle_Kp_Get(1.0f);
    Yaw_Kp_Get(1.0f);
    Yaw_Ki_Get(0.0f);
    Yaw_Kd_Get(0.0f);
    Yaw_aim_Get(359.0f);

    Drone_Outer_Angle_PID_Control(0.0f, 0.0f, 1.0f);
    assert(Yaw_err_Get() == -2.0f); /* 359°→1° 应走 -2°，不是 +358°。 */
    Drone_Inner_Rate_PID_Control(0.0f, 0.0f, 0.0f);
    assert(Get_Motor_Duty_FrontLeft() < Get_Motor_Duty_FrontRight());

    Drone_Motors_Stop();
    assert(Get_Motor_Duty_FrontLeft() == 500u);
    assert(Get_Motor_Duty_FrontRight() == 500u);
}

static void AssertNavigationOutputAndReset(void)
{
    Drone_Motors_Stop();
    Set_Base_Duty(50.0f);
    Altitude_aim_Get(100.0f);
    Position_aim_Get(100, 200);
    Altitude_Kp_Get(0.5f);
    Altitude_Ki_Get(0.0f);
    Altitude_Kd_Get(0.0f);
    Position_X_Kp_Get(0.1f);
    Position_X_Ki_Get(0.0f);
    Position_X_Kd_Get(0.0f);
    Position_Y_Kp_Get(0.1f);
    Position_Y_Ki_Get(0.0f);
    Position_Y_Kd_Get(0.0f);

    Drone_Altitude_Position_PID_Control(90.0f, 90, 210, 0.05f);
    assert(Altitude_pid_Get() == 5.0f);
    assert(Position_roll_aim_Get() == 1.0f);
    assert(Position_pitch_aim_Get() == -1.0f);

    /* 帧间隔超过控制器允许值时清空旧修正，避免下一次突跳。 */
    Drone_Altitude_Position_PID_Control(80.0f, 80, 220, 0.20f);
    assert(Altitude_pid_Get() == 0.0f);
    assert(Position_roll_aim_Get() == 0.0f);
    assert(Position_pitch_aim_Get() == 0.0f);
}

int main(void)
{
    AssertYawShortestPathAndMixer();
    AssertNavigationOutputAndReset();
    puts("pid_control_test: PASS");
    return 0;
}
