#include "slave_app.h"
int main(void)
{
    SlaveApp_Init();
    for (;;) SlaveApp_Process();
}
