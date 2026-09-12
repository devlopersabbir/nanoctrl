CC ?= clang
CFLAGS ?= -Oz -flto -Wall -Wextra -Wno-unused-command-line-argument
INCLUDES = -Isrc/core -Isrc/net -Isrc/screen -Isrc/input -Isrc/crypto -Isrc/platform/macos
FRAMEWORKS = -framework Cocoa -framework ScreenCaptureKit -framework CoreGraphics -framework CoreMedia -framework CoreVideo -framework Foundation -framework ApplicationServices
LDFLAGS = -flto -Wl,-dead_strip $(FRAMEWORKS)

SRC_CORE = src/core/protocol.c src/core/session.c
SRC_NET  = src/net/socket.c src/net/transport.c
SRC_SCR  = src/screen/capture.c src/screen/diff.c
SRC_CRY  = src/crypto/random.c src/crypto/auth.c
SRC_PLAT = src/platform/macos/screen_mac.m src/platform/macos/input_mac.m src/platform/macos/ui_mac.m
SRC_MAIN = src/main.c

SRCS = $(SRC_CORE) $(SRC_NET) $(SRC_SCR) $(SRC_CRY) $(SRC_PLAT) $(SRC_MAIN)
OBJS = $(patsubst src/%.c, build/obj/%.o, $(filter %.c, $(SRCS))) $(patsubst src/%.m, build/obj/%.o, $(filter %.m, $(SRCS)))

TARGET = build/nanoctrl

.PHONY: all clean test size

all: $(TARGET) size

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	@strip $@

build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

build/obj/%.o: src/%.m
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

size: $(TARGET)
	@echo "========================================"
	@echo "          NANOCTRL SIZE REPORT          "
	@echo "========================================"
	@ls -lh $(TARGET) | awk '{printf "Binary size: %s (Target: < 2.0 MB)\n", $$5}'
	@size $(TARGET) || true
	@echo "========================================"

# Test targets
COMMON_TEST_SRCS = $(SRC_CORE) $(SRC_NET) $(SRC_SCR) $(SRC_CRY) src/platform/macos/screen_mac.m src/platform/macos/input_mac.m

test: test_protocol test_diff test_loopback
	@echo "\n=== Running All Unit and Integration Tests ==="
	@build/test_protocol
	@build/test_diff
	@build/test_loopback
	@echo "\n>>> ALL TESTS PASSED SUCCESSFULLY! <<<\n"

test_protocol: tests/test_protocol.c src/core/protocol.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o build/$@

test_diff: tests/test_diff.c src/screen/diff.c src/screen/capture.c src/core/protocol.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o build/$@

test_loopback: tests/test_loopback.c $(COMMON_TEST_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o build/$@

clean:
	rm -rf build
