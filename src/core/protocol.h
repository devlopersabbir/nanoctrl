#ifndef NANO_PROTOCOL_H
#define NANO_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NANO_MAGIC 0x4E  /* 'N' */
#define NANO_VERSION 1
#define NANO_HEADER_SIZE 8
#define NANO_MAX_PAYLOAD (16 * 1024 * 1024) /* 16 MB max message size */
#define NANO_DEFAULT_PORT 7443
#define NANO_TILE_SIZE 64

typedef enum {
    NANO_MSG_HELLO          = 0x01,
    NANO_MSG_AUTH_CHALLENGE = 0x02,
    NANO_MSG_AUTH_RESPONSE  = 0x03,
    NANO_MSG_AUTH_RESULT    = 0x04,
    NANO_MSG_SCREEN_HEADER  = 0x10,
    NANO_MSG_SCREEN_TILE    = 0x11,
    NANO_MSG_MOUSE_MOVE     = 0x20,
    NANO_MSG_MOUSE_BUTTON   = 0x21,
    NANO_MSG_MOUSE_SCROLL   = 0x22,
    NANO_MSG_KEY            = 0x30,
    NANO_MSG_PING           = 0xF0,
    NANO_MSG_PONG           = 0xF1,
    NANO_MSG_DISCONNECT     = 0xFF
} nano_msg_type_t;

typedef enum {
    NANO_FLAG_NONE       = 0x00,
    NANO_FLAG_COMPRESSED = 0x01,
    NANO_FLAG_RAW        = 0x02
} nano_msg_flags_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic;       /* 'N' (0x4E) */
    uint8_t  version;     /* 1 */
    uint8_t  type;        /* nano_msg_type_t */
    uint8_t  flags;       /* nano_msg_flags_t */
    uint32_t length;      /* big-endian payload length */
} nano_header_t;

typedef struct {
    uint8_t  role;        /* 0 = host, 1 = controller */
    uint16_t width;       /* big-endian */
    uint16_t height;      /* big-endian */
    uint16_t reserved;
} nano_msg_hello_t;

typedef struct {
    uint8_t nonce[32];
} nano_msg_auth_challenge_t;

typedef struct {
    uint8_t hash[32];
} nano_msg_auth_response_t;

typedef struct {
    uint8_t status;       /* 0: OK, 1: REJECTED, 2: BAD_PIN, 3: ERROR */
} nano_msg_auth_result_t;

typedef struct {
    uint16_t width;       /* Display width */
    uint16_t height;      /* Display height */
    uint16_t tile_w;      /* Tile width (e.g. 64) */
    uint16_t tile_h;      /* Tile height (e.g. 64) */
    uint8_t  format;      /* 0: BGRA32 */
    uint8_t  reserved;
} nano_msg_screen_header_t;

typedef struct {
    uint16_t tile_x;      /* Tile column index */
    uint16_t tile_y;      /* Tile row index */
    uint16_t width;       /* Actual width of this tile */
    uint16_t height;      /* Actual height of this tile */
    uint32_t uncompressed_len; /* Length before compression */
} nano_msg_tile_header_t;

typedef struct {
    uint16_t norm_x;      /* 0..65535 */
    uint16_t norm_y;      /* 0..65535 */
} nano_msg_mouse_move_t;

typedef struct {
    uint8_t  button;      /* 0: Left, 1: Right, 2: Other */
    uint8_t  action;      /* 0: Up, 1: Down */
    uint16_t norm_x;      /* 0..65535 */
    uint16_t norm_y;      /* 0..65535 */
} nano_msg_mouse_button_t;

typedef struct {
    int16_t dx;
    int16_t dy;
} nano_msg_mouse_scroll_t;

typedef struct {
    uint16_t keycode;     /* macOS virtual keycode */
    uint8_t  action;      /* 0: Up, 1: Down */
    uint8_t  modifiers;   /* Bitmask: 1=Shift, 2=Ctrl, 4=Alt, 8=Cmd */
} nano_msg_key_t;

typedef struct {
    uint32_t seq;
} nano_msg_ping_t;

typedef struct {
    uint8_t reason;       /* 0: normal, 1: timeout, 2: rejected, 3: error */
} nano_msg_disconnect_t;

#pragma pack(pop)

/* Protocol helper functions */
void nano_header_init(nano_header_t *hdr, nano_msg_type_t type, nano_msg_flags_t flags, uint32_t length);
bool nano_header_encode(const nano_header_t *hdr, uint8_t *buf, size_t buf_len);
bool nano_header_decode(const uint8_t *buf, size_t buf_len, nano_header_t *hdr);

#endif /* NANO_PROTOCOL_H */
