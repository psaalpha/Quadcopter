#include "hal_i2c.h"
#include "hal_pwm.h"
#include "hal_spi.h"
#include "hal_timer.h"
#include "hal_uart.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    HalTransferState state;
    uint32_t calls;
    uint32_t value;
    uint16_t length;
    uint8_t channel;
} FakeHal;

static HalStatus FakeUartTransmit(
    void *context,
    const uint8_t *data,
    uint16_t length)
{
    FakeHal *fake = (FakeHal *)context;

    fake->calls++;
    fake->length = length;
    fake->value = data[0];
    return HAL_STATUS_OK;
}

static HalStatus FakeUartRead(
    void *context,
    uint8_t *data,
    uint16_t capacity,
    uint16_t *read_length)
{
    FakeHal *fake = (FakeHal *)context;

    fake->calls++;
    data[0] = 0xA5u;
    *read_length = (capacity != 0u) ? 1u : 0u;
    return HAL_STATUS_OK;
}

static HalStatus FakeGetState(void *context, HalTransferState *state)
{
    *state = ((FakeHal *)context)->state;
    return HAL_STATUS_OK;
}

static HalStatus FakeControl(void *context)
{
    ((FakeHal *)context)->calls++;
    return HAL_STATUS_OK;
}

static HalStatus FakeI2cStart(
    void *context,
    const HalI2cTransfer *transfer)
{
    FakeHal *fake = (FakeHal *)context;

    fake->calls++;
    fake->value = transfer->address_7bit;
    fake->length = (uint16_t)(transfer->write_length
        + transfer->read_length);
    return HAL_STATUS_OK;
}

static HalStatus FakeSpiStart(
    void *context,
    const HalSpiTransfer *transfer)
{
    FakeHal *fake = (FakeHal *)context;

    fake->calls++;
    fake->channel = transfer->chip_select_id;
    fake->length = transfer->length;
    return HAL_STATUS_OK;
}

static HalStatus FakePwmSet(
    void *context,
    uint8_t channel,
    uint32_t pulse_ticks)
{
    FakeHal *fake = (FakeHal *)context;

    fake->calls++;
    fake->channel = channel;
    fake->value = pulse_ticks;
    return HAL_STATUS_OK;
}

static HalStatus FakePwmEnable(
    void *context,
    uint8_t channel,
    uint8_t enabled)
{
    FakeHal *fake = (FakeHal *)context;

    fake->calls++;
    fake->channel = channel;
    fake->value = enabled;
    return HAL_STATUS_OK;
}

static HalStatus FakePwmLimits(
    void *context,
    uint8_t channel,
    HalPwmLimits *limits)
{
    FakeHal *fake = (FakeHal *)context;

    fake->channel = channel;
    limits->minimum_ticks = 500u;
    limits->maximum_ticks = 1000u;
    return HAL_STATUS_OK;
}

static HalStatus FakeTimerValue(void *context, uint32_t *value)
{
    *value = ((FakeHal *)context)->value;
    return HAL_STATUS_OK;
}

static void TestUartContract(void)
{
    static const HalUartOps ops =
    {
        FakeUartTransmit,
        FakeUartRead,
        FakeGetState,
        FakeControl
    };
    FakeHal fake = {0};
    HalUart uart;
    HalTransferState state;
    uint8_t tx[2] = {0x42u, 0x43u};
    uint8_t rx[2] = {0u};
    uint16_t read_length = 0u;

    uart.context = &fake;
    uart.ops = &ops;
    fake.state = HAL_TRANSFER_BUSY;

    assert(HalUart_StartTransmit(&uart, tx, 2u) == HAL_STATUS_OK);
    assert(fake.length == 2u);
    assert(fake.value == 0x42u);
    assert(HalUart_Read(
        &uart,
        rx,
        sizeof(rx),
        &read_length) == HAL_STATUS_OK);
    assert(read_length == 1u);
    assert(rx[0] == 0xA5u);
    assert(HalUart_GetTransmitState(&uart, &state) == HAL_STATUS_OK);
    assert(state == HAL_TRANSFER_BUSY);
    assert(HalUart_AbortTransmit(&uart) == HAL_STATUS_OK);
    assert(HalUart_StartTransmit(&uart, 0, 2u)
        == HAL_STATUS_INVALID_ARGUMENT);
}

