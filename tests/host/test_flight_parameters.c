#include "flight_parameters.h"
#include <assert.h>
#include <math.h>
int main(void)
{
    flight_parameters_t p,q;flight_command_t c;uint8_t image[FLIGHT_PARAMETERS_BYTES];
    flight_parameters_defaults(&p);flight_parameters_defaults(&q);
    assert(flight_command_parse("slider,PAKp,12.5",&c) && c.id==FP_PAKP);
    assert(flight_parameter_set(&p,c.id,c.value));
    assert(flight_command_parse("set,AKp,0.3",&c) && c.id==FP_AKP);
    assert(!flight_command_parse("set,AKp,nan",&c));
    assert(!flight_command_parse("set,AKp,0.3,extra",&c));
    assert(!flight_command_parse("get,unknown",&c));
    assert(flight_command_parse("save",&c) && c.op==FC_SAVE);
    assert(!flight_parameter_set(&p,FP_NAV_ENABLE,1));
    assert(flight_parameter_set(&p,FP_AKP,.3f));assert(flight_parameter_set(&p,FP_XKP,.2f));
    assert(flight_parameter_set(&p,FP_YXKP,.2f));assert(flight_parameter_set(&p,FP_NAV_ENABLE,1));
    assert(!flight_parameter_set(&p,FP_XKP,0));
    assert(!flight_parameter_set(&p,FP_NAV_ENABLE,.5f));
    assert(flight_command_parse("set,YKd,3.4444e-3",&c) && c.value>.0034f && c.value<.0035f);
    assert(!flight_command_parse("set,YKd,3e-7",&c));
    assert(!flight_parameter_set(&p,FP_BAT_CRIT,12));assert(!flight_parameter_set(&p,FP_FLOW_X_SCALE,0));
    assert(flight_parameter_set(&p,FP_PAIM,10));flight_parameters_encode(&p,image);
    assert(flight_parameters_decode(&q,image,sizeof(image)) && q.value[FP_PAIM]==0 && q.value[FP_PAKP]==12.5f);
    image[0]=2;assert(!flight_parameters_decode(&q,image,sizeof(image)));
    assert(!flight_parameter_set(&p,FP_PKP,INFINITY));return 0;
}
