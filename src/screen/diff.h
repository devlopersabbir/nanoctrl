#ifndef NANO_DIFF_H
#define NANO_DIFF_H

#include "capture.h"
#include "protocol.h"

/* Fast dirty check for a tile region */
bool nano_tile_is_dirty(const nano_frame_t *curr,
                           const nano_frame_t *prev,
                           uint16_t tile_x,
                           uint16_t tile_y,
                           uint16_t tile_w,
                           uint16_t tile_h);

/* Extract tile raw pixel data */
size_t nano_tile_extract_raw(const nano_frame_t *frame,
                             uint16_t tile_x,
                             uint16_t tile_y,
                             uint16_t tile_w,
                             uint16_t tile_h,
                             uint8_t *out_buf,
                             size_t out_max);

/* Run-length encoding of 32-bit pixels */
size_t nano_rle_encode(const uint32_t *pixels,
                       size_t count,
                       uint8_t *out_buf,
                       size_t out_max);

/* Run-length decoding into 32-bit pixels */
bool nano_rle_decode(const uint8_t *in_buf,
                     size_t in_len,
                     uint32_t *out_pixels,
                     size_t expected_count);

/* Blit received tile onto target framebuffer */
bool nano_tile_apply(nano_frame_t *frame,
                     uint16_t tile_x,
                     uint16_t tile_y,
                     uint16_t tile_w,
                     uint16_t tile_h,
                     const uint8_t *data,
                     size_t data_len,
                     nano_msg_flags_t flags);

#endif /* NANO_DIFF_H */
