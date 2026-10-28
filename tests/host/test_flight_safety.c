#include "flight_safety.h"
#include <assert.h>
int main(void)
{
    flight_safety_context_t s;flight_safety_init(&s);
    flight_safety_on_valid_rc_frame(&s,0,0,5,1,1);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,1,0,5,0,1);
    flight_safety_on_valid_rc_frame(&s,2,0,5,2,1);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,3,0,5,1,0);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,4,80,5,1,1);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,4,0,5,1,1);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,4,0,5,0,1);
    flight_safety_on_valid_rc_frame(&s,5,0,5,1,1);assert(flight_safety_motors_allowed(&s));
    assert(!flight_safety_check_timeout(&s,304,300));assert(flight_safety_check_timeout(&s,305,300));
    flight_safety_on_valid_rc_frame(&s,306,0,5,1,1);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,307,0,5,0,1);
    flight_safety_on_valid_rc_frame(&s,308,0,5,1,1);assert(flight_safety_motors_allowed(&s));
    flight_safety_latch_fault(&s);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,309,0,5,1,1);assert(!flight_safety_motors_allowed(&s));
    flight_safety_on_valid_rc_frame(&s,310,70,5,0,1);assert(s.state==FLIGHT_SAFETY_DISARMED);
    flight_safety_init(&s);flight_safety_on_valid_rc_frame(&s,0xfffffff0u,0,5,0,1);
    assert(!flight_safety_check_timeout(&s,5u,32u));assert(flight_safety_check_timeout(&s,16u,32u));
    return 0;
}
