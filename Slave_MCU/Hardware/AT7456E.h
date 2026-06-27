#ifndef __AT7456E_H
#define __AT7456E_H

#include "stm32f10x.h"

/* AT7456E 底层驱动函数 */
void AT7456E_Init(void);                    // AT7456E 硬件初始化
void AT7456E_ClearSRAM(void);               // 清除显存
void AT7456E_WriteSRAM(uint8_t row, uint8_t columns, uint8_t addr);  // 单字节写入显存
void AT7456E_OSD_On(void);                  // 打开OSD显示
void AT7456E_OSD_Off(void);                 // 关闭OSD显示

/* OSD 显示相关函数 */
void OSD_Init(void);                        // OSD 显示框架初始化
void OSD_DisplayInt(uint8_t row, uint8_t columns, uint8_t number_len, int32_t number);       // 显示整数（0占位）
void OSD_DisplayInt_1(uint8_t row, uint8_t columns, uint8_t number_len, int32_t number);      // 显示整数（无0占位）
void OSD_DisplayFloat(uint8_t row, uint8_t columns, uint8_t int_n, uint8_t float_n, float number);  // 显示浮点数

/* 寄存器地址定义 */
#define VM0                     0x00        // Video Mode0
#define VM1                     0x01        // Video Mode1
#define HOS                     0x02        // Horizontal Offset
#define VOS                     0x03        // Vertical Offset
#define DMM                     0x04        // Display Memory Mode
#define DMAH                    0x05        // Display Memory Address High
#define DMAL                    0x06        // Display Memory Address Low
#define DMDI                    0x07        // Display Memory Data In
#define CMM                     0x08        // Character Memory Mode
#define CMAH                    0x09        // Character Memory Address High
#define CMAL                    0x0a        // Character Memory Address Low
#define CMDI                    0x0b        // Character Memory Data In
#define OSDM                    0x0c        // OSD Insertion Mux
#define RB0                     0x10        // Row 0 Brightness
#define RB1                     0x11        // Row 1 Brightness
#define RB2                     0x12        // Row 2 Brightness
#define RB3                     0x13        // Row 3 Brightness
#define RB4                     0x14        // Row 4 Brightness
#define RB5                     0x15        // Row 5 Brightness
#define RB6                     0x16        // Row 6 Brightness
#define RB7                     0x17        // Row 7 Brightness
#define RB8                     0x18        // Row 8 Brightness
#define RB9                     0x19        // Row 9 Brightness
#define RB10                    0x1a        // Row 10 Brightness
#define RB11                    0x1b        // Row 11 Brightness
#define RB12                    0x1c        // Row 12 Brightness
#define RB13                    0x1d        // Row 13 Brightness
#define RB14                    0x1e        // Row 14 Brightness
#define RB15                    0x1f        // Row 15 Brightness
#define OSDBL                   0x6c        // OSD Black Level
#define STAT                    0x20        // Status (read only)
#define DMDO                    0x30        // Display Memory Data Out (read only)
#define CMDO                    0x40        // Character Memory Data Out (read only)

#define NVM_RAM                 0x50        // 从NVM读取字库到镜像RAM
#define RAM_NVM                 0xa0        // 从镜像RAM写入字库到NVM

/* VM0 视频模式寄存器 */
#define NTSC                    (0 << 6)
#define PAL                     (1 << 6)
#define SYNC_AUTO               (0 << 4)
#define SYNC_EXTERNAL           (2 << 4)
#define SYNC_INTERNAL           (3 << 4)
#define OSD_ENABLE              (1 << 3)
#define OSD_DISABLE             (0 << 3)
#define SOFT_RESET              (1 << 1)
#define VOUT_ENABLE             (0 << 0)
#define VOUT_DISABLE            (1 << 0)

/* VM1 背景亮度 */
#define BACKGND_0               (0 << 4)
#define BACKGND_7               (1 << 4)
#define BACKGND_14              (2 << 4)
#define BACKGND_21              (3 << 4)
#define BACKGND_28              (4 << 4)
#define BACKGND_35              (5 << 4)
#define BACKGND_42              (6 << 4)
#define BACKGND_49              (7 << 4)

#define BLINK_TIME40            (0 << 2)    // 闪烁周期40ms(NTSC)
#define BLINK_TIME80            (1 << 2)
#define BLINK_TIME120           (2 << 2)
#define BLINK_TIME160           (3 << 2)

#define BLINK_DUTY_1_1          0           // BT : BT
#define BLINK_DUTY_1_2          1           // BT : 2BT
#define BLINK_DUTY_1_3          2           // BT : 3BT
#define BLINK_DUTY_3_1          3           // 3BT : BT

/* DMM 显示存储器模式 */
#define SPI_BIT16               (0 << 6)
#define SPI_BIT8                (1 << 6)
#define CHAR_LBC                (1 << 5)
#define CHAR_BLK                (1 << 4)
#define CHAR_INV                (1 << 3)
#define CLEAR_SRAM              (1 << 2)
#define VERTICAL_SYNC           (1 << 1)
#define AUTO_INC                (1 << 0)

/* RBi 行亮度 */
#define BLACK_LEVEL_0           (0 << 2)
#define BLACK_LEVEL_10          (1 << 2)
#define BLACK_LEVEL_20          (2 << 2)
#define BLACK_LEVEL_30          (3 << 2)
#define WHITE_LEVEL_120         (0 << 0)
#define WHITE_LEVEL_100         (1 << 0)
#define WHITE_LEVEL_90          (2 << 0)
#define WHITE_LEVEL_80          (3 << 0)

/* STAT 状态寄存器 */
#define PAL_DETECT              (1 << 0)
#define NTSC_DETECT             (1 << 1)
#define LOS_DETECT              (1 << 2)
#define VSYNC_FLAG              (1 << 4)

#endif /* __AT7456E_H */

