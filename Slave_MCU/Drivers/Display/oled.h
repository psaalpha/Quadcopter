#ifndef SLAVE_MCU_DRIVERS_DISPLAY_OLED_H
#define SLAVE_MCU_DRIVERS_DISPLAY_OLED_H

#include <stdint.h>

void oled_init(void);
void oled_clear(void);
void oled_show_char(uint8_t line, uint8_t column, char character);
void oled_show_string(uint8_t line, uint8_t column, char *string);
void oled_show_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
void oled_show_signed_num(uint8_t line, uint8_t column, int32_t number, uint8_t length);
void oled_show_hex_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
void oled_show_bin_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
void oled_show_float(uint8_t line, uint8_t column, float value, uint8_t integer_len);
#endif
