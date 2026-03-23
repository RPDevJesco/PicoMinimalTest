/*
 * hal_types.h — Minimal HAL shim for Pico SDK / RP2350
 *
 * Provides the macros and types that framebuffer.c expects from
 * the full Tutorial-OS HAL. On RP2350 we pull types from the
 * standard headers and define ARM32 barriers.
 */
#ifndef HAL_TYPES_H
#define HAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ARM32 Cortex-M33 barriers */
#define HAL_DMB()   __asm__ volatile("dmb sy" ::: "memory")
#define HAL_DSB()   __asm__ volatile("dsb sy" ::: "memory")
#define HAL_ISB()   __asm__ volatile("isb sy" ::: "memory")
#define HAL_NOP()   __asm__ volatile("nop")
#define HAL_WFI()   __asm__ volatile("wfi")
#define HAL_WFE()   __asm__ volatile("wfe")
#define HAL_SEV()   __asm__ volatile("sev")

/* Utility macros */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef BIT
#define BIT(n) (1UL << (n))
#endif
#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#endif

#define HAL_INLINE static inline __attribute__((always_inline))
#define HAL_WEAK   __attribute__((weak))

/* Volatile memory access */
#define READ_VOLATILE(ptr)       (*(volatile typeof(*(ptr)) *)(ptr))
#define WRITE_VOLATILE(ptr, val) (*(volatile typeof(*(ptr)) *)(ptr) = (val))

/* Pixel format enum — must match framebuffer.h */
typedef enum {
    FB_FORMAT_ARGB8888 = 0,
    FB_FORMAT_ABGR8888 = 1,
    FB_FORMAT_RGB565   = 2,
} fb_pixel_format_t;

#endif /* HAL_TYPES_H */
