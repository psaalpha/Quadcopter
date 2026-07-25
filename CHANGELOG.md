# Changelog

## 2026-07-26 - Keep Host assertions active in Release

### Fixed
- Undefine `NDEBUG` for GCC/Clang and MSVC Host test targets so Release builds
  still execute assertion-based protocol and safety tests.
- Prevent Release warnings-as-errors caused by test variables becoming unused
  or uninitialized after assertion removal.
- Document the Debug-versus-Release test lesson in the student guide.

### Scope
- Host test configuration and documentation only.
- No firmware source, target compiler option, algorithm, protocol, interrupt,
  peripheral, mixer, or PWM behavior was changed.

## 2026-07-26 - Add the develop engineering learning guide

### Added
- Added a version-driven embedded software course for all six `main..develop`
  engineering commits.
- Mapped every version to its engineering problem, key files and functions,
  technical concepts, hands-on experiments, acceptance criteria, common
  mistakes, and interview explanation.
- Added five end-to-end code-reading routes, twelve progressive experiments, a
  twelve-week teaching plan, a capstone project, and a competency rubric.
- Linked the guide from the root README and documentation index, and made it a
  required engineering document.

### Scope
- Documentation and repository validation only.
- No PID, attitude, mixer, protocol, interrupt, peripheral, or PWM behavior was
  changed.

## 2026-07-26 - Complete the engineering documentation baseline

### Added
- Added a documentation index with role-based and sequential learning paths.
- Documented the repository structure, module ownership, active versus legacy
  code, and dependency rules.
- Defined a normative driver API standard covering naming, status returns,
  data snapshots, units, blocking behavior, ISR boundaries, headers, and a
  staged migration plan for existing drivers.
- Added development, automated testing, maintenance, troubleshooting, release,
  paired-firmware, flashing, and rollback procedures.

### Changed
- Expanded the architecture document with the problems before refactoring, the
  intent and effect of all four engineering commits, embedded-engineer skill
  mappings, current limitations, and future boundaries.
- Expanded the root README and contribution guide to point maintainers to the
  complete documentation set.
- Extended repository validation so missing core maintenance documents fail the
  engineering quality gate.

### Scope
- No flight-control algorithm, PID parameter, protocol layout, interrupt
  behavior, peripheral configuration, or motor-output behavior was changed.

### Validation
- Repository validation passed, including required engineering files, Keil
  source/include references, shared-protocol integration, and internal
  Markdown links.
- Root CMake build passed and `2/2` host tests passed.
- Unified Keil script result: both Master and Slave reported
  `0 Error(s), 0 Warning(s)`.

## 2026-07-25 - Add firmware engineering quality gates

### Changed
- Replaced the misleading `PWM1`, `PWM3`, and `Timer1` timer modules with a
  single `BSP/control_timers` owner for the master control timebase.
- Removed unused GPIO alternate-function setup from the TIM2 and TIM3 timebase
  path. This eliminates hidden conflicts with CRSF on PA2 and MPU6050 software
  I2C on PA6.
- Made the control timers start only after sensor, communication, motor-output,
  and safety initialization has completed.
- Added a repository-level CMake entry point, dual-target Keil build script,
  and Python validation for Keil XML source/include references and required
  architecture boundaries.
- Added GitHub Actions checks for repository validation, warning-clean host
  builds, and unit tests on `main`, `develop`, and pull requests.
- Added README, contribution rules, and architecture, build, pinout, protocol,
  safety, and roadmap documentation.

### Validation
- Repository validation passed for both Keil projects and every referenced
  source/include path.
- Root CMake build passed with GCC 8.1.0; `2/2` host tests passed.
- Unified Keil script result: both Master and Slave reported
  `0 Error(s), 0 Warning(s)`.
- Master program size after timer consolidation: Code `28748`, RO-data `892`,
  RW-data `568`, ZI-data `2664` bytes.

## 2026-07-25 - Make the master runtime deterministic

### Changed
- Added a cooperative periodic-task scheduler with four explicit rates:
  500 Hz IMU/inner loop, 200 Hz RC service, 100 Hz angle loop, and 50 Hz motor
  output.
- Reduced TIM1/TIM2/TIM3/TIM4 interrupt handlers to timekeeping, task
  notification, and interrupt acknowledgement only.
- Moved IMU filtering, gyro reads, angle reads, PID calculations, Bluetooth
  telemetry formatting, CRSF parsing, and motor register updates into the main
  execution context.
- Coalesced missed periodic releases instead of replaying stale control work;
  each task records an overrun counter for diagnostics.
- Centralized task rates, CRSF channel assignments, failsafe timing,
  low-throttle threshold, and minimum ESC compare value in
  `Master_MCU/BSP/board_config.h`.
- Split master application policy into `App` scheduler and flight-safety
  modules while keeping hardware drivers independent.

### Safety
- Replaced scattered booleans with explicit `STARTUP_LOCK`, `ACTIVE`,
  `LINK_LOSS`, and `RECOVERY_LOCK` states.
- A valid low-throttle frame is required before entering `ACTIVE` after both
  startup and link recovery.
- A 300 ms link timeout immediately enters `LINK_LOSS`, resets controller
  state, and writes the minimum value to all four motor outputs.
- Existing channel meanings are unchanged. This milestone does not silently
  assign an ARM switch; explicit pilot arming remains a separately documented
  follow-up.

### Validation
- Added host tests for startup lock, link loss, high-throttle recovery lock,
  low-throttle recovery, and 32-bit tick wraparound.
- Host result: `2/2` tests passed with warnings treated as errors.
- ARMCC master result: `0 Error(s), 0 Warning(s)`; Code `28932`, RO-data `892`,
  RW-data `568`, ZI-data `2664` bytes.

## 2026-07-25 - Version the inter-MCU sensor protocol

### Changed
- Replaced the compiler-dependent packed structure and raw `float` transfer
  between the slave and master with a 41-byte versioned protocol frame.
- Defined explicit little-endian integer fields and physical units for pressure,
  temperature, barometric altitude, yaw, optical flow, range, signal quality,
  battery voltage, sequence, timestamp, and sensor status flags.
- Replaced the single-byte XOR checksum with CRC16-CCITT (`0x1021`, initial
  value `0xFFFF`).
- Added receiver resynchronization, frame-format counters, CRC-error counters,
  and sequence-gap diagnostics.
- Added a shared, hardware-independent codec under `Shared/Protocol` and made
  both Keil projects compile the same implementation.

### Compatibility
- This is an intentional wire-protocol break. Master and slave firmware from
  this version must be flashed as a matched pair.
- Application-facing master values retain their previous units: altitude in
  centimetres, yaw in degrees, flow range in millimetres.

### Validation
- Added a host-side C test covering the standard CRC reference vector,
  byte-order checks, signed-value round trips, and rejection of malformed or
  corrupted frames.
- Host test result: `1/1` passed with GCC 8.1.0 and warnings treated as errors.
- ARMCC master result: `0 Error(s), 0 Warning(s)`; Code `28716`, RO-data `892`,
  RW-data `580`, ZI-data `2628` bytes.
- ARMCC slave result: `0 Error(s), 0 Warning(s)`; Code `30122`, RO-data `2182`,
  RW-data `104`, ZI-data `1736` bytes.

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
