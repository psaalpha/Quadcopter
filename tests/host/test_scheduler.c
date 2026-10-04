#include "periodic_tasks.h"
#include "app_scheduler.h"
#include "slave_scheduler.h"
#include "flight_safety.h"
#include "board_config.h"
#include <assert.h>
#include <stdio.h>
extern uint32_t fake_interrupt_mask;
int main(void)
{
    uint32_t counts[4] = {0u};
    uint32_t slave_counts[2] = {0u};
    uint32_t tick;
    uint8_t i;
    PeriodicTasks delayed;
    FlightSafetyContext safety;
    const uint32_t periods[4] = {2u, 5u, 10u, 100u};
    fake_interrupt_mask = 1u;
    AppScheduler_Init();
    SlaveScheduler_Init();
    assert(fake_interrupt_mask == 1u);
    fake_interrupt_mask = 0u;
    for (tick = 0u; tick < 10000u; ++tick) {
        AppScheduler_TickFromIsr(1u);
        SlaveScheduler_TickFromIsr(1u);
        for (i = 0u; i < 4u; ++i) counts[i] += AppScheduler_Take((AppTaskId)i);
        for (i = 0u; i < 2u; ++i) slave_counts[i] += SlaveScheduler_Take((SlaveTaskId)i);
    }
    assert(counts[0] == 5000u && counts[1] == 2000u);
    assert(counts[2] == 1000u && counts[3] == 100u);
    assert(slave_counts[0] == 200u && slave_counts[1] == 50u);
    assert(SlaveScheduler_Now() == 10000u);
    assert(fake_interrupt_mask == 0u);
    AppScheduler_TickFromIsr(2u);
    fake_interrupt_mask = 1u;
    assert(AppScheduler_Take(APP_TASK_IMU_UPDATE) == 1u);
    assert(fake_interrupt_mask == 1u);
    fake_interrupt_mask = 0u;
    /* New notification after taking an event survives task execution. */
    AppScheduler_TickFromIsr(2u);
    assert(AppScheduler_Take(APP_TASK_IMU_UPDATE) == 1u);
    PeriodicTasks_Init(&delayed, periods, 4u);
    delayed.now_ms = UINT32_MAX - 49u;
    PeriodicTasks_Advance(&delayed, 100u);
    assert(delayed.now_ms == 50u);
    assert(delayed.overruns[0] == 49u && delayed.overruns[1] == 19u);
    assert(delayed.overruns[2] == 9u && delayed.overruns[3] == 0u);
    for (i = 0u; i < 4u; ++i) {
        assert(PeriodicTasks_Take(&delayed, i) == 1u);
        assert(PeriodicTasks_Take(&delayed, i) == 0u);
    }
    PeriodicTasks_Advance(&delayed, 7u);
    assert(delayed.remaining_ms[0] == 1u && delayed.remaining_ms[1] == 3u);
    assert(delayed.overruns[0] == 51u);
    FlightSafety_Init(&safety);
    FlightSafety_OnValidRcFrame(&safety, UINT32_MAX - 99u, 0u, 5u);
    assert(FlightSafety_CheckTimeout(&safety, 199u, BOARD_RC_FAILSAFE_TIMEOUT_MS) == 0u);
    assert(FlightSafety_CheckTimeout(&safety, 200u, BOARD_RC_FAILSAFE_TIMEOUT_MS) == 1u);
    puts("scheduler tests passed");
    return 0;
}
