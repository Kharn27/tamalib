#include <Arduino.h>
extern "C" {
#include "tamalib.h"
}

/*
 * Minimal PlatformIO sketch to demonstrate how to wire TamaLIB into the
 * ESP32 Cheap Yellow Display (CYD). Implement the HAL callbacks to map
 * inputs/outputs to your hardware. See lib/tamalib/hal.h for details.
 */

static timestamp_t placeholder_timestamp() {
    return 0;
}

static void placeholder_sleep(timestamp_t) {}
static void placeholder_update_screen() {}
static void placeholder_set_lcd_matrix(u8_t, u8_t, bool_t) {}
static void placeholder_set_lcd_icon(u8_t, bool_t) {}
static void placeholder_set_frequency(u32_t) {}
static void placeholder_play_frequency(bool_t) {}
static int placeholder_handler() { return 1; }
static void placeholder_halt() { while (true) { delay(1000); } }
static void *placeholder_malloc(u32_t size) { return malloc(size); }
static void placeholder_free(void *ptr) { free(ptr); }
static bool_t placeholder_is_log_enabled(log_level_t) { return 0; }
static void placeholder_log(log_level_t, char *, ...) {}

static hal_t s_hal = {
    placeholder_malloc,
    placeholder_free,
    placeholder_halt,
    placeholder_is_log_enabled,
    placeholder_log,
    placeholder_sleep,
    placeholder_timestamp,
    placeholder_update_screen,
    placeholder_set_lcd_matrix,
    placeholder_set_lcd_icon,
    placeholder_set_frequency,
    placeholder_play_frequency,
    placeholder_handler,
};

void setup() {
    tamalib_register_hal(&s_hal);
    // Provide a real ROM and HAL implementation before calling init.
}

void loop() {
    delay(1000);
}
