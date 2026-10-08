#include "slave_app.h"
#include "slave_scheduler.h"
#include "slave_board.h"
#include "master_link.h"
#include "board_inputs.h"
#include "optical_flow.h"
#include "qmc5883p.h"
#include "oled.h"
#include "at7456e.h"
#include "bmp3.h"
#include "bmp390.h"
#include "Delay.h"
#include <stdio.h>
#include <math.h>
typedef enum { CALIB_IDLE, CALIB_COLLECTING, CALIB_SAVE_PENDING } calib_state_t;
static calib_state_t calib_state;
static uint32_t calib_started_ms;
static uint8_t mag_ok;
static uint8_t baro_available;
static uint8_t baro_valid;
static uint8_t mag_sample_new;
static uint8_t zero_pending;
static uint8_t key_state;
static uint32_t key_changed_ms;
static uint16_t packet_sequence;
static float pressure, temperature, relative_altitude_m, battery_voltage;
static struct bmp3_dev barometer;
static slave_app_stats_t statistics;
static char display_lines[4][17];
static char displayed_lines[4][16];
static uint8_t display_cursor = 64u;
static uint8_t osd_step = 4u;
static optical_flow_data_t display_flow;
static float display_yaw, display_temperature, display_battery;
/* 气压高度滤波：滑动均值 + 慢速基线漂移滤波。 */
#define FILTER_WINDOW_SIZE  8

static double filter_buffer[FILTER_WINDOW_SIZE] = {0};
static int    filter_index  = 0;
static int    filter_filled = 0;

static double moving_average_filter(double new_val)
{
    filter_buffer[filter_index] = new_val;
    filter_index = (filter_index + 1) % FILTER_WINDOW_SIZE;
    if (filter_index == 0) filter_filled = 1;

    int    count = filter_filled ? FILTER_WINDOW_SIZE : filter_index;
    double sum   = 0.0;
    for (int i = 0; i < count; i++) sum += filter_buffer[i];
    return sum / count;
}

/* 长时间常数基线滤波，用于抑制天气和气压缓慢漂移。 */
#define BASELINE_ALPHA  0.00025

static double altitude_baseline   = 0.0;
static int    baseline_initialized = 0;
static double ref_altitude        = 0.0;

static double drift_filter(double raw_altitude)
{
    if (!baseline_initialized) {
        altitude_baseline   = raw_altitude;
        baseline_initialized = 1;
        return raw_altitude;
    }
    altitude_baseline += BASELINE_ALPHA * (raw_altitude - altitude_baseline);
    return raw_altitude - altitude_baseline;
}

static int32_t scale_float(float value,
                          float scale,
                          int32_t minimum,
                          int32_t maximum)
{
    float scaled = value * scale;
    if (scaled != scaled) return 0; /* Avoid undefined NaN-to-integer conversion. */

    if (scaled <= (float)minimum) {
        return minimum;
    }
    if (scaled >= (float)maximum) {
        return maximum;
    }
    if (scaled >= 0.0f) {
        scaled += 0.5f;
    } else {
        scaled -= 0.5f;
    }
    return (int32_t)scaled;
}


