#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp32/rom/ets_sys.h>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

extern "C" {
#include "tamalib.h"
#include "hw.h"
}

#include "esp32_hal.h"

/*
 * Hardware mapping for the ESP32 Cheap Yellow Display (CYD)
 */
#ifndef TAMALIB_LCD_BG
#define TAMALIB_LCD_BG    TFT_YELLOW
#endif

#ifndef TAMALIB_LCD_FG
#define TAMALIB_LCD_FG    TFT_BLACK
#endif

#ifndef TAMALIB_LCD_ICON_FG
#define TAMALIB_LCD_ICON_FG TFT_DARKGREY
#endif

#ifndef TAMALIB_BTN_LEFT_PIN
#define TAMALIB_BTN_LEFT_PIN   35
#endif

#ifndef TAMALIB_BTN_MIDDLE_PIN
#define TAMALIB_BTN_MIDDLE_PIN 0
#endif

#ifndef TAMALIB_BTN_RIGHT_PIN
#define TAMALIB_BTN_RIGHT_PIN  39
#endif

#ifndef TAMALIB_BTN_TAP_PIN
#define TAMALIB_BTN_TAP_PIN    34
#endif

#ifndef TAMALIB_BUZZER_PIN
#define TAMALIB_BUZZER_PIN     25
#endif

static constexpr u8_t kPixelSize = 6;
static constexpr u8_t kIconSize  = 8;
static constexpr u16_t kFrameIntervalUs = 33333; // ~30 fps
static constexpr u8_t kBuzzerChannel = 0;

static TFT_eSPI tft;
static bool s_lcd_matrix[LCD_WIDTH][LCD_HEIGHT];
static bool s_icons[ICON_NUM];
static timestamp_t s_last_frame = 0;
static bool s_dirty = false;
static u32_t s_freq_dhz = 0;
static btn_state_t s_last_buttons[BTN_TAP + 1] = {
    BTN_STATE_RELEASED,
    BTN_STATE_RELEASED,
    BTN_STATE_RELEASED,
    BTN_STATE_RELEASED
};

static timestamp_t esp32_get_timestamp(void)
{
    return (timestamp_t) esp_timer_get_time();
}

static void esp32_sleep_until(timestamp_t ts)
{
    timestamp_t now = esp32_get_timestamp();
    timestamp_t remaining = ts - now;

    while ((timestamp_t) remaining > 1000) {
        vTaskDelay(pdMS_TO_TICKS((remaining / 1000)));
        now = esp32_get_timestamp();
        remaining = ts - now;
    }

    while ((timestamp_t) remaining > 0) {
        ets_delay_us((uint32_t) ((remaining > 100) ? 100 : remaining));
        now = esp32_get_timestamp();
        remaining = ts - now;
    }
}

static void esp32_set_pixel(u8_t x, u8_t y, bool on)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }

    s_lcd_matrix[x][y] = on;
    s_dirty = true;
}

static void esp32_set_icon(u8_t icon, bool on)
{
    if (icon >= ICON_NUM) {
        return;
    }

    s_icons[icon] = on;
    s_dirty = true;
}

static void esp32_update_screen(void)
{
    timestamp_t now = esp32_get_timestamp();
    if ((timestamp_t) (now - s_last_frame) < kFrameIntervalUs) {
        return;
    }
    s_last_frame = now;

    if (!s_dirty) {
        return;
    }

    tft.startWrite();
    for (u8_t y = 0; y < LCD_HEIGHT; ++y) {
        for (u8_t x = 0; x < LCD_WIDTH; ++x) {
            uint16_t color = s_lcd_matrix[x][y] ? TAMALIB_LCD_FG : TAMALIB_LCD_BG;
            tft.fillRect(x * kPixelSize, y * kPixelSize, kPixelSize, kPixelSize, color);
        }
    }

    for (u8_t icon = 0; icon < ICON_NUM; ++icon) {
        uint16_t color = s_icons[icon] ? TAMALIB_LCD_ICON_FG : TAMALIB_LCD_BG;
        uint16_t base_x = icon * (kIconSize + 2);
        uint16_t base_y = LCD_HEIGHT * kPixelSize + 4;
        tft.fillRect(base_x, base_y, kIconSize, kIconSize, color);
    }
    tft.endWrite();

    s_dirty = false;
}

