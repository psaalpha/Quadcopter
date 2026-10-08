#include "slave_app.h"
int main(void)
{
    slave_app_init();
    for (;;) slave_app_process();
}
