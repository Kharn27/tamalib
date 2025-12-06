/*
 * TamaLIB - ESP32 CYD type definitions
 */
#ifndef _HAL_TYPES_H_
#define _HAL_TYPES_H_

#include <stdint.h>

/* Boolean type expected by TamaLIB */
typedef uint8_t bool_t;

typedef uint8_t u4_t;   /* At least 4 bits */
typedef uint8_t u5_t;   /* At least 5 bits */
typedef uint8_t u8_t;
typedef uint16_t u12_t; /* At least 12 bits */
typedef uint16_t u13_t; /* At least 13 bits */
typedef uint32_t u32_t;
typedef uint32_t timestamp_t; /* Unsigned type to preserve wrap-around semantics */

#endif /* _HAL_TYPES_H_ */
