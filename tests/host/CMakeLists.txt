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

add_executable(
    status_led_test
    test_status_led.c
    fakes/fake_hal_gpio.c
    ../../Shared/Drivers/driver_status.c
    ../../Shared/Drivers/status_led.c
    ../../Shared/HAL/hal_gpio.c
)

target_include_directories(
    status_led_test
    PRIVATE
    fakes
    ../../Shared/Drivers
    ../../Shared/HAL
)

if(MSVC)
    target_compile_options(status_led_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        status_led_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

add_test(NAME status_led COMMAND status_led_test)

add_executable(
    app_task_model_test
    test_app_task_model.c
    ../../Master_MCU/App/app_task_model.c
)

target_include_directories(
    app_task_model_test
    PRIVATE
    ../../Master_MCU/App
    ../../Master_MCU/BSP
)

if(MSVC)
    target_compile_options(app_task_model_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        app_task_model_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

add_test(NAME app_task_model COMMAND app_task_model_test)

add_executable(
    parameter_store_test
    test_parameter_store.c
    ../../Shared/Services/parameter_store.c
)

target_include_directories(
    parameter_store_test
    PRIVATE
    ../../Shared/Services
)

if(MSVC)
    target_compile_options(parameter_store_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        parameter_store_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

add_test(NAME parameter_store COMMAND parameter_store_test)

add_executable(
    event_log_test
    test_event_log.c
    ../../Shared/Services/event_log.c
)

target_include_directories(
    event_log_test
    PRIVATE
    ../../Shared/Services
)

if(MSVC)
    target_compile_options(event_log_test PRIVATE /W4 /WX)
else()
    target_compile_options(
        event_log_test
        PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

add_test(NAME event_log COMMAND event_log_test)
