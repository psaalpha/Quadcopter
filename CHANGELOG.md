# Changelog

## 2026-07-25 - Share the STM32F1 platform layer

### Changed
- Consolidated the byte-identical CMSIS/startup sources used by both
  controllers under `Platform/STM32F1/CMSIS`.
- Consolidated the byte-identical STM32F10x Standard Peripheral Library under
  `Platform/STM32F1/SPL`.
- Consolidated the common delay service under `Platform/STM32F1/System`.
- Updated both Keil projects to consume the shared platform sources directly,
  eliminating 61 duplicate source and header files.

### Compatibility
- The master and slave application, hardware, and user source trees remain
  independent.
- No control algorithm, interrupt timing, pin assignment, or wire protocol was
  changed in this milestone.

### Validation
- Rebuilt both `Master_MCU/Project.uvprojx` and
  `Slave_MCU/Project.uvprojx` with ARMCC 5.06 update 7.
- Master result: `0 Error(s), 0 Warning(s)`; Code `28236`, RO-data `892`,
  RW-data `576`, ZI-data `2592` bytes.
- Slave result: `0 Error(s), 0 Warning(s)`; Code `29478`, RO-data `2186`,
  RW-data `96`, ZI-data `1736` bytes.

## 2026-07-25 - Harden RC input and motor failsafe

### Fixed
- Added CRSF CRC8 DVB-S2 validation (`0xD5` polynomial) over frame type and
  payload. Invalid CRC frames no longer update RC channels or refresh link
  health.
- Added strict RC channel-frame length checks plus valid-frame and CRC-error
  diagnostic counters.
- Added a 300 ms RC timeout based on the existing 5 ms TIM1 tick. A timeout
  immediately clears control state and writes the minimum compare value to all
  four ESC outputs.
- Added startup and reconnect throttle locking. Motor output remains inhibited
  until a valid RC frame is received with throttle at or below 5 percent.
- Added `Drone_Motors_Stop()` to clear base throttle, cached motor mix values,
  PID outputs, integrators, and filter history so stale motor commands cannot be
  emitted after a stop request.
- Fixed Bluetooth tuning so PID defaults are preserved until a valid parameter
  update is actually received. Each frame now returns an update bitmask and
  only the named parameter is applied.
- Rejected malformed Bluetooth numeric values instead of interpreting them as
  zero, and made the ISR-shared receive flag `volatile`.

### Safety behavior
- CRSF frames with invalid CRC do not affect control state.
- On boot, after RC timeout, and after reconnect with high throttle, all four
  TIM4 compare registers are held at `500`.
- CH4 and CH5 retain their existing meanings; this change does not introduce a
  new ARM switch or alter channel and motor ordering.

### Validation
- Rebuilt `Master_MCU/Project.uvprojx` with ARMCC 5.06 update 7.
- Build result: `0 Error(s), 0 Warning(s)`.
- Program size: Code `28236`, RO-data `892`, RW-data `576`, ZI-data `2592`
  bytes.

### Remaining limitation
- The project still does not have a dedicated ARM/DISARM flight-state machine.
  The low-throttle interlock added here is an immediate safety baseline, not a
  replacement for explicit arming logic.

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
