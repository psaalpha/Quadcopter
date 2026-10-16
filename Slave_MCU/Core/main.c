#include "slave_app.h"
#include "slave_rtos.h"
int main(void)
{
    slave_app_init(); /* 慢速设备初始化在调度器启动之前完成 */
    slave_rtos_start();
    return 0;
}
