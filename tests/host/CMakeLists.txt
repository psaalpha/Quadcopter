cmake_minimum_required(VERSION 3.16)

project(quadcopter_host_tests C)

enable_testing()

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
    ../../Master_MCU/Hardware/Pid.c
)
target_include_directories(pid_control_test PRIVATE ../../Master_MCU/Hardware)

# 姿态模块只在此测试中使用一份最小 STM32 类型桩；不模拟传感器时序。
add_executable(yaw_estimator_test
    test_yaw_estimator.c
    ../../Master_MCU/User/hubu.c
)
target_include_directories(yaw_estimator_test PRIVATE
    fakes
    ../../Master_MCU/User
    ../../Master_MCU/Hardware
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

add_test(NAME pid_control COMMAND pid_control_test)
add_test(NAME yaw_estimator COMMAND yaw_estimator_test)
