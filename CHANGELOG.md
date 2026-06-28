# Changelog

## 2026-06-28 - Improve PID

### Changed
- Added this changelog to record project-level changes alongside Git commits.
- Protected interrupt-shared data in `Master_MCU/User/main.c`: `roll`, `pitch`, `yaw`, `rollRate`, `pitchRate`, `yawRate`, and `PWM_Flag` are now `volatile`.
- Added short `__disable_irq()` snapshot sections when the main loop reads attitude, angular-rate, and slave-sensor data, avoiding partial reads while ISRs update shared values.
- Enabled yaw mixing in `Master_MCU/Hardware/Pid.c`: `FL`/`BR` add `yaw_pid_out`, while `FR`/`BL` subtract it, so yaw PID now reaches all four motor outputs.
- Added altitude PID control in `Drone_Altitude_Position_PID_Control()` using `slave.flow_altitude`, with output available through `Altitude_pid_Get()`.
- Added position-hold PID calculations using `slave.flow_x` and `slave.flow_y`, with suggested roll and pitch target corrections available through `Position_roll_aim_Get()` and `Position_pitch_aim_Get()`.
- Connected slave sensor snapshots in the master main loop for `flow_x`, `flow_y`, `flow_altitude`, `baro_altitude`, and `mag_yaw`, then calls the altitude/position PID routine.
- Updated `Master_MCU/Hardware/Pid.h` with `stm32f10x.h` and altitude/position PID configuration and readback interfaces.

### Notes
- Altitude and position PID default `Kp`, `Ki`, and `Kd` values are `0`, so they do not automatically alter throttle or remote attitude targets until tuned.
