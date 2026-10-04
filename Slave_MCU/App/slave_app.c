#include "slave_app.h"
#include "slave_scheduler.h"
#include "slave_board.h"
#include "slave_link.h"
#include "exti.h"
#include "OpticalFlow.h"
#include "QMC5883P.h"
#include "OLED.h"
#include "AT7456E.h"
#include "bmp3.h"
#include "BMP390.h"
#include "Delay.h"
#include <stdio.h>
#include <math.h>
typedef enum { CALIB_IDLE, CALIB_COLLECTING, CALIB_SAVE_PENDING } CalibState;
static CalibState calib_state;
static uint32_t calib_started_ms;
static uint8_t mag_ok;
static uint8_t baro_available;
static uint8_t baro_valid;
static uint8_t mag_sample_new;
static uint8_t zero_pending;
static uint8_t key_state;
static uint32_t key_changed_ms;
static uint16_t packet_sequence;
static float pressure, temperature, rela_altitude, battery_voltage;
static struct bmp3_dev barometer;
static SlaveAppStats statistics;
static char display_lines[4][17];
static char displayed_lines[4][16];
static uint8_t display_cursor = 64u;
static uint8_t osd_step = 4u;
static OpticalFlow_Data_t display_flow;
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

static int32_t ScaleFloat(float value,
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


static uint8_t Barometer_Init(void)
{
    struct bmp3_settings settings;
    uint32_t desired = BMP3_SEL_PRESS_EN | BMP3_SEL_TEMP_EN |
        BMP3_SEL_PRESS_OS | BMP3_SEL_TEMP_OS | BMP3_SEL_IIR_FILTER | BMP3_SEL_ODR;
    barometer.intf = BMP3_SPI_INTF;
    barometer.read = bmp3_spi_read;
    barometer.write = bmp3_spi_write;
    barometer.delay_us = BMP390_DelayUs;
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
void SlaveApp_Init(void)
{
    SlaveBoard_InitInterrupts();
    SlaveScheduler_Init();
    SPI1_Init();
    OpticalFlow_Init();
    SlaveLink_Init();
    AT7456E_Init();
    OSD_Init();
    OLED_Init();
    OLED_Clear();
    baro_available = Barometer_Init();
    mag_ok = QMC5883P_Init() == 0u;
    SlaveBoard_Init();
    SlaveBoard_StartTick();
}
static void ProcessInputs(uint32_t now_ms)
{
    SlaveInputEvents events;
    EXTI_Inputs_Take(&events);
    if (events.servo_changed != 0u) {
        SlaveBoard_SetServo(EXTI_ServoLevel() != 0u ? SLAVE_SERVO_HIGH_US : SLAVE_SERVO_LOW_US);
    }
    if (events.calibration_pulse != 0u && calib_state == CALIB_IDLE && mag_ok != 0u) {
        calib_state = CALIB_COLLECTING;
        calib_started_ms = now_ms;
        QMC5883P_Calibrate_Start();
    }
    if (events.key != 0u && key_state == 0u) {
        key_state = 1u;
        key_changed_ms = now_ms;
    }
    if (key_state == 1u && (uint32_t)(now_ms - key_changed_ms) >= SLAVE_KEY_DEBOUNCE_MS) {
        if (EXTI_KeyLevel() == 0u) {
            zero_pending = 1u;
            key_state = 2u;
        } else key_state = 0u;
    } else if (key_state == 2u && EXTI_KeyLevel() != 0u) {
        key_state = 3u;
        key_changed_ms = now_ms;
    } else if (key_state == 3u) {
        if (EXTI_KeyLevel() == 0u) key_state = 2u;
        else if ((uint32_t)(now_ms - key_changed_ms) >= SLAVE_KEY_DEBOUNCE_MS) key_state = 0u;
    }
}
static void ProcessCalibration(uint32_t now_ms)
{
    if (calib_state == CALIB_COLLECTING &&
        (uint32_t)(now_ms - calib_started_ms) >= SLAVE_CALIBRATION_MS) {
        calib_state = CALIB_SAVE_PENDING;
    }
    if (calib_state == CALIB_SAVE_PENDING && SlaveLink_IsBusy() == 0u) {
        SlaveBoard_FeedWatchdog();
        OpticalFlow_Pause(); /* Flash may mask IRQs longer than one DMA ring. */
        if (QMC5883P_Calibrate_End() == 0u) statistics.calibration_saved++;
        else statistics.calibration_errors++;
        OpticalFlow_Resume();
        calib_state = CALIB_IDLE;
    }
}
static void PublishPacket(uint32_t now_ms)
{
    InterMcuSensorData packet;
    OpticalFlow_Data_t flow;
    OpticalFlow_GetSnapshot(&flow);
    packet.sequence = packet_sequence++;
    packet.flags = 0u;
    if (baro_valid != 0u) packet.flags |= INTER_MCU_SENSOR_FLAG_BARO_VALID;
    if (mag_ok != 0u && calib_state == CALIB_IDLE && mag_sample_new != 0u)
        packet.flags |= INTER_MCU_SENSOR_FLAG_MAG_VALID;
    if (flow.data_valid != 0u) packet.flags |= INTER_MCU_SENSOR_FLAG_FLOW_VALID;
    if (SlaveBoard_BatteryValid() != 0u && battery_voltage > 0.5f) {
        packet.flags |= INTER_MCU_SENSOR_FLAG_BATTERY_VALID;
        if (battery_voltage < SLAVE_LOW_BATTERY_V) packet.flags |= INTER_MCU_SENSOR_FLAG_LOW_BATTERY;
    }
    if (calib_state != CALIB_IDLE) packet.flags |= INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING;
    packet.timestamp_ms = now_ms;
    packet.pressure_pa = ScaleFloat(pressure, 1.0f, 0, 200000);
    packet.temperature_centi_c = (int16_t)ScaleFloat(temperature, 100.0f, -32768, 32767);
    packet.baro_altitude_mm = ScaleFloat(rela_altitude, 1000.0f, -100000000, 100000000);
    packet.yaw_centi_deg = (uint16_t)ScaleFloat(QMC5883P_Yaw, 100.0f, 0, 35999);
    packet.flow_x = flow.flow_x;
    packet.flow_y = flow.flow_y;
    packet.flow_distance_mm = flow.distance;
    packet.flow_quality = flow.signal_strength;
    packet.battery_mv = (uint16_t)ScaleFloat(battery_voltage, 1000.0f, 0, 65535);
    SlaveLink_Send(&packet, now_ms);
}
static void RunSensorTask(uint32_t now_ms)
{
    struct bmp3_data data;
    mag_sample_new = 0u;
    SlaveBoard_RequestBattery(now_ms);
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
        rela_altitude = (float)(drift_free_alt - ref_altitude);
        baro_valid = 1u;
    } else statistics.barometer_errors++;
    if (mag_ok != 0u) {
        if (calib_state == CALIB_COLLECTING) QMC5883P_Calibrate_Collect();
        else if (calib_state == CALIB_IDLE) mag_sample_new = QMC5883P_UpdateYaw();
    }
    SlaveBoard_ProcessBattery(SlaveScheduler_Now());
    battery_voltage = SlaveBoard_BatteryVoltage();
    SlaveBoard_SetLowBattery(SlaveBoard_BatteryValid() != 0u &&
        battery_voltage > 0.5f && battery_voltage < SLAVE_LOW_BATTERY_V);
    PublishPacket(now_ms);

}
static void PrepareDisplayLine(uint8_t row, const char *text)
{
    uint8_t i = 0u;
    char *line = display_lines[row - 1u];
    while (i < 16u && text[i] != '\0') { line[i] = text[i]; i++; }
    while (i < 16u) line[i++] = ' ';
    line[16] = '\0';
}
static void RunDisplayTask(uint32_t now_ms)
{
    char text[40];
    OpticalFlow_Data_t flow;
    if (display_cursor < 64u || osd_step < 4u) statistics.display_restarts++;
    OpticalFlow_GetSnapshot(&flow);
    display_flow = flow;
    display_yaw = QMC5883P_Yaw;
    display_temperature = temperature;
    display_battery = battery_voltage;
    display_cursor = 0u;
    osd_step = 0u;
    if (baro_valid != 0u) {
        snprintf(text, sizeof(text), "T:%.1fC          ", (double)temperature);
        PrepareDisplayLine(1, text);
        snprintf(text, sizeof(text), "H:%.1fcm        ", (double)rela_altitude * 100.0);
        PrepareDisplayLine(2, text);
    } else {
        PrepareDisplayLine(1, "BMP390 ERR");
        PrepareDisplayLine(2, "H: invalid");
    }
    snprintf(text, sizeof(text), "Yaw:%.1f        ", (double)QMC5883P_Yaw);
    PrepareDisplayLine(3, text);
    if (calib_state != CALIB_IDLE) {
        uint32_t elapsed = now_ms - calib_started_ms;
        uint32_t remaining = elapsed < SLAVE_CALIBRATION_MS ? SLAVE_CALIBRATION_MS - elapsed : 0u;
        snprintf(text, sizeof(text), "CAL:%lu s       ", (unsigned long)((remaining + 999u) / 1000u));
    } else snprintf(text, sizeof(text), "D:%.1fcm F:%ld", (double)flow.distance / 10.0, (long)flow.flow_x);
    PrepareDisplayLine(4, text);
}
/* At most one OSD field or one OLED character per main-loop turn.
 * Periodic 5Hz tasks prepare the newest display snapshot, never a whole
 * blocking screen transfer. UART and input services run between chunks.
 */
static void ProcessDisplay(void)
{
    if (osd_step < 4u) {
        switch (osd_step++) {
        case 0u: OSD_DisplayInt(0, 7, 5, display_flow.distance); break;
        case 1u: OSD_DisplayFloat(1, 4, 3, 1, display_yaw); break;
        case 2u: OSD_DisplayInt(15, 13, 4, (int)(display_temperature * 10.0f)); break;
        default: OSD_DisplayFloat(14, 3, 1, 1, display_battery); break;
        }
        return;
    }
    while (display_cursor < 64u) {
        uint8_t row = display_cursor / 16u;
        uint8_t column = display_cursor % 16u;
        display_cursor++;
        if (display_lines[row][column] != displayed_lines[row][column]) {
            OLED_ShowChar(row + 1u, column + 1u, display_lines[row][column]);
            displayed_lines[row][column] = display_lines[row][column];
            return;
        }
    }
}
void SlaveApp_Process(void)
{
    uint32_t now_ms = SlaveScheduler_Now();
    SlaveBoard_FeedWatchdog();
    SlaveLink_Process(now_ms);
    SlaveBoard_ProcessBattery(now_ms);
    OpticalFlow_Process(now_ms);
    ProcessInputs(now_ms);
    ProcessCalibration(now_ms);
    now_ms = SlaveScheduler_Now(); /* Flash save may have advanced the clock. */
    if (SlaveScheduler_Take(SLAVE_TASK_SENSORS) != 0u) RunSensorTask(now_ms);
    if (SlaveScheduler_Take(SLAVE_TASK_DISPLAY) != 0u) RunDisplayTask(SlaveScheduler_Now());
    ProcessDisplay();
}
void SlaveApp_GetStats(SlaveAppStats *stats) { if (stats != 0) *stats = statistics; }
