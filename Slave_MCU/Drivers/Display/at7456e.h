#ifndef SLAVE_MCU_DRIVERS_DISPLAY_AT7456E_H
#define SLAVE_MCU_DRIVERS_DISPLAY_AT7456E_H

#include "stm32f10x.h"

/* AT7456E 底层驱动函数 */
void at7456e_init(void);                    // AT7456E 硬件初始化
void at7456e_clear_sram(void);               // 清除显存
void at7456e_write_sram(uint8_t row, uint8_t columns, uint8_t addr);  // 单字节写入显存
void at7456e_osd_on(void);                  // 打开OSD显示
void at7456e_osd_off(void);                 // 关闭OSD显示

/* OSD 显示相关函数 */
void osd_init(void);                        // OSD 显示框架初始化
void osd_display_int_padded(uint8_t row, uint8_t columns, uint8_t number_len, int32_t number);       // 显示整数（0占位）
void osd_display_int(uint8_t row, uint8_t columns, uint8_t number_len, int32_t number);      // 显示整数（无0占位）
void osd_display_float(uint8_t row, uint8_t columns, uint8_t int_n, uint8_t float_n, float number);  // 显示浮点数

/* 寄存器地址定义 */
#define AT7456E_VM0                     0x00        // Video Mode0
#define AT7456E_VM1                     0x01        // Video Mode1
#define AT7456E_HOS                     0x02        // Horizontal Offset
#define AT7456E_VOS                     0x03        // Vertical Offset
#define AT7456E_DMM                     0x04        // Display Memory Mode
#define AT7456E_DMAH                    0x05        // Display Memory Address High
#define AT7456E_DMAL                    0x06        // Display Memory Address Low
#define AT7456E_DMDI                    0x07        // Display Memory data In
#define AT7456E_CMM                     0x08        // Character Memory Mode
#define AT7456E_CMAH                    0x09        // Character Memory Address High
#define AT7456E_CMAL                    0x0a        // Character Memory Address Low
#define AT7456E_CMDI                    0x0b        // Character Memory data In
#define AT7456E_OSDM                    0x0c        // OSD Insertion Mux
#define AT7456E_RB0                     0x10        // Row 0 Brightness
#define AT7456E_RB1                     0x11        // Row 1 Brightness
#define AT7456E_RB2                     0x12        // Row 2 Brightness
#define AT7456E_RB3                     0x13        // Row 3 Brightness
#define AT7456E_RB4                     0x14        // Row 4 Brightness
#define AT7456E_RB5                     0x15        // Row 5 Brightness
#define AT7456E_RB6                     0x16        // Row 6 Brightness
#define AT7456E_RB7                     0x17        // Row 7 Brightness
#define AT7456E_RB8                     0x18        // Row 8 Brightness
#define AT7456E_RB9                     0x19        // Row 9 Brightness
#define AT7456E_RB10                    0x1a        // Row 10 Brightness
#define AT7456E_RB11                    0x1b        // Row 11 Brightness
#define AT7456E_RB12                    0x1c        // Row 12 Brightness
#define AT7456E_RB13                    0x1d        // Row 13 Brightness
#define AT7456E_RB14                    0x1e        // Row 14 Brightness
#define AT7456E_RB15                    0x1f        // Row 15 Brightness
#define AT7456E_OSDBL                   0x6c        // OSD Black Level
#define AT7456E_STAT                    0x20        // Status (read only)
#define AT7456E_DMDO                    0x30        // Display Memory data Out (read only)
#define AT7456E_CMDO                    0x40        // Character Memory data Out (read only)

#define AT7456E_NVM_RAM                 0x50        // 从NVM读取字库到镜像RAM
#define AT7456E_RAM_NVM                 0xa0        // 从镜像RAM写入字库到NVM

/* AT7456E_VM0 视频模式寄存器 */
#define AT7456E_NTSC                    (0 << 6)
#define AT7456E_PAL                     (1 << 6)
#define AT7456E_SYNC_AUTO               (0 << 4)
#define AT7456E_SYNC_EXTERNAL           (2 << 4)
#define AT7456E_SYNC_INTERNAL           (3 << 4)
#define AT7456E_OSD_ENABLE              (1 << 3)
#define AT7456E_OSD_DISABLE             (0 << 3)
#define AT7456E_SOFT_RESET              (1 << 1)
#define AT7456E_VOUT_ENABLE             (0 << 0)
#define AT7456E_VOUT_DISABLE            (1 << 0)

/* AT7456E_VM1 背景亮度 */
#define AT7456E_BACKGND_0               (0 << 4)
#define AT7456E_BACKGND_7               (1 << 4)
#define AT7456E_BACKGND_14              (2 << 4)
#define AT7456E_BACKGND_21              (3 << 4)
#define AT7456E_BACKGND_28              (4 << 4)
#define AT7456E_BACKGND_35              (5 << 4)
#define AT7456E_BACKGND_42              (6 << 4)
#define AT7456E_BACKGND_49              (7 << 4)

#define AT7456E_BLINK_TIME40            (0 << 2)    // 闪烁周期40ms(AT7456E_NTSC)
#define AT7456E_BLINK_TIME80            (1 << 2)
#define AT7456E_BLINK_TIME120           (2 << 2)
#define AT7456E_BLINK_TIME160           (3 << 2)

#define AT7456E_BLINK_DUTY_1_1          0           // BT : BT
#define AT7456E_BLINK_DUTY_1_2          1           // BT : 2BT
#define AT7456E_BLINK_DUTY_1_3          2           // BT : 3BT
#define AT7456E_BLINK_DUTY_3_1          3           // 3BT : BT

/* AT7456E_DMM 显示存储器模式 */
#define AT7456E_SPI_BIT16               (0 << 6)
#define AT7456E_SPI_BIT8                (1 << 6)
#define AT7456E_CHAR_LBC                (1 << 5)
#define AT7456E_CHAR_BLK                (1 << 4)
#define AT7456E_CHAR_INV                (1 << 3)
#define AT7456E_CLEAR_SRAM              (1 << 2)
#define AT7456E_VERTICAL_SYNC           (1 << 1)
#define AT7456E_AUTO_INC                (1 << 0)

/* RBi 行亮度 */
#define AT7456E_BLACK_LEVEL_0           (0 << 2)
#define AT7456E_BLACK_LEVEL_10          (1 << 2)
#define AT7456E_BLACK_LEVEL_20          (2 << 2)
#define AT7456E_BLACK_LEVEL_30          (3 << 2)
#define AT7456E_WHITE_LEVEL_120         (0 << 0)
#define AT7456E_WHITE_LEVEL_100         (1 << 0)
#define AT7456E_WHITE_LEVEL_90          (2 << 0)
#define AT7456E_WHITE_LEVEL_80          (3 << 0)

/* AT7456E_STAT 状态寄存器 */
#define AT7456E_PAL_DETECT              (1 << 0)
#define AT7456E_NTSC_DETECT             (1 << 1)
#define AT7456E_LOS_DETECT              (1 << 2)
#define AT7456E_VSYNC_FLAG              (1 << 4)

uint32_t at7456e_get_spi_timeouts(void);

#endif /* SLAVE_MCU_DRIVERS_DISPLAY_AT7456E_H */
