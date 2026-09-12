#include "diff.h"
#include <stdlib.h>
#include <string.h>

bool nano_tile_is_dirty(const nano_frame_t *curr,
                        const nano_frame_t *prev,
                        uint16_t tile_x,
                        uint16_t tile_y,
                        uint16_t tile_w,
                        uint16_t tile_h) {
    if (!curr || !curr->data) return false;
    if (!prev || !prev->data) return true; /* First frame is always dirty */
    if (curr->width != prev->width || curr->height != prev->height) return true;

    uint32_t start_x = (uint32_t)tile_x * NANO_TILE_SIZE;
    uint32_t start_y = (uint32_t)tile_y * NANO_TILE_SIZE;
    size_t row_bytes = (size_t)tile_w * 4;

    for (uint16_t row = 0; row < tile_h; ++row) {
        uint32_t y = start_y + row;
        if (y >= curr->height) break;

        const uint8_t *curr_ptr = curr->data + (y * curr->stride) + (start_x * 4);
        const uint8_t *prev_ptr = prev->data + (y * prev->stride) + (start_x * 4);

        if (memcmp(curr_ptr, prev_ptr, row_bytes) != 0) {
            return true;
        }
    }

    return false;
}

size_t nano_tile_extract_raw(const nano_frame_t *frame,
                             uint16_t tile_x,
                             uint16_t tile_y,
                             uint16_t tile_w,
                             uint16_t tile_h,
                             uint8_t *out_buf,
                             size_t out_max) {
    if (!frame || !frame->data || !out_buf) return 0;

    size_t needed = (size_t)tile_w * tile_h * 4;
    if (out_max < needed) return 0;

    uint32_t start_x = (uint32_t)tile_x * NANO_TILE_SIZE;
    uint32_t start_y = (uint32_t)tile_y * NANO_TILE_SIZE;
    size_t row_bytes = (size_t)tile_w * 4;

    uint8_t *dst = out_buf;
    for (uint16_t row = 0; row < tile_h; ++row) {
        uint32_t y = start_y + row;
        const uint8_t *src = frame->data + (y * frame->stride) + (start_x * 4);
        memcpy(dst, src, row_bytes);
        dst += row_bytes;
    }

    return needed;
}

size_t nano_rle_encode(const uint32_t *pixels,
                       size_t count,
                       uint8_t *out_buf,
                       size_t out_max) {
    if (!pixels || count == 0 || !out_buf) return 0;

    size_t out_idx = 0;
    size_t i = 0;

    while (i < count) {
        uint32_t val = pixels[i];
        size_t run_len = 1;

        while ((i + run_len < count) && (pixels[i + run_len] == val) && (run_len < 65535)) {
            run_len++;
        }

        /* Check buffer bounds: each entry takes 2 bytes (len) + 4 bytes (pixel) = 6 bytes */
        if (out_idx + 6 > out_max) {
            return 0; /* Overflow */
        }

        uint16_t rlen16 = (uint16_t)run_len;
        memcpy(out_buf + out_idx, &rlen16, sizeof(rlen16));
        out_idx += sizeof(rlen16);

        memcpy(out_buf + out_idx, &val, sizeof(val));
        out_idx += sizeof(val);

        i += run_len;
    }

    return out_idx;
}

bool nano_rle_decode(const uint8_t *in_buf,
                     size_t in_len,
                     uint32_t *out_pixels,
                     size_t expected_count) {
    if (!in_buf || !out_pixels) return false;

    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx + 6 <= in_len && out_idx < expected_count) {
        uint16_t run_len;
        memcpy(&run_len, in_buf + in_idx, sizeof(run_len));
        in_idx += sizeof(run_len);

        uint32_t val;
        memcpy(&val, in_buf + in_idx, sizeof(val));
        in_idx += sizeof(val);

        if (out_idx + run_len > expected_count) {
            return false;
        }

        for (uint16_t r = 0; r < run_len; ++r) {
            out_pixels[out_idx++] = val;
        }
    }

    return (out_idx == expected_count);
}

bool nano_tile_apply(nano_frame_t *frame,
                     uint16_t tile_x,
                     uint16_t tile_y,
                     uint16_t tile_w,
                     uint16_t tile_h,
                     const uint8_t *data,
                     size_t data_len,
                     nano_msg_flags_t flags) {
    if (!frame || !frame->data || !data) return false;

    size_t total_pixels = (size_t)tile_w * tile_h;
    uint32_t scratch[NANO_TILE_SIZE * NANO_TILE_SIZE];
    const uint32_t *tile_pixels = NULL;

    if (flags & NANO_FLAG_COMPRESSED) {
        if (total_pixels > (NANO_TILE_SIZE * NANO_TILE_SIZE)) return false;
        if (!nano_rle_decode(data, data_len, scratch, total_pixels)) {
            return false;
        }
        tile_pixels = scratch;
    } else {
        if (data_len < total_pixels * 4) return false;
        tile_pixels = (const uint32_t *)data;
    }

    uint32_t start_x = (uint32_t)tile_x * NANO_TILE_SIZE;
    uint32_t start_y = (uint32_t)tile_y * NANO_TILE_SIZE;
    size_t row_bytes = (size_t)tile_w * 4;

    for (uint16_t row = 0; row < tile_h; ++row) {
        uint32_t y = start_y + row;
        if (y >= frame->height) break;

        uint8_t *dst = frame->data + (y * frame->stride) + (start_x * 4);
        const uint8_t *src = (const uint8_t *)(tile_pixels + (row * tile_w));
        memcpy(dst, src, row_bytes);
    }

    return true;
}
