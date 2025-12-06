#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <cstring>
#include <vector>
#include "esp32_hal.h"

extern "C" {
#include "tamalib.h"
}

/*
 * PlatformIO sketch wiring TamaLIB into the ESP32 Cheap Yellow Display (CYD).
 * The HAL implementation in esp32_hal.cpp drives the screen, buttons and
 * buzzer. This file is responsible for bootstrapping the hardware stack,
 * loading the program/ROM image, wiring button events and providing a hook for
 * state persistence.
 */

static constexpr timestamp_t kTimestampFrequency = 1000000; // us
static constexpr char kDefaultRomPath[] = "/roms/program.rom";
static constexpr char kStatePath[] = "/tamalib_state.bin";
static constexpr uint32_t kAutosavePeriodMs = 60000; // 1 minute

static std::vector<u12_t> s_program;
static breakpoint_t *s_breakpoints = NULL;

static hal_t *s_hal = NULL;
static bool s_tama_ready = false;
static bool s_storage_ready = false;
static uint32_t s_last_save_ms = 0;

struct PersistedState {
    u13_t pc;
    u12_t x;
    u12_t y;
    u4_t a;
    u4_t b;
    u5_t np;
    u8_t sp;
    u4_t flags;

    u32_t tick_counter;
    u32_t clk_timer_2hz_timestamp;
    u32_t clk_timer_4hz_timestamp;
    u32_t clk_timer_8hz_timestamp;
    u32_t clk_timer_16hz_timestamp;
    u32_t clk_timer_32hz_timestamp;
    u32_t clk_timer_64hz_timestamp;
    u32_t clk_timer_128hz_timestamp;
    u32_t clk_timer_256hz_timestamp;
    u32_t prog_timer_timestamp;
    bool_t prog_timer_enabled;
    u8_t prog_timer_data;
    u8_t prog_timer_rld;

    u32_t call_depth;
    bool_t cpu_halted;
    interrupt_t interrupts[INT_SLOT_NUM];
    MEM_BUFFER_TYPE memory[MEM_BUFFER_SIZE];
};

static bool mount_storage()
{
    s_storage_ready = SPIFFS.begin(true);
    if (!s_storage_ready) {
        Serial.println("[TamaLIB] Failed to mount SPIFFS; ROM/state loading will be skipped.");
    }
    return s_storage_ready;
}

static bool load_rom_from_storage(const char *path)
{
    if (!s_storage_ready) {
        return false;
    }

    File rom = SPIFFS.open(path, FILE_READ);
    if (!rom) {
        Serial.printf("[TamaLIB] ROM not found at %s\n", path);
        return false;
    }

    s_program.clear();
    while (rom.available()) {
        int low = rom.read();
        int high = rom.read();
        if (low < 0 || high < 0) {
            break;
        }

        u16_t word = static_cast<u16_t>((high << 8) | (low & 0xFF));
        s_program.push_back(static_cast<u12_t>(word & U12_MASK));
    }

    Serial.printf("[TamaLIB] Loaded %u 12-bit words from %s\n", (unsigned int) s_program.size(), path);
    return !s_program.empty();
}

static bool save_state_to_storage(const char *path)
{
    if (!s_storage_ready || !s_tama_ready) {
        return false;
    }

    state_t *runtime = tamalib_get_state();
    PersistedState snapshot = {
        .pc = *(runtime->pc),
        .x = *(runtime->x),
        .y = *(runtime->y),
        .a = *(runtime->a),
        .b = *(runtime->b),
        .np = *(runtime->np),
        .sp = *(runtime->sp),
        .flags = *(runtime->flags),

        .tick_counter = *(runtime->tick_counter),
        .clk_timer_2hz_timestamp = *(runtime->clk_timer_2hz_timestamp),
        .clk_timer_4hz_timestamp = *(runtime->clk_timer_4hz_timestamp),
        .clk_timer_8hz_timestamp = *(runtime->clk_timer_8hz_timestamp),
        .clk_timer_16hz_timestamp = *(runtime->clk_timer_16hz_timestamp),
        .clk_timer_32hz_timestamp = *(runtime->clk_timer_32hz_timestamp),
        .clk_timer_64hz_timestamp = *(runtime->clk_timer_64hz_timestamp),
        .clk_timer_128hz_timestamp = *(runtime->clk_timer_128hz_timestamp),
        .clk_timer_256hz_timestamp = *(runtime->clk_timer_256hz_timestamp),
        .prog_timer_timestamp = *(runtime->prog_timer_timestamp),
        .prog_timer_enabled = *(runtime->prog_timer_enabled),
        .prog_timer_data = *(runtime->prog_timer_data),
        .prog_timer_rld = *(runtime->prog_timer_rld),

        .call_depth = *(runtime->call_depth),
        .cpu_halted = *(runtime->cpu_halted),
    };

    memcpy(snapshot.interrupts, runtime->interrupts, sizeof(snapshot.interrupts));
    memcpy(snapshot.memory, runtime->memory, sizeof(snapshot.memory));

    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[TamaLIB] Cannot open %s for writing\n", path);
        return false;
    }

    size_t written = file.write(reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot));
    bool ok = written == sizeof(snapshot);
    Serial.printf("[TamaLIB] %s state to %s (%u bytes)\n", ok ? "Saved" : "Failed to save", path, (unsigned int) written);
    return ok;
}

