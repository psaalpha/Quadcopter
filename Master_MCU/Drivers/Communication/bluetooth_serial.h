#ifndef MASTER_MCU_DRIVERS_COMMUNICATION_BLUETOOTH_SERIAL_H
#define MASTER_MCU_DRIVERS_COMMUNICATION_BLUETOOTH_SERIAL_H

#include <stdio.h>
#include <stdint.h>
#include "flight_parameters.h"

extern char bluetooth_rx_packet[];
extern volatile uint8_t bluetooth_rx_ready;

void bluetooth_serial_init(void);
int bluetooth_serial_take_command(flight_command_t *command);
uint8_t bluetooth_serial_try_send(uint8_t *data,uint16_t length);
void bluetooth_serial_service_tx(uint32_t now_ms);
/* Main-loop service; returns 1 when one command is ready to parse. */
uint8_t bluetooth_serial_process(void);
uint32_t bluetooth_serial_get_rx_overruns(void);
uint32_t bluetooth_serial_get_rx_frame_errors(void);
void bluetooth_serial_send_byte(uint8_t byte);
void bluetooth_serial_send_array(uint8_t *array, uint16_t length);
void bluetooth_serial_send_string(char *string);
void bluetooth_serial_send_number(uint32_t number, uint8_t length);
void bluetooth_serial_printf(char *format, ...);
void bluetooth_serial_send_dma(uint8_t *buffer, uint16_t length);

#endif
