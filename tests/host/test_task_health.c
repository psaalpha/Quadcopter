#include "task_health.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    /* 两条心跳都新鲜才能喂狗：低优先级任务饿死同样必须拒绝。 */
    assert(task_health_all_fresh(100u, 100u, 99u, 100u));
    assert(task_health_all_fresh(199u, 100u, 100u, 100u));
    assert(!task_health_all_fresh(200u, 200u, 100u, 100u));
    assert(!task_health_all_fresh(200u, 100u, 200u, 100u));
    assert(!task_health_all_fresh(1000u, 0u, 0u, 100u));
    assert(task_health_all_fresh(20u, UINT32_MAX - 10u, 19u, 100u));
    assert(!task_health_all_fresh(100u, UINT32_MAX - 10u, 99u, 100u));
    assert(!task_health_all_fresh(0u, 0u, 0u, 0u));
    puts("task watchdog health tests passed");
    return 0;
}