static bool load_state_from_storage(const char *path)
{
    if (!s_storage_ready || !s_tama_ready) {
        return false;
    }

    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
        return false;
    }

    PersistedState snapshot = {};
    size_t read = file.readBytes(reinterpret_cast<char *>(&snapshot), sizeof(snapshot));
    if (read != sizeof(snapshot)) {
        Serial.printf("[TamaLIB] State file %s has unexpected size (%u bytes)\n", path, (unsigned int) read);
        return false;
    }

    state_t *runtime = tamalib_get_state();
    *(runtime->pc) = snapshot.pc;
    *(runtime->x) = snapshot.x;
    *(runtime->y) = snapshot.y;
    *(runtime->a) = snapshot.a;
    *(runtime->b) = snapshot.b;
    *(runtime->np) = snapshot.np;
    *(runtime->sp) = snapshot.sp;
    *(runtime->flags) = snapshot.flags;

    *(runtime->tick_counter) = snapshot.tick_counter;
    *(runtime->clk_timer_2hz_timestamp) = snapshot.clk_timer_2hz_timestamp;
    *(runtime->clk_timer_4hz_timestamp) = snapshot.clk_timer_4hz_timestamp;
    *(runtime->clk_timer_8hz_timestamp) = snapshot.clk_timer_8hz_timestamp;
    *(runtime->clk_timer_16hz_timestamp) = snapshot.clk_timer_16hz_timestamp;
    *(runtime->clk_timer_32hz_timestamp) = snapshot.clk_timer_32hz_timestamp;
    *(runtime->clk_timer_64hz_timestamp) = snapshot.clk_timer_64hz_timestamp;
    *(runtime->clk_timer_128hz_timestamp) = snapshot.clk_timer_128hz_timestamp;
    *(runtime->clk_timer_256hz_timestamp) = snapshot.clk_timer_256hz_timestamp;
    *(runtime->prog_timer_timestamp) = snapshot.prog_timer_timestamp;
    *(runtime->prog_timer_enabled) = snapshot.prog_timer_enabled;
    *(runtime->prog_timer_data) = snapshot.prog_timer_data;
    *(runtime->prog_timer_rld) = snapshot.prog_timer_rld;

    *(runtime->call_depth) = snapshot.call_depth;
    *(runtime->cpu_halted) = snapshot.cpu_halted;

    memcpy(runtime->interrupts, snapshot.interrupts, sizeof(snapshot.interrupts));
    memcpy(runtime->memory, snapshot.memory, sizeof(snapshot.memory));

    tamalib_refresh_hw();
    Serial.printf("[TamaLIB] Restored state from %s\n", path);
    return true;
}

static void autosave_state()
{
    uint32_t now = millis();
    if (now - s_last_save_ms >= kAutosavePeriodMs) {
        if (save_state_to_storage(kStatePath)) {
            s_last_save_ms = now;
        }
    }
}

void setup()
{
    Serial.begin(115200);

    mount_storage();
    s_hal = esp32_cyd_hal_init();
    tamalib_register_hal(s_hal);

    if (load_rom_from_storage(kDefaultRomPath)) {
        s_tama_ready = !tamalib_init(s_program.data(), s_breakpoints, kTimestampFrequency);
        if (s_tama_ready) {
            load_state_from_storage(kStatePath);
            s_last_save_ms = millis();
        }
    } else {
        Serial.println("[TamaLIB] No ROM loaded; emulator will remain idle.");
    }
}

void loop()
{
    if (!s_tama_ready) {
        delay(1000);
        return;
    }

    // tamalib_mainloop will poll the HAL handler, which forwards button events
    // via tamalib_set_button().
    tamalib_mainloop();
    autosave_state();
}
