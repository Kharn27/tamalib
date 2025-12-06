/*
 * TamaLIB - ESP32 CYD type definitions
 */
#ifndef _HAL_TYPES_H_
#define _HAL_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

/* Boolean type expected by TamaLIB */
typedef bool bool_t;

typedef uint8_t u4_t;   /* 4-bit storage */
typedef uint8_t u5_t;   /* 5-bit storage */
typedef uint8_t u8_t;
typedef uint16_t u12_t; /* 12-bit storage */
typedef uint16_t u13_t; /* 13-bit storage */
typedef uint32_t u32_t;
typedef uint32_t timestamp_t; /* Unsigned type to preserve wrap-around semantics */

/* Bit masks to constrain TamaLIB-specific non-standard widths */
#define U4_MASK   ((u4_t)0x0F)
#define U5_MASK   ((u5_t)0x1F)
#define U12_MASK  ((u12_t)0x0FFF)
#define U13_MASK  ((u13_t)0x1FFF)

#endif /* _HAL_TYPES_H_ */
