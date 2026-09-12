CC ?= clang

VERSION ?= $(shell cat VERSION 2>/dev/null | tr -d '\n\r')
ifeq ($(VERSION),)
  VERSION := 0.1.0
endif

GIT_COMMIT ?= $(shell git rev-parse --short HEAD 2>/dev/null)
ifeq ($(GIT_COMMIT),)
  GIT_COMMIT := dev
endif

CFLAGS ?= -Oz -flto -Wall -Wextra -Wno-unused-command-line-argument
CFLAGS += -DNANO_VERSION_STR=\"$(VERSION)\" -DNANO_GIT_COMMIT=\"$(GIT_COMMIT)\"

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
TARGET_UNIVERSAL = build/nanoctrl-universal
TARGET_ARM64 = build/nanoctrl-arm64
TARGET_X86_64 = build/nanoctrl-x86_64

.PHONY: all clean test size version check-version universal arm64 x86_64

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

universal: check-version
	@mkdir -p build
	$(CC) $(CFLAGS) -arch arm64 -arch x86_64 $(INCLUDES) $(SRCS) $(LDFLAGS) -o $(TARGET_UNIVERSAL)
	@strip $(TARGET_UNIVERSAL)
	@echo "Built universal binary: $(TARGET_UNIVERSAL)"
	@ls -lh $(TARGET_UNIVERSAL) | awk '{printf "Universal size: %s\n", $$5}'

arm64: check-version
	@mkdir -p build
	$(CC) $(CFLAGS) -arch arm64 $(INCLUDES) $(SRCS) $(LDFLAGS) -o $(TARGET_ARM64)
	@strip $(TARGET_ARM64)
	@echo "Built arm64 binary: $(TARGET_ARM64)"
	@ls -lh $(TARGET_ARM64) | awk '{printf "arm64 size: %s\n", $$5}'

x86_64: check-version
	@mkdir -p build
	$(CC) $(CFLAGS) -arch x86_64 $(INCLUDES) $(SRCS) $(LDFLAGS) -o $(TARGET_X86_64)
	@strip $(TARGET_X86_64)
	@echo "Built x86_64 binary: $(TARGET_X86_64)"
	@ls -lh $(TARGET_X86_64) | awk '{printf "x86_64 size: %s\n", $$5}'

version:
	@echo "$(VERSION)"

check-version:
	@echo "Validating version '$(VERSION)'..."
	@echo "$(VERSION)" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$$' > /dev/null || \
		(echo "Error: '$(VERSION)' is not a valid Semantic Version (MAJOR.MINOR.PATCH)" && exit 1)

size: $(TARGET)
	@echo "========================================"
	@echo "     NANOCTRL SIZE REPORT (v$(VERSION))  "
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
