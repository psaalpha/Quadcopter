#include "status_led.h"

#include <assert.h>
#include <stdio.h>

#include "fake_hal_gpio.h"

static void TestActiveLowLed(void)
{
    FakeHalGpio fake;
    StatusLed led;
    StatusLedState state;

    FakeHalGpio_Init(&fake, HAL_GPIO_LEVEL_LOW);

    assert(StatusLed_Init(
        &led,
        &fake.gpio,
        HAL_GPIO_LEVEL_LOW,
        10u) == DRIVER_STATUS_OK);
    assert(fake.level == HAL_GPIO_LEVEL_HIGH);
    assert(fake.write_count == 1u);
    assert(led.health.state == DRIVER_STATE_READY);

    assert(StatusLed_Set(&led, STATUS_LED_ON, 20u) == DRIVER_STATUS_OK);
    assert(fake.level == HAL_GPIO_LEVEL_LOW);
    assert(StatusLed_GetState(&led, &state) == DRIVER_STATUS_OK);
    assert(state == STATUS_LED_ON);

    assert(StatusLed_Toggle(&led, 30u) == DRIVER_STATUS_OK);
    assert(fake.level == HAL_GPIO_LEVEL_HIGH);
    assert(StatusLed_GetState(&led, &state) == DRIVER_STATUS_OK);
    assert(state == STATUS_LED_OFF);
}

static void TestActiveHighLed(void)
{
    FakeHalGpio fake;
    StatusLed led;

    FakeHalGpio_Init(&fake, HAL_GPIO_LEVEL_HIGH);

    assert(StatusLed_Init(
        &led,
        &fake.gpio,
        HAL_GPIO_LEVEL_HIGH,
        0u) == DRIVER_STATUS_OK);
    assert(fake.level == HAL_GPIO_LEVEL_LOW);
    assert(StatusLed_Set(&led, STATUS_LED_ON, 1u) == DRIVER_STATUS_OK);
    assert(fake.level == HAL_GPIO_LEVEL_HIGH);
}

static void TestFailureIsObservableAndRecoverable(void)
{
    FakeHalGpio fake;
    StatusLed led;
    const DriverHealth *health;

    FakeHalGpio_Init(&fake, HAL_GPIO_LEVEL_HIGH);
    assert(StatusLed_Init(
        &led,
        &fake.gpio,
        HAL_GPIO_LEVEL_LOW,
        0u) == DRIVER_STATUS_OK);

    FakeHalGpio_FailNextWrite(&fake, HAL_STATUS_IO_ERROR);
    assert(StatusLed_Set(
        &led,
        STATUS_LED_ON,
        50u) == DRIVER_STATUS_IO_ERROR);

    health = StatusLed_GetHealth(&led);
    assert(health != 0);
    assert(health->state == DRIVER_STATE_DEGRADED);
    assert(health->last_status == DRIVER_STATUS_IO_ERROR);
    assert(health->last_error_ms == 50u);

    assert(StatusLed_Set(&led, STATUS_LED_ON, 60u) == DRIVER_STATUS_OK);
    assert(fake.level == HAL_GPIO_LEVEL_LOW);
    assert(health->state == DRIVER_STATE_READY);
    assert(health->consecutive_errors == 0u);
}

static void TestInvalidInterfaces(void)
{
    HalGpio invalid_gpio = {0};
    StatusLed led;

    assert(StatusLed_Init(
        0,
        &invalid_gpio,
        HAL_GPIO_LEVEL_LOW,
        0u) == DRIVER_STATUS_INVALID_ARGUMENT);
    assert(StatusLed_Init(
        &led,
        &invalid_gpio,
        HAL_GPIO_LEVEL_LOW,
        0u) == DRIVER_STATUS_INVALID_ARGUMENT);
    assert(led.health.state == DRIVER_STATE_FAULT);
}

int main(void)
{
    TestActiveLowLed();
    TestActiveHighLed();
    TestFailureIsObservableAndRecoverable();
    TestInvalidInterfaces();

    puts("status LED tests passed");
    return 0;
}
