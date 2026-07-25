#include "app_task_model.h"

#include <assert.h>
#include <stdio.h>

#include "board_config.h"

static void TestPeriodsMatchTheStableScheduler(void)
{
    const AppTaskSpec *imu = AppTaskModel_Get(APP_TASK_IMU_UPDATE);
    const AppTaskSpec *rc = AppTaskModel_Get(APP_TASK_RC_SERVICE);
    const AppTaskSpec *angle = AppTaskModel_Get(APP_TASK_ANGLE_CONTROL);
    const AppTaskSpec *motor = AppTaskModel_Get(APP_TASK_MOTOR_OUTPUT);

    assert(imu != 0);
    assert(rc != 0);
    assert(angle != 0);
    assert(motor != 0);

    assert(imu->period_ms == BOARD_IMU_TASK_PERIOD_MS);
    assert(rc->period_ms == BOARD_RC_TASK_PERIOD_MS);
    assert(angle->period_ms == BOARD_ANGLE_TASK_PERIOD_MS);
    assert(motor->period_ms == BOARD_MOTOR_TASK_PERIOD_MS);
    assert(AppTaskModel_Get(APP_TASK_COUNT) == 0);
}

static void TestSafetyCriticalPriorityOrdering(void)
{
    const AppTaskSpec *imu = AppTaskModel_Get(APP_TASK_IMU_UPDATE);
    const AppTaskSpec *rc = AppTaskModel_Get(APP_TASK_RC_SERVICE);
    const AppTaskSpec *angle = AppTaskModel_Get(APP_TASK_ANGLE_CONTROL);
    const AppTaskSpec *motor = AppTaskModel_Get(APP_TASK_MOTOR_OUTPUT);

    assert(imu->priority > rc->priority);
    assert(rc->priority > angle->priority);
    assert(motor->priority == rc->priority);
    assert(imu->coalesce_releases == 1u);
    assert(motor->coalesce_releases == 1u);
}

static void TestManifestResourceBudget(void)
{
    assert(AppTaskModel_Validate() == 1u);
    assert(AppTaskModel_TotalStackWords() == 832u);
}

int main(void)
{
    TestPeriodsMatchTheStableScheduler();
    TestSafetyCriticalPriorityOrdering();
    TestManifestResourceBudget();

    puts("application task model tests passed");
    return 0;
}
