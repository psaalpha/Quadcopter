#include "LED.h"

#include "stm32f10x.h"

#include "hal_gpio.h"
#include "status_led.h"
#include "stm32f1_gpio_hal.h"

static Stm32f1GpioOutput board_gpio_outputs[BOARD_LED_COUNT];
static HalGpio board_gpio[BOARD_LED_COUNT];
static StatusLed board_led[BOARD_LED_COUNT];
static uint8_t board_led_initialized;

static DriverStatus BoardLed_Bind(
    BoardLedId led,
    GPIO_TypeDef *port,
    uint16_t pin)
{
    HalStatus hal_status;

    hal_status = Stm32f1GpioOutput_Bind(
        &board_gpio_outputs[(uint8_t)led],
        port,
        pin,
        &board_gpio[(uint8_t)led]);
    if (hal_status != HAL_STATUS_OK)
    {
        return DRIVER_STATUS_IO_ERROR;
    }

    return StatusLed_Init(
        &board_led[(uint8_t)led],
        &board_gpio[(uint8_t)led],
        HAL_GPIO_LEVEL_LOW,
        0u);
}

DriverStatus BoardLed_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    DriverStatus status;

    board_led_initialized = 0u;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA,
        ENABLE);

    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Pin = GPIO_Pin_13;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &gpio_init);

    gpio_init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_5;
    GPIO_Init(GPIOA, &gpio_init);

    status = BoardLed_Bind(BOARD_LED_STATUS, GPIOC, GPIO_Pin_13);
    if (status != DRIVER_STATUS_OK)
    {
        return status;
    }
    status = BoardLed_Bind(BOARD_LED_AUXILIARY_1, GPIOA, GPIO_Pin_0);
    if (status != DRIVER_STATUS_OK)
    {
        return status;
    }
    status = BoardLed_Bind(BOARD_LED_AUXILIARY_2, GPIOA, GPIO_Pin_5);
    if (status != DRIVER_STATUS_OK)
    {
        return status;
    }

    board_led_initialized = 1u;
    return DRIVER_STATUS_OK;
}

DriverStatus BoardLed_Set(BoardLedId led, BoardLedState state)
{
    if ((uint8_t)led >= (uint8_t)BOARD_LED_COUNT)
    {
        return DRIVER_STATUS_INVALID_ARGUMENT;
    }
    if (!board_led_initialized)
    {
        return DRIVER_STATUS_NOT_INITIALIZED;
    }
    if ((state != BOARD_LED_OFF) && (state != BOARD_LED_ON))
    {
        return DRIVER_STATUS_INVALID_ARGUMENT;
    }

    return StatusLed_Set(
        &board_led[(uint8_t)led],
        (state == BOARD_LED_ON) ? STATUS_LED_ON : STATUS_LED_OFF,
        0u);
}

const DriverHealth *BoardLed_GetHealth(BoardLedId led)
{
    if ((uint8_t)led >= (uint8_t)BOARD_LED_COUNT)
    {
        return 0;
    }

    return StatusLed_GetHealth(&board_led[(uint8_t)led]);
}

void LED_Init(void)
{
    (void)BoardLed_Init();
}

void LED1_ON(void)
{
    (void)BoardLed_Set(BOARD_LED_STATUS, BOARD_LED_ON);
}

void LED1_OFF(void)
{
    (void)BoardLed_Set(BOARD_LED_STATUS, BOARD_LED_OFF);
}

void LED2_ON(void)
{
    (void)BoardLed_Set(BOARD_LED_AUXILIARY_1, BOARD_LED_ON);
}

void LED2_OFF(void)
{
    (void)BoardLed_Set(BOARD_LED_AUXILIARY_1, BOARD_LED_OFF);
}

void LED3_ON(void)
{
    (void)BoardLed_Set(BOARD_LED_AUXILIARY_2, BOARD_LED_ON);
}

void LED3_OFF(void)
{
    (void)BoardLed_Set(BOARD_LED_AUXILIARY_2, BOARD_LED_OFF);
}
