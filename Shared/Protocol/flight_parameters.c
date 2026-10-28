#include "flight_parameters.h"
#include <string.h>
static int decimal(const char *text,float *value)
{
    const char *p=text;float result=0.0f,factor=.1f;int sign=1,exponent=0,exponent_sign=1;unsigned digits=0u;
    if(*p=='-' || *p=='+'){if(*p=='-')sign=-1;p++;}
    while(*p>='0' && *p<='9') {
        if(++digits>9u)return 0;
        result=result*10.0f+(float)(*p++-'0');
    }
    if(*p=='.') {
        p++;
        while(*p>='0' && *p<='9'){result+=(float)(*p++-'0')*factor;factor*=.1f;digits++;}
    }
    if(digits==0u)return 0;
    if(*p=='e' || *p=='E') {
        p++;
        if(*p=='-' || *p=='+'){if(*p=='-')exponent_sign=-1;p++;}
        if(*p<'0' || *p>'9')return 0;
        while(*p>='0' && *p<='9'){exponent=exponent*10+(*p++-'0');if(exponent>6)return 0;}
        while(exponent--){result=exponent_sign>0?result*10.0f:result*.1f;}
    }
    if(*p!='\0')return 0;
    *value=(float)sign*result;return 1;
}
static const flight_parameter_info_t info[FP_COUNT]={
 {"PKp",.179f,0,40,1},{"PKi",.1058f,0,1,1},{"PKd",.1838f,0,5,1},{"Mid",0,-10,10,1},
 {"RKp",.8f,0,10,1},{"RKi",.105f,0,10,1},{"RKd",.18f,0,10,1},
 {"YKp",2.87f,0,10,1},{"YKi",.0251f,0,10,1},{"YKd",.0034444f,0,10,1},
 {"PAKp",6.03f,0,40,1},{"RAKp",12.03f,0,40,1},{"YAKp",5.72f,0,40,1},
 {"PAIM",0,-30,30,0},{"RAIM",0,-30,30,0},{"Contrl_Speed",0,0,20,0},
 {"AKp",0,0,5,1},{"AKi",0,0,2,1},{"AKd",0,0,5,1},
 {"XKp",0,0,5,1},{"XKi",0,0,2,1},{"XKd",0,0,5,1},
 {"YKpPos",0,0,5,1},{"YKiPos",0,0,2,1},{"YKdPos",0,0,5,1},
 {"FlowXScale",1,-5,5,1},{"FlowYScale",1,-5,5,1},{"FlowYaw",0,-180,180,1},
 {"Hover",35,5,80,1},{"BatWarn",10.5f,6,26,1},{"BatCrit",9.9f,6,26,1},
 {"RollOffset",2,-15,15,1},{"PitchOffset",-1,-15,15,1},{"Declination",0,-180,180,1},
 {"NavEnable",0,0,1,1},{"GyroX",-64,-2000,2000,1},{"GyroY",-29,-2000,2000,1},{"GyroZ",-43,-2000,2000,1}
};
const flight_parameter_info_t *flight_parameter_info(unsigned id) { return id<FP_COUNT?&info[id]:0; }
void flight_parameters_defaults(flight_parameters_t *p)
{ unsigned i;for(i=0;i<FP_COUNT;i++)p->value[i]=info[i].initial;p->revision=1u;p->dirty=0u; }
int flight_parameter_set(flight_parameters_t *p,unsigned id,float value)
{
    if(id>=FP_COUNT || !(value>=info[id].minimum && value<=info[id].maximum))return 0;
    if((id==FP_NAV_ENABLE && value!=0.0f && value!=1.0f) ||
       ((id==FP_FLOW_X_SCALE || id==FP_FLOW_Y_SCALE) && value>-0.01f && value<0.01f))return 0;
    if(id==FP_BAT_WARN && value<=p->value[FP_BAT_CRIT])return 0;
    if(id==FP_BAT_CRIT && value>=p->value[FP_BAT_WARN])return 0;
    if(id==FP_NAV_ENABLE && value!=0 && (p->value[FP_AKP]<=0 || p->value[FP_XKP]<=0 || p->value[FP_YXKP]<=0))return 0;
    if(p->value[FP_NAV_ENABLE]!=0 && (id==FP_AKP || id==FP_XKP || id==FP_YXKP) && value<=0)return 0;
    p->value[id]=value;p->revision++;if(info[id].persistent)p->dirty=1u;return 1;
}
int flight_command_parse(const char *text,flight_command_t *c)
{
    char copy[100],*name,*value,*comma;unsigned id;size_t length=strlen(text);
    if(length==0u || length>=sizeof(copy))return 0;
    memcpy(copy,text,length+1u);
    if(strcmp(copy,"save")==0)c->op=FC_SAVE;
    else if(strcmp(copy,"load")==0)c->op=FC_LOAD;
    else if(strcmp(copy,"defaults")==0)c->op=FC_DEFAULTS;
    else if(strcmp(copy,"calibrate,gyro")==0)c->op=FC_GYRO_CAL;
    else if(strcmp(copy,"zero,yaw")==0)c->op=FC_YAW_ZERO;
    else if(strcmp(copy,"status")==0)c->op=FC_STATUS;
    else {
        comma=strchr(copy,',');if(comma==0)return 0;*comma++='\0';name=comma;
        if(strcmp(copy,"bench")==0) {
            c->op=FC_BENCH;
            return decimal(name,&c->value) && (c->value==0 || c->value==1);
        }
        if(strcmp(copy,"get")==0) {
            c->op=FC_GET;value=0;
        } else if(strcmp(copy,"slider")==0 || strcmp(copy,"set")==0) {
            comma=strchr(name,',');if(comma==0)return 0;*comma++='\0';value=comma;c->op=FC_SET;
        } else return 0;
        for(id=0;id<FP_COUNT;id++)if(strcmp(name,info[id].name)==0)break;
        if(id==FP_COUNT)return 0;
        c->id=(uint8_t)id;
        if(value) {
            if(!decimal(value,&c->value) || !(c->value>=info[id].minimum && c->value<=info[id].maximum))return 0;
        }
    }
    return 1;
}
static void put32(uint8_t *p,uint32_t u)
{ p[0]=(uint8_t)u;p[1]=(uint8_t)(u>>8);p[2]=(uint8_t)(u>>16);p[3]=(uint8_t)(u>>24); }
static uint32_t get32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void flight_parameters_encode(const flight_parameters_t *p,uint8_t *b)
{
    unsigned i;put32(b,1u);
    for(i=0;i<FP_COUNT;i++) { uint32_t u;float v=info[i].persistent?p->value[i]:info[i].initial;memcpy(&u,&v,4);put32(b+4+4*i,u); }
}
int flight_parameters_decode(flight_parameters_t *p,const uint8_t *b,unsigned size)
{
    flight_parameters_t temporary;unsigned i;
    if(size!=FLIGHT_PARAMETERS_BYTES || get32(b)!=1u)return 0;
    flight_parameters_defaults(&temporary);
    for(i=0;i<FP_COUNT;i++) {
        uint32_t u=get32(b+4+4*i);float v;memcpy(&v,&u,4);
        if(!(v>=info[i].minimum && v<=info[i].maximum))return 0;
        if(info[i].persistent)temporary.value[i]=v;
    }
    if(temporary.value[FP_BAT_CRIT]>=temporary.value[FP_BAT_WARN] ||
       (temporary.value[FP_NAV_ENABLE]!=0 && temporary.value[FP_NAV_ENABLE]!=1) ||
       (temporary.value[FP_NAV_ENABLE]!=0 && (temporary.value[FP_AKP]<=0 || temporary.value[FP_XKP]<=0 || temporary.value[FP_YXKP]<=0)) ||
       (temporary.value[FP_FLOW_X_SCALE]>-.01f && temporary.value[FP_FLOW_X_SCALE]<.01f) ||
       (temporary.value[FP_FLOW_Y_SCALE]>-.01f && temporary.value[FP_FLOW_Y_SCALE]<.01f))return 0;
    temporary.revision=p->revision+1u;temporary.dirty=0u;*p=temporary;return 1;
}
