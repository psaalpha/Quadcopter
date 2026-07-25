cmake_minimum_required(VERSION 3.16)

project(quadcopter_host_tests C)

enable_testing()

function(add_quadcopter_host_test target_name test_name)
    add_executable(${target_name} ${ARGN})

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /UNDEBUG)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
            -Wall
            -Wextra
            -Werror
            -pedantic
            -UNDEBUG
        )
    endif()

    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

add_quadcopter_host_test(
    inter_mcu_protocol_test
    inter_mcu_protocol
    test_inter_mcu_protocol.c
    ../../Shared/Protocol/inter_mcu_protocol.c
)
target_include_directories(
    inter_mcu_protocol_test
    PRIVATE
    ../../Shared/Protocol
)

add_quadcopter_host_test(
    flight_safety_test
    flight_safety
    test_flight_safety.c
    ../../Master_MCU/App/flight_safety.c
)
target_include_directories(
    flight_safety_test
    PRIVATE
    ../../Master_MCU/App
)

add_quadcopter_host_test(
    driver_status_test
    driver_status
    test_driver_status.c
    ../../Shared/Drivers/driver_status.c
)
target_include_directories(
    driver_status_test
    PRIVATE
    ../../Shared/Drivers
)

add_quadcopter_host_test(
    status_led_test
    status_led
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

add_quadcopter_host_test(
    app_task_model_test
    app_task_model
    test_app_task_model.c
    ../../Master_MCU/App/app_task_model.c
)
target_include_directories(
    app_task_model_test
    PRIVATE
    ../../Master_MCU/App
    ../../Master_MCU/BSP
)

add_quadcopter_host_test(
    parameter_store_test
    parameter_store
    test_parameter_store.c
    ../../Shared/Services/parameter_store.c
)
target_include_directories(
    parameter_store_test
    PRIVATE
    ../../Shared/Services
)

add_quadcopter_host_test(
    event_log_test
    event_log
    test_event_log.c
    ../../Shared/Services/event_log.c
)
target_include_directories(
    event_log_test
    PRIVATE
    ../../Shared/Services
)

add_quadcopter_host_test(
    watchdog_manager_test
    watchdog_manager
    test_watchdog_manager.c
    ../../Shared/Safety/watchdog_manager.c
)
target_include_directories(
    watchdog_manager_test
    PRIVATE
    ../../Shared/Safety
)

add_quadcopter_host_test(
    fault_manager_test
    fault_manager
    test_fault_manager.c
    ../../Shared/Safety/fault_manager.c
)
target_include_directories(
    fault_manager_test
    PRIVATE
    ../../Shared/Safety
)

add_quadcopter_host_test(
    failsafe_state_machine_test
    failsafe_state_machine
    test_failsafe_state_machine.c
    ../../Shared/Safety/failsafe_state_machine.c
)
target_include_directories(
    failsafe_state_machine_test
    PRIVATE
    ../../Shared/Safety
)

add_quadcopter_host_test(
    hal_interfaces_test
    hal_interfaces
    test_hal_interfaces.c
    ../../Shared/HAL/hal_uart.c
    ../../Shared/HAL/hal_i2c.c
    ../../Shared/HAL/hal_spi.c
    ../../Shared/HAL/hal_pwm.c
    ../../Shared/HAL/hal_timer.c
)
target_include_directories(
    hal_interfaces_test
    PRIVATE
    ../../Shared/HAL
)

add_quadcopter_host_test(
    flight_data_logger_test
    flight_data_logger
    test_flight_data_logger.c
    ../../Shared/Services/flight_data_logger.c
)
target_include_directories(
    flight_data_logger_test
    PRIVATE
    ../../Shared/Services
)

add_quadcopter_host_test(
    parameter_extensions_test
    parameter_extensions
    test_parameter_extensions.c
    ../../Shared/Services/parameter_store.c
    ../../Shared/Services/parameter_catalog.c
    ../../Shared/Services/parameter_persistence.c
)
target_include_directories(
    parameter_extensions_test
    PRIVATE
    ../../Shared/Services
)

add_quadcopter_host_test(
    ground_station_protocol_test
    ground_station_protocol
    test_ground_station_protocol.c
    ../../Shared/Protocol/ground_station_protocol.c
    ../../Shared/Services/ground_station_service.c
)
target_include_directories(
    ground_station_protocol_test
    PRIVATE
    ../../Shared/Protocol
    ../../Shared/Services
)