static uint8_t barometer_init(void)
{
    struct bmp3_settings settings;
    uint32_t desired = BMP3_SEL_PRESS_EN | BMP3_SEL_TEMP_EN |
        BMP3_SEL_PRESS_OS | BMP3_SEL_TEMP_OS | BMP3_SEL_IIR_FILTER | BMP3_SEL_ODR;
    barometer.intf = BMP3_SPI_INTF;
    barometer.read = bmp390_spi_read;
    barometer.write = bmp390_spi_write;
    barometer.delay_us = bmp390_delay_us;
    barometer.intf_ptr = &barometer;
    if (bmp3_init(&barometer) != BMP3_OK) return 0u;
    if (bmp3_get_sensor_settings(&settings, &barometer) != BMP3_OK) return 0u;
    settings.press_en = BMP3_ENABLE;
    settings.temp_en = BMP3_ENABLE;
    settings.odr_filter.press_os = 3u;
    settings.odr_filter.temp_os = 1u;
    settings.odr_filter.iir_filter = 3u;
    settings.odr_filter.odr = BMP3_ODR_25_HZ;
    if (bmp3_set_sensor_settings(desired, &settings, &barometer) != BMP3_OK) return 0u;
    settings.op_mode = BMP3_MODE_NORMAL;
    if (bmp3_set_op_mode(&settings, &barometer) != BMP3_OK) return 0u;
    Delay_ms(200);
    return 1u;
}
void slave_app_init(void)
{
    slave_board_init_interrupts();
    slave_scheduler_init();
    bmp390_spi_init();
    optical_flow_init();
    master_link_init();
    at7456e_init();
    osd_init();
    oled_init();
    oled_clear();
    baro_available = barometer_init();
    mag_ok = qmc5883p_init() == 0u;
    slave_board_init();
    slave_board_start_tick();
}
static void process_inputs(uint32_t now_ms)
{
    board_input_events_t events;
    board_inputs_take(&events);
    if (events.servo_changed != 0u) {
        slave_board_set_servo(board_inputs_servo_level() != 0u ? SLAVE_SERVO_HIGH_US : SLAVE_SERVO_LOW_US);
    }
    if (events.calibration_pulse != 0u && calib_state == CALIB_IDLE && mag_ok != 0u) {
        calib_state = CALIB_COLLECTING;
        calib_started_ms = now_ms;
        qmc5883p_calibrate_start();
    }
    if (events.key != 0u && key_state == 0u) {
        key_state = 1u;
        key_changed_ms = now_ms;
    }
    if (key_state == 1u && (uint32_t)(now_ms - key_changed_ms) >= SLAVE_KEY_DEBOUNCE_MS) {
        if (board_inputs_key_level() == 0u) {
            zero_pending = 1u;
            key_state = 2u;
        } else key_state = 0u;
    } else if (key_state == 2u && board_inputs_key_level() != 0u) {
        key_state = 3u;
        key_changed_ms = now_ms;
    } else if (key_state == 3u) {
        if (board_inputs_key_level() == 0u) key_state = 2u;
        else if ((uint32_t)(now_ms - key_changed_ms) >= SLAVE_KEY_DEBOUNCE_MS) key_state = 0u;
    }
}
static void process_calibration(uint32_t now_ms)
{
    if (calib_state == CALIB_COLLECTING &&
        (uint32_t)(now_ms - calib_started_ms) >= SLAVE_CALIBRATION_MS) {
        calib_state = CALIB_SAVE_PENDING;
    }
    if (calib_state == CALIB_SAVE_PENDING && master_link_is_busy() == 0u) {
        slave_board_feed_watchdog();
        optical_flow_pause(); /* Flash may mask IRQs longer than one DMA ring. */
        if (qmc5883p_calibrate_end() == 0u) statistics.calibration_saved++;
        else statistics.calibration_errors++;
        optical_flow_resume();
        calib_state = CALIB_IDLE;
    }
}
static void publish_packet(uint32_t now_ms)
{
    inter_mcu_sensor_data_t packet;
    optical_flow_data_t flow;
    optical_flow_get_snapshot(&flow);
    packet.sequence = packet_sequence++;
    packet.flags = 0u;
    if (baro_valid != 0u) packet.flags |= INTER_MCU_SENSOR_FLAG_BARO_VALID;
    if (mag_ok != 0u && calib_state == CALIB_IDLE && mag_sample_new != 0u)
        packet.flags |= INTER_MCU_SENSOR_FLAG_MAG_VALID;
    if (flow.data_valid != 0u) packet.flags |= INTER_MCU_SENSOR_FLAG_FLOW_VALID;
    if (slave_board_battery_valid() != 0u && battery_voltage > 0.5f) {
        packet.flags |= INTER_MCU_SENSOR_FLAG_BATTERY_VALID;
        if (battery_voltage < SLAVE_LOW_BATTERY_V) packet.flags |= INTER_MCU_SENSOR_FLAG_LOW_BATTERY;
    }
    if (calib_state != CALIB_IDLE) packet.flags |= INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING;
    packet.timestamp_ms = now_ms;
    packet.pressure_pa = scale_float(pressure, 1.0f, 0, 200000);
    packet.temperature_centi_c = (int16_t)scale_float(temperature, 100.0f, -32768, 32767);
    packet.baro_altitude_mm = scale_float(relative_altitude_m, 1000.0f, -100000000, 100000000);
    packet.yaw_centi_deg = (uint16_t)scale_float(qmc5883p_yaw_deg, 100.0f, 0, 35999);
    packet.flow_x = flow.flow_x;
    packet.flow_y = flow.flow_y;
    packet.flow_distance_mm = flow.distance;
    packet.flow_quality = flow.signal_strength;
    packet.battery_mv = (uint16_t)scale_float(battery_voltage, 1000.0f, 0, 65535);
    master_link_send(&packet, now_ms);
}
static void run_sensor_task(uint32_t now_ms)
{
    struct bmp3_data data;
    mag_sample_new = 0u;
    slave_board_request_battery(now_ms);
    baro_valid = 0u;
    if (baro_available != 0u &&
        bmp3_get_sensor_data(BMP3_PRESS_TEMP, &data, &barometer) == BMP3_OK &&
        data.pressure > 0.0 && data.pressure < 200000.0) {
        double filtered_pressure;
        double altitude;
        double drift_free_alt;
        pressure = (float)data.pressure;
        temperature = (float)data.temperature;
        filtered_pressure = moving_average_filter(pressure);
        altitude = 44330.0 * (1.0 - pow(filtered_pressure / 101325.0, 0.190295));
        drift_free_alt = drift_filter(altitude);
        if (zero_pending != 0u) {
            ref_altitude = drift_free_alt;
            zero_pending = 0u;
        }
        relative_altitude_m = (float)(drift_free_alt - ref_altitude);
        baro_valid = 1u;
    } else statistics.barometer_errors++;
    if (mag_ok != 0u) {
        if (calib_state == CALIB_COLLECTING) qmc5883p_calibrate_collect();
        else if (calib_state == CALIB_IDLE) mag_sample_new = qmc5883p_update_yaw();
    }
    slave_board_process_battery(slave_scheduler_now_ms());
    battery_voltage = slave_board_battery_voltage();
    slave_board_set_low_battery(slave_board_battery_valid() != 0u &&
        battery_voltage > 0.5f && battery_voltage < SLAVE_LOW_BATTERY_V);
    publish_packet(now_ms);

}
static void prepare_display_line(uint8_t row, const char *text)
{
    uint8_t i = 0u;
    char *line = display_lines[row - 1u];
    while (i < 16u && text[i] != '\0') { line[i] = text[i]; i++; }
    while (i < 16u) line[i++] = ' ';
    line[16] = '\0';
}
static void run_display_task(uint32_t now_ms)
{
    char text[40];
    optical_flow_data_t flow;
    if (display_cursor < 64u || osd_step < 4u) statistics.display_restarts++;
    optical_flow_get_snapshot(&flow);
    display_flow = flow;
    display_yaw = qmc5883p_yaw_deg;
    display_temperature = temperature;
    display_battery = battery_voltage;
    display_cursor = 0u;
    osd_step = 0u;
    if (baro_valid != 0u) {
        snprintf(text, sizeof(text), "T:%.1fC          ", (double)temperature);
        prepare_display_line(1, text);
        snprintf(text, sizeof(text), "H:%.1fcm        ", (double)relative_altitude_m * 100.0);
        prepare_display_line(2, text);
    } else {
        prepare_display_line(1, "BMP390 ERR");
        prepare_display_line(2, "H: invalid");
    }
    snprintf(text, sizeof(text), "Yaw:%.1f        ", (double)qmc5883p_yaw_deg);
    prepare_display_line(3, text);
    if (calib_state != CALIB_IDLE) {
        uint32_t elapsed = now_ms - calib_started_ms;
        uint32_t remaining = elapsed < SLAVE_CALIBRATION_MS ? SLAVE_CALIBRATION_MS - elapsed : 0u;
        snprintf(text, sizeof(text), "CAL:%lu s       ", (unsigned long)((remaining + 999u) / 1000u));
    } else snprintf(text, sizeof(text), "D:%.1fcm F:%ld", (double)flow.distance / 10.0, (long)flow.flow_x);
    prepare_display_line(4, text);
}
/* At most one OSD field or one OLED character per main-loop turn.
 * Periodic 5Hz tasks prepare the newest display snapshot, never a whole
 * blocking screen transfer. UART and input services run between chunks.
 */
