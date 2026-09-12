#ifndef NANO_CAPTURE_H
#define NANO_CAPTURE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t stride; /* bytes per row */
    uint32_t size;   /* total buffer size in bytes */
    uint8_t *data;   /* 32-bit BGRA */
} nano_frame_t;

bool nano_frame_alloc(nano_frame_t *frame, uint16_t width, uint16_t height);
void nano_frame_free(nano_frame_t *frame);
bool nano_frame_copy(nano_frame_t *dst, const nano_frame_t *src);

/* Capture callback invoked when a new screen frame is ready */
typedef void (*nano_capture_callback_t)(const nano_frame_t *frame, void *user_data);

typedef struct nano_capture_s nano_capture_t;

/* Platform-implemented screen capture lifecycle */
nano_capture_t *nano_capture_create(void);
bool nano_capture_start(nano_capture_t *cap, nano_capture_callback_t cb, void *user_data);
void nano_capture_stop(nano_capture_t *cap);
void nano_capture_destroy(nano_capture_t *cap);
bool nano_capture_has_permission(void);

/* Synthetic test frame generator (used when testing or before Screen Recording permission) */
void nano_frame_generate_test_pattern(nano_frame_t *frame, uint32_t frame_index);

#endif /* NANO_CAPTURE_H */
