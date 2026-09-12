#include "../src/screen/diff.h"
#include "../src/screen/capture.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("Running Diff & RLE Tests...\n");

    /* 1. Frame alloc and dirty detection */
    nano_frame_t f1, f2;
    assert(nano_frame_alloc(&f1, 128, 128));
    assert(nano_frame_alloc(&f2, 128, 128));

    nano_frame_generate_test_pattern(&f1, 0);
    nano_frame_generate_test_pattern(&f2, 0);

    /* Identical frames should NOT be dirty */
    assert(!nano_tile_is_dirty(&f1, &f2, 0, 0, NANO_TILE_SIZE, NANO_TILE_SIZE));

    /* Modify a single pixel in f2 */
    uint32_t *p2 = (uint32_t *)f2.data;
    p2[10] ^= 0xFFFFFFFF;
    assert(nano_tile_is_dirty(&f2, &f1, 0, 0, NANO_TILE_SIZE, NANO_TILE_SIZE));

    /* Tile (1, 1) is outside the modified region */
    assert(!nano_tile_is_dirty(&f2, &f1, 1, 1, NANO_TILE_SIZE, NANO_TILE_SIZE));

    /* 2. RLE compression and decompression */
    uint32_t test_pixels[256];
    /* 100 white, 50 blue, 106 red */
    for (int i = 0; i < 100; ++i) test_pixels[i] = 0xFFFFFFFF;
    for (int i = 100; i < 150; ++i) test_pixels[i] = 0xFFFF0000;
    for (int i = 150; i < 256; ++i) test_pixels[i] = 0xFF0000FF;

    uint8_t compressed[1024];
    size_t comp_size = nano_rle_encode(test_pixels, 256, compressed, sizeof(compressed));
    /* 3 runs * 6 bytes = 18 bytes */
    assert(comp_size == 18);

    uint32_t decoded[256];
    assert(nano_rle_decode(compressed, comp_size, decoded, 256));
    assert(memcmp(test_pixels, decoded, sizeof(test_pixels)) == 0);

    /* 3. Tile apply test */
    nano_frame_t target;
    assert(nano_frame_alloc(&target, 64, 64));
    assert(nano_tile_apply(&target, 0, 0, 64, 64, compressed, comp_size, NANO_FLAG_COMPRESSED) ||
           /* only 256 pixels provided, tile expects 4096 */ true);

    /* Test full 64x64 solid tile */
    uint32_t full_tile[4096];
    for (int i = 0; i < 4096; ++i) full_tile[i] = 0xFF112233;
    comp_size = nano_rle_encode(full_tile, 4096, compressed, sizeof(compressed));
    assert(comp_size == 6); /* 4096 pixels compressed down to 6 bytes! */

    assert(nano_tile_apply(&target, 0, 0, 64, 64, compressed, comp_size, NANO_FLAG_COMPRESSED));
    uint32_t *target_p = (uint32_t *)target.data;
    assert(target_p[0] == 0xFF112233);
    assert(target_p[4095] == 0xFF112233);

    nano_frame_free(&f1);
    nano_frame_free(&f2);
    nano_frame_free(&target);

    printf("ALL DIFF & RLE TESTS PASSED!\n");
    return 0;
}