static void esp32_set_frequency(u32_t freq)
{
    s_freq_dhz = freq;
    if (freq == 0) {
        ledcWriteTone(kBuzzerChannel, 0);
        ledcWrite(kBuzzerChannel, 0);
        return;
    }

    double hz = ((double) freq) / 10.0;
    ledcWriteTone(kBuzzerChannel, hz);
}

static void esp32_play_frequency(bool en)
{
    if (en && s_freq_dhz != 0) {
        ledcWrite(kBuzzerChannel, 128);
    } else {
        ledcWrite(kBuzzerChannel, 0);
    }
}

static void esp32_configure_button(uint8_t pin)
{
    pinMode(pin, INPUT_PULLUP);
}

static void esp32_scan_button(button_t btn, uint8_t pin)
{
    btn_state_t state = digitalRead(pin) == LOW ? BTN_STATE_PRESSED : BTN_STATE_RELEASED;
    if (state != s_last_buttons[btn]) {
        s_last_buttons[btn] = state;
        tamalib_set_button(btn, state);
    }
}

static int esp32_handler(void)
{
    esp32_scan_button(BTN_LEFT, TAMALIB_BTN_LEFT_PIN);
    esp32_scan_button(BTN_MIDDLE, TAMALIB_BTN_MIDDLE_PIN);
    esp32_scan_button(BTN_RIGHT, TAMALIB_BTN_RIGHT_PIN);
    esp32_scan_button(BTN_TAP, TAMALIB_BTN_TAP_PIN);
    return 0;
}

static void esp32_log(log_level_t level, char *buff, ...)
{
    if (!Serial) {
        return;
    }

    va_list args;
    va_start(args, buff);
    Serial.printf("[HAL %u] ", (unsigned int) level);
    Serial.vprintf(buff, args);
    va_end(args);
}

static bool_t esp32_is_log_enabled(log_level_t)
{
    return 0;
}

static void *esp32_malloc(u32_t size)
{
    return malloc(size);
}

static void esp32_free(void *ptr)
{
    free(ptr);
}

static void esp32_halt(void)
{
    esp_deep_sleep_start();
}

static hal_t s_hal = {
    esp32_malloc,
    esp32_free,
    esp32_halt,
    esp32_is_log_enabled,
    esp32_log,
    esp32_sleep_until,
    esp32_get_timestamp,
    esp32_update_screen,
    esp32_set_pixel,
    esp32_set_icon,
    esp32_set_frequency,
    esp32_play_frequency,
    esp32_handler,
};

static void esp32_init_display()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TAMALIB_LCD_BG);
    tft.setSwapBytes(true);
}

hal_t *esp32_cyd_hal_init(void)
{
    Serial.begin(115200);
    esp32_init_display();

    esp32_configure_button(TAMALIB_BTN_LEFT_PIN);
    esp32_configure_button(TAMALIB_BTN_MIDDLE_PIN);
    esp32_configure_button(TAMALIB_BTN_RIGHT_PIN);
    esp32_configure_button(TAMALIB_BTN_TAP_PIN);

    ledcSetup(kBuzzerChannel, 2000, 8);
    ledcAttachPin(TAMALIB_BUZZER_PIN, kBuzzerChannel);
    ledcWrite(kBuzzerChannel, 0);

    memset(s_lcd_matrix, 0, sizeof(s_lcd_matrix));
    memset(s_icons, 0, sizeof(s_icons));
    s_last_frame = esp32_get_timestamp();
    s_dirty = true;

    return &s_hal;
}