static void TestI2cAndSpiContracts(void)
{
    static const HalI2cOps i2c_ops =
    {
        FakeI2cStart,
        FakeGetState,
        FakeControl
    };
    static const HalSpiOps spi_ops =
    {
        FakeSpiStart,
        FakeGetState,
        FakeControl
    };
    FakeHal fake = {0};
    HalI2c i2c;
    HalSpi spi;
    HalI2cTransfer i2c_transfer;
    HalSpiTransfer spi_transfer;
    uint8_t tx[2] = {1u, 2u};
    uint8_t rx[2] = {0u};

    i2c.context = &fake;
    i2c.ops = &i2c_ops;
    memset(&i2c_transfer, 0, sizeof(i2c_transfer));
    i2c_transfer.address_7bit = 0x68u;
    i2c_transfer.write_data = tx;
    i2c_transfer.write_length = 1u;
    i2c_transfer.read_data = rx;
    i2c_transfer.read_length = 2u;
    assert(HalI2c_Start(&i2c, &i2c_transfer) == HAL_STATUS_OK);
    assert(fake.value == 0x68u);
    assert(fake.length == 3u);
    i2c_transfer.address_7bit = 0x80u;
    assert(HalI2c_Start(
        &i2c,
        &i2c_transfer) == HAL_STATUS_INVALID_ARGUMENT);

    spi.context = &fake;
    spi.ops = &spi_ops;
    spi_transfer.transmit_data = tx;
    spi_transfer.receive_data = rx;
    spi_transfer.length = 2u;
    spi_transfer.chip_select_id = 3u;
    assert(HalSpi_Start(&spi, &spi_transfer) == HAL_STATUS_OK);
    assert(fake.channel == 3u);
    assert(fake.length == 2u);
    spi_transfer.length = 0u;
    assert(HalSpi_Start(
        &spi,
        &spi_transfer) == HAL_STATUS_INVALID_ARGUMENT);
}

static void TestPwmRangeProtection(void)
{
    static const HalPwmOps ops =
    {
        FakePwmSet,
        FakePwmEnable,
        FakePwmLimits
    };
    FakeHal fake = {0};
    HalPwm pwm;
    HalPwmLimits limits;

    pwm.context = &fake;
    pwm.ops = &ops;
    assert(HalPwm_GetLimits(&pwm, 2u, &limits) == HAL_STATUS_OK);
    assert(limits.minimum_ticks == 500u);
    assert(limits.maximum_ticks == 1000u);
    assert(HalPwm_SetPulseTicks(&pwm, 2u, 750u) == HAL_STATUS_OK);
    assert(fake.channel == 2u);
    assert(fake.value == 750u);
    assert(HalPwm_SetPulseTicks(
        &pwm,
        2u,
        1001u) == HAL_STATUS_OUT_OF_RANGE);
    assert(HalPwm_Enable(&pwm, 2u, 1u) == HAL_STATUS_OK);
    assert(HalPwm_Enable(
        &pwm,
        2u,
        2u) == HAL_STATUS_INVALID_ARGUMENT);
}

static void TestTimerContract(void)
{
    static const HalTimerOps ops =
    {
        FakeControl,
        FakeControl,
        FakeTimerValue,
        FakeTimerValue
    };
    FakeHal fake = {0};
    HalTimer timer;
    uint32_t value;

    timer.context = &fake;
    timer.ops = &ops;
    fake.value = 72000000u;

    assert(HalTimer_Start(&timer) == HAL_STATUS_OK);
    assert(HalTimer_GetFrequencyHz(&timer, &value) == HAL_STATUS_OK);
    assert(value == 72000000u);
    fake.value = 1234u;
    assert(HalTimer_GetCounterTicks(&timer, &value) == HAL_STATUS_OK);
    assert(value == 1234u);
    assert(HalTimer_Stop(&timer) == HAL_STATUS_OK);
}

int main(void)
{
    TestUartContract();
    TestI2cAndSpiContracts();
    TestPwmRangeProtection();
    TestTimerContract();

    puts("HAL interface tests passed");
    return 0;
}
