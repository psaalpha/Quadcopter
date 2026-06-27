#include "stm32f10x.h"                  // Device header


void IWDG_Init(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_8);
    IWDG_SetReload(499);
    IWDG_ReloadCounter();  // 第一次喂狗
    IWDG_Enable();         // 开启看门狗
}