static void process_display(void)
{
    if (osd_step < 4u) {
        switch (osd_step++) {
        case 0u: osd_display_int_padded(0, 7, 5, display_flow.distance); break;
        case 1u: osd_display_float(1, 4, 3, 1, display_yaw); break;
        case 2u: osd_display_int_padded(15, 13, 4, (int)(display_temperature * 10.0f)); break;
        default: osd_display_float(14, 3, 1, 1, display_battery); break;
        }
        return;
    }
    while (display_cursor < 64u) {
        uint8_t row = display_cursor / 16u;
        uint8_t column = display_cursor % 16u;
        display_cursor++;
        if (display_lines[row][column] != displayed_lines[row][column]) {
            oled_show_char(row + 1u, column + 1u, display_lines[row][column]);
            displayed_lines[row][column] = display_lines[row][column];
            return;
        }
    }
}
void slave_app_process(void)
{
    uint32_t now_ms = slave_scheduler_now_ms();
    slave_board_feed_watchdog();
    master_link_process(now_ms);
    slave_board_process_battery(now_ms);
    optical_flow_process(now_ms);
    process_inputs(now_ms);
    process_calibration(now_ms);
    now_ms = slave_scheduler_now_ms(); /* Flash save may have advanced the clock. */
    if (slave_scheduler_take(SLAVE_TASK_SENSORS) != 0u) run_sensor_task(now_ms);
    if (slave_scheduler_take(SLAVE_TASK_DISPLAY) != 0u) run_display_task(slave_scheduler_now_ms());
    process_display();
}
void slave_app_get_stats(slave_app_stats_t *stats) { if (stats != 0) *stats = statistics; }
