cmake_minimum_required(VERSION 3.16)

project(quadcopter_host_tests C)

enable_testing()
include_directories(../../Shared/Scheduling)

add_executable(task_health_test test_task_health.c ../../Shared/Scheduling/task_health.c)
target_include_directories(task_health_test PRIVATE ../../Shared/Scheduling)
if(MSVC)
    target_compile_options(task_health_test PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(task_health_test PRIVATE -Wall -Wextra -Werror -pedantic -UNDEBUG)
endif()
add_test(NAME task_health COMMAND task_health_test)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(crsf_io_test test_crsf_io.c fakes/serial/registers.c
        ../../Shared/Drivers/dma_rx.c ../../Master_MCU/Drivers/Communication/crsf.c)
    target_include_directories(crsf_io_test PRIVATE fakes/serial ../../Shared/Drivers
        ../../Master_MCU/Drivers/Communication)
    target_compile_options(crsf_io_test PRIVATE -Wall -Wextra -Werror -pedantic
        -UNDEBUG -Wno-pointer-to-int-cast -fno-pie)
    target_link_options(crsf_io_test PRIVATE -no-pie)
    add_test(NAME crsf_io COMMAND crsf_io_test)
endif()

add_executable(
    inter_mcu_protocol_test
    test_inter_mcu_protocol.c
    ../../Shared/Protocol/inter_mcu_protocol.c
)

target_include_directories(
    inter_mcu_protocol_test
    PRIVATE
    ../../Shared/Protocol
)

if(MSVC)
    target_compile_options(
        inter_mcu_protocol_test
        PRIVATE
        /W4
        /WX
        /UNDEBUG
    )
else()
    target_compile_options(
        inter_mcu_protocol_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
        -UNDEBUG
    )
endif()

add_test(NAME inter_mcu_protocol COMMAND inter_mcu_protocol_test)

add_executable(
    flight_safety_test
    test_flight_safety.c
    ../../Master_MCU/App/flight_safety.c
)

target_include_directories(
    flight_safety_test
    PRIVATE
    ../../Master_MCU/App
)

if(MSVC)
    target_compile_options(
        flight_safety_test
        PRIVATE
        /W4
        /WX
        /UNDEBUG
    )
else()
    target_compile_options(
        flight_safety_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
        -UNDEBUG
    )
endif()

add_test(NAME flight_safety COMMAND flight_safety_test)

# 控制算法是纯 C 计算：在电脑上验证符号、限幅和重置，不依赖电机硬件。
add_executable(pid_control_test
    test_pid_control.c
    ../../Master_MCU/Control/pid_controller.c
)
target_include_directories(pid_control_test PRIVATE
    ../../Master_MCU/Control
)

# 姿态模块只在此测试中使用一份最小 STM32 类型桩；不模拟传感器时序。
add_executable(yaw_estimator_test
    test_yaw_estimator.c
    ../../Master_MCU/Control/attitude_estimator.c
)
target_include_directories(yaw_estimator_test PRIVATE
    fakes
    ../../Master_MCU/Control
    ../../Master_MCU/Drivers/Sensors
)

foreach(control_test pid_control_test yaw_estimator_test)
    if(MSVC)
        target_compile_options(${control_test} PRIVATE /W4 /WX /UNDEBUG)
    else()
        target_compile_options(${control_test} PRIVATE
            -Wall -Wextra -Werror -pedantic -UNDEBUG)
        target_link_libraries(${control_test} PRIVATE m)
    endif()
endforeach()

add_executable(flight_controller_test test_flight_controller.c
    ../../Master_MCU/App/flight_controller.c ../../Master_MCU/App/flight_safety.c
    ../../Master_MCU/Control/pid_controller.c ../../Master_MCU/Control/attitude_estimator.c
    ../../Master_MCU/Control/navigation_estimator.c ../../Shared/Protocol/flight_parameters.c)
target_include_directories(flight_controller_test PRIVATE fakes/serial
    ../../Master_MCU/App ../../Master_MCU/BSP ../../Master_MCU/Control
    ../../Master_MCU/Drivers/Sensors ../../Master_MCU/Drivers/Communication
    ../../Shared/Protocol ../../Platform/STM32F1/System)
if(MSVC)
    target_compile_options(flight_controller_test PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(flight_controller_test PRIVATE -Wall -Wextra -Werror -pedantic -UNDEBUG)
    target_link_libraries(flight_controller_test PRIVATE m)
endif()
add_test(NAME flight_controller COMMAND flight_controller_test)

add_test(NAME pid_control COMMAND pid_control_test)
add_test(NAME yaw_estimator COMMAND yaw_estimator_test)

# Register-model regression tests for the actual Master drivers. No ARM
# timing is simulated. Non-PIE keeps modeled 32-bit DMA addresses valid.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(master_io_test
        ../../Shared/Protocol/flight_parameters.c
        test_master_io.c
        fakes/serial/registers.c
        ../../Shared/Drivers/dma_rx.c
        ../../Master_MCU/Drivers/Communication/bluetooth_serial.c
        ../../Master_MCU/Drivers/Communication/slave_link.c
        ../../Master_MCU/BSP/motor_pwm.c
        ../../Shared/Protocol/inter_mcu_protocol.c
    )
    target_include_directories(master_io_test PRIVATE
        fakes/serial
        ../../Master_MCU/Drivers/Communication
        ../../Master_MCU/BSP
        ../../Shared/Protocol
        ../../Shared/Drivers
    )
    target_compile_options(master_io_test PRIVATE
        -Wall -Wextra -Werror -pedantic -UNDEBUG -Wno-pointer-to-int-cast -fno-pie)
    target_link_options(master_io_test PRIVATE -no-pie)
    add_test(NAME master_io COMMAND master_io_test)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(scheduler_test
        test_scheduler.c fakes/serial/registers.c
        ../../Shared/Scheduling/periodic_tasks.c
        ../../Master_MCU/App/app_scheduler.c
        ../../Master_MCU/App/flight_safety.c
        ../../Slave_MCU/App/slave_scheduler.c)
    target_include_directories(scheduler_test PRIVATE
        fakes/serial ../../Shared/Scheduling ../../Master_MCU/App
        ../../Master_MCU/BSP ../../Slave_MCU/App ../../Slave_MCU/BSP)
    add_executable(slave_io_test
        test_slave_io.c fakes/serial/registers.c
        ../../Shared/Drivers/dma_rx.c ../../Shared/Protocol/inter_mcu_protocol.c
        ../../Shared/Protocol/cv0850.c
        ../../Slave_MCU/Drivers/Sensors/optical_flow.c
        ../../Slave_MCU/Drivers/Communication/master_link.c ../../Slave_MCU/BSP/board_inputs.c)
    target_include_directories(slave_io_test PRIVATE
        fakes/serial
        ../../Shared/Drivers
        ../../Shared/Protocol
        ../../Slave_MCU/BSP
        ../../Slave_MCU/Drivers/Communication
        ../../Slave_MCU/Drivers/Sensors
    )
    add_executable(slave_app_test
        test_slave_app.c fakes/serial/registers.c
        ../../Slave_MCU/App/slave_app.c ../../Slave_MCU/App/slave_scheduler.c
        ../../Shared/Scheduling/periodic_tasks.c)
    target_include_directories(slave_app_test PRIVATE
        fakes/serial
        ../../Slave_MCU/App
        ../../Slave_MCU/BSP
        ../../Slave_MCU/Drivers/Communication
        ../../Slave_MCU/Drivers/Display
        ../../Slave_MCU/Drivers/Sensors
        ../../Slave_MCU/Drivers/Sensors/BMP390
        ../../Shared/Protocol
        ../../Shared/Scheduling
        ../../Platform/STM32F1/System
    )
    target_link_libraries(slave_app_test PRIVATE m)
    foreach(io_test scheduler_test slave_io_test slave_app_test)
        target_compile_options(${io_test} PRIVATE
            -Wall -Wextra -Werror -pedantic -UNDEBUG -Wno-pointer-to-int-cast -fno-pie)
        target_link_options(${io_test} PRIVATE -no-pie)
    endforeach()
    add_test(NAME scheduler COMMAND scheduler_test)
    add_test(NAME slave_io COMMAND slave_io_test)
    add_test(NAME slave_app COMMAND slave_app_test)
endif()


add_executable(cv0850_test test_cv0850.c ../../Shared/Protocol/cv0850.c)
target_include_directories(cv0850_test PRIVATE ../../Shared/Protocol)
if(MSVC)
    target_compile_options(cv0850_test PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(cv0850_test PRIVATE -Wall -Wextra -Werror -pedantic -UNDEBUG)
endif()
add_test(NAME cv0850 COMMAND cv0850_test)

foreach(name flight_parameters config_journal navigation_estimator)
    if(name STREQUAL "navigation_estimator")
        set(implementation ../../Master_MCU/Control/navigation_estimator.c)
    elseif(name STREQUAL "config_journal")
        set(implementation ../../Shared/Protocol/config_journal.c ../../Shared/Protocol/inter_mcu_protocol.c)
    else()
        set(implementation ../../Shared/Protocol/flight_parameters.c)
    endif()
    add_executable(${name}_test test_${name}.c ${implementation})
    target_include_directories(${name}_test PRIVATE ../../Shared/Protocol ../../Master_MCU/Control)
    if(MSVC)
        target_compile_options(${name}_test PRIVATE /W4 /WX /UNDEBUG)
    else()
        target_compile_options(${name}_test PRIVATE -Wall -Wextra -Werror -pedantic -UNDEBUG)
        target_link_libraries(${name}_test PRIVATE m)
    endif()
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()
