#ifndef __BMP390_H
#define __BMP390_H

#include "stm32f10x.h"

typedef struct {
    float par_t1, par_t2, par_t3;
    float par_p1, par_p2, par_p3, par_p4;
    float par_p5, par_p6, par_p7, par_p8;
    float par_p9, par_p10, par_p11;
    float t_lin;
} BMP390_Calib;

typedef struct {
    float temp;
    float press;
    float alt;
} BMP390_Data;

void SPI1_Init(void);
void BMP390_Init(void);
BMP390_Data BMP390_GetData(void);

#endif
