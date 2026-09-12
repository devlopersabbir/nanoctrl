#include "capture.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool nano_frame_alloc(nano_frame_t *frame, uint16_t width, uint16_t height) {
    if (!frame || width == 0 || height == 0) return false;
    frame->width = width;
    frame->height = height;
    frame->stride = (uint32_t)width * 4;
    frame->size = frame->stride * (uint32_t)height;
    frame->data = (uint8_t *)calloc(1, frame->size);
    return frame->data != NULL;
}

void nano_frame_free(nano_frame_t *frame) {
    if (!frame) return;
    if (frame->data) {
        free(frame->data);
        frame->data = NULL;
    }
    frame->width = 0;
    frame->height = 0;
    frame->stride = 0;
    frame->size = 0;
}

bool nano_frame_copy(nano_frame_t *dst, const nano_frame_t *src) {
    if (!dst || !src || !src->data) return false;
    if (dst->width != src->width || dst->height != src->height || !dst->data) {
        nano_frame_free(dst);
        if (!nano_frame_alloc(dst, src->width, src->height)) {
            return false;
        }
    }
    memcpy(dst->data, src->data, src->size);
    return true;
}

void nano_frame_generate_test_pattern(nano_frame_t *frame, uint32_t frame_index) {
    if (!frame || !frame->data) return;

    uint32_t *pixels = (uint32_t *)frame->data;
    uint16_t w = frame->width;
    uint16_t h = frame->height;

    /* Animated background gradient + moving box */
    uint32_t box_size = 80;
    uint32_t box_x = (frame_index * 4) % (w > box_size ? (w - box_size) : 1);
    uint32_t box_y = (frame_index * 2) % (h > box_size ? (h - box_size) : 1);

    for (uint16_t y = 0; y < h; ++y) {
        for (uint16_t x = 0; x < w; ++x) {
            uint32_t idx = y * w + x;
            if (x >= box_x && x < box_x + box_size && y >= box_y && y < box_y + box_size) {
                /* Vibrant moving white/yellow box */
                pixels[idx] = 0xFFFFFF00; /* BGRA or ARGB depending on endian, bright yellow */
            } else {
                /* Elegant dark slate gradient pattern */
                uint8_t r = (uint8_t)((x * 40) / w + 20);
                uint8_t g = (uint8_t)((y * 40) / h + 25);
                uint8_t b = (uint8_t)(((x + y) * 40) / (w + h) + 35);
                uint8_t a = 0xFF;
                pixels[idx] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }
}
