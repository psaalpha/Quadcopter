#include "navigation_estimator.h"
#include <string.h>
#include <math.h>
#define RAD .01745329252f
void navigation_estimator_reset(navigation_estimator_t *n){memset(n,0,sizeof(*n));}
void navigation_estimator_gyro(navigation_estimator_t *n,float roll_rate,float pitch_rate,float dt)
{
    if(dt>0 && dt<=.01f){n->roll_integral+=roll_rate*dt*RAD;n->pitch_integral+=pitch_rate*dt*RAD;}
}
static int32_t difference(int32_t a,int32_t b)
{uint32_t u=(uint32_t)a-(uint32_t)b;int32_t s;memcpy(&s,&u,4);return s;}
int navigation_estimator_update(navigation_estimator_t *n,int32_t x,int32_t y,uint32_t us,uint16_t epoch,
    uint16_t range,float roll,float pitch,float yaw,float sx,float sy,float sensor_yaw)
{
    float a,dx,dy,bx,by,wx,wy,dt;uint32_t elapsed=us-n->last_integration_us;
    if(!(range>=20u && range<=8000u && fabsf(roll)<=35 && fabsf(pitch)<=35 &&
         fabsf(sx)>=.01f && fabsf(sx)<=5 && fabsf(sy)>=.01f && fabsf(sy)<=5 &&
         fabsf(yaw)<=360 && fabsf(sensor_yaw)<=180)) {
        navigation_estimator_reset(n);return 0;
    }
    n->height_cm=(float)range*.1f*cosf(roll*RAD)*cosf(pitch*RAD);
    if(!n->initialized || n->epoch!=epoch || elapsed==0u || elapsed>200000u) {
        n->last_x=x;n->last_y=y;n->last_integration_us=us;n->epoch=epoch;n->initialized=1u;
        n->x_mm=n->y_mm=n->velocity_x_cm_s=n->velocity_y_cm_s=0;
        n->roll_integral=n->pitch_integral=0;return 0;
    }
    dt=(float)elapsed*.000001f;
    dx=(float)difference(x,n->last_x)*.0001f*sx;
    dy=(float)difference(y,n->last_y)*.0001f*sy;
    a=sensor_yaw*RAD;
    bx=dx*cosf(a)-dy*sinf(a)-n->roll_integral;
    by=dx*sinf(a)+dy*cosf(a)-n->pitch_integral;
    n->roll_integral=n->pitch_integral=0;
    n->last_x=x;n->last_y=y;n->last_integration_us=us;
    if(!(fabsf(bx)<.5f && fabsf(by)<.5f)){n->initialized=0u;return 0;}
    bx*=n->height_cm*10.0f;by*=n->height_cm*10.0f;
    a=yaw*RAD;wx=bx*cosf(a)-by*sinf(a);wy=bx*sinf(a)+by*cosf(a);
    n->x_mm+=wx;n->y_mm+=wy;
    n->velocity_x_cm_s+=.3f*(wx*.1f/dt-n->velocity_x_cm_s);
    n->velocity_y_cm_s+=.3f*(wy*.1f/dt-n->velocity_y_cm_s);
    if(!(fabsf(n->x_mm)<1000000 && fabsf(n->y_mm)<1000000)){n->initialized=0u;return 0;}
    return 1;
}
