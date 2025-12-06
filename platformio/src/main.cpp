#include <Arduino.h>
#include <TFT_eSPI.h>
#include "esp32_hal.h"

extern "C" {
#include "tamalib.h"
}

/*
 * Minimal PlatformIO sketch to wire TamaLIB into the ESP32 Cheap Yellow
 * Display (CYD). The HAL implementation in esp32_hal.cpp drives the
 * screen, buttons and buzzer. Provide a valid ROM/program before
 * calling tamalib_init().
 */

// Replace these with your program/ROM and breakpoint table.
static const u12_t *s_program = NULL;
static breakpoint_t *s_breakpoints = NULL;

static hal_t *s_hal = NULL;
static bool s_tama_ready = false;

void setup() {
    s_hal = esp32_cyd_hal_init();
    tamalib_register_hal(s_hal);

    if (s_program != NULL) {
        // Timestamps are expressed in microseconds, so pass 1 MHz.
        s_tama_ready = !tamalib_init(s_program, s_breakpoints, 1000000);
    }
}

void loop() {
    if (s_tama_ready) {
        tamalib_mainloop();
    } else {
        delay(1000);
    }
}
