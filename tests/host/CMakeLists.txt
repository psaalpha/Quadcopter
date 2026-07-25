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
    target_compile_options(inter_mcu_protocol_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        inter_mcu_protocol_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
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
    target_compile_options(flight_safety_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        flight_safety_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

add_test(NAME flight_safety COMMAND flight_safety_test)

add_executable(
    driver_status_test
    test_driver_status.c
    ../../Shared/Drivers/driver_status.c
)

target_include_directories(
    driver_status_test
    PRIVATE
    ../../Shared/Drivers
)

if(MSVC)
    target_compile_options(driver_status_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        driver_status_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

add_test(NAME driver_status COMMAND driver_status_test)
