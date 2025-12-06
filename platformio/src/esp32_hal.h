#ifndef _ESP32_HAL_H_
#define _ESP32_HAL_H_

#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the ESP32 Cheap Yellow Display (CYD) peripherals and
 * return a HAL instance configured for TamaLIB.
 */
hal_t *esp32_cyd_hal_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_H_ */
