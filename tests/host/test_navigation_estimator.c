#include "navigation_estimator.h"
#include <assert.h>
#include <math.h>
int main(void)
{
    navigation_estimator_t n;navigation_estimator_reset(&n);
    assert(!navigation_estimator_update(&n,0,0,10000,1,1000,0,0,0,1,1,0));
    assert(navigation_estimator_update(&n,100,0,30000,1,1000,0,0,0,1,1,0));
    assert(fabsf(n.x_mm-10)<.01f && fabsf(n.height_cm-100)<.01f);
    navigation_estimator_gyro(&n,100,0,.002f);
    assert(navigation_estimator_update(&n,100,0,50000,1,1000,0,0,0,1,1,0));assert(n.x_mm<10);
    assert(!navigation_estimator_update(&n,120,0,70000,2,1000,0,0,0,1,1,0));assert(n.x_mm==0);
    assert(!navigation_estimator_update(&n,120,0,90000,2,0,0,0,0,1,1,0));
    navigation_estimator_reset(&n);
    assert(!navigation_estimator_update(&n,0,0,UINT32_MAX-9999u,1,1000,0,0,90,1,1,0));
    assert(navigation_estimator_update(&n,100,0,10000u,1,1000,0,0,90,1,1,0));
    assert(fabsf(n.y_mm-10)<.01f && fabsf(n.x_mm)<.01f);return 0;
}
