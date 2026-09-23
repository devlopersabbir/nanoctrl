CC ?= clang

VERSION ?= $(shell cat VERSION 2>/dev/null | tr -d '\n\r')
ifeq ($(VERSION),)
  VERSION := 0.1.0
endif

GIT_COMMIT ?= $(shell git rev-parse --short HEAD 2>/dev/null)
ifeq ($(GIT_COMMIT),)
  GIT_COMMIT := dev
endif

CFLAGS ?= -Oz -flto -Wall -Wextra -Wno-unused-command-line-argument -fobjc-arc
CFLAGS += -DNANO_VERSION_STR=\"$(VERSION)\" -DNANO_GIT_COMMIT=\"$(GIT_COMMIT)\"

INCLUDES = -Isrc/core -Isrc/net -Isrc/screen -Isrc/input -Isrc/crypto -Isrc/platform/macos
FRAMEWORKS = -framework Cocoa -framework ScreenCaptureKit -framework CoreGraphics -framework CoreMedia -framework CoreVideo -framework Foundation -framework ApplicationServices
LDFLAGS = -flto -Wl,-dead_strip $(FRAMEWORKS)

SRC_CORE = src/core/protocol.c src/core/session.c
SRC_NET  = src/net/socket.c src/net/transport.c src/net/server.c
SRC_SCR  = src/screen/capture.c src/screen/diff.c
SRC_CRY  = src/crypto/random.c src/crypto/auth.c
SRC_PLAT = src/platform/macos/screen_mac.m src/platform/macos/input_mac.m src/platform/macos/ui_mac.m
SRC_MAIN = src/main.c

SRCS = $(SRC_CORE) $(SRC_NET) $(SRC_SCR) $(SRC_CRY) $(SRC_PLAT) $(SRC_MAIN)
OBJS = $(patsubst src/%.c, build/obj/%.o, $(filter %.c, $(SRCS))) $(patsubst src/%.m, build/obj/%.o, $(filter %.m, $(SRCS)))

TARGET = build/nanoctrl
SERVER_TARGET = build/nanosrv
TARGET_UNIVERSAL = build/nanoctrl-universal
TARGET_ARM64 = build/nanoctrl-arm64
TARGET_X86_64 = build/nanoctrl-x86_64

APP_NAME = NANOCTRL
APP_BUNDLE = build/$(APP_NAME).app
APP_CONTENTS = $(APP_BUNDLE)/Contents
APP_MACOS = $(APP_CONTENTS)/MacOS
APP_RESOURCES = $(APP_CONTENTS)/Resources

.PHONY: all clean test size version check-version universal arm64 x86_64 bundle dmg server

all: $(TARGET) $(SERVER_TARGET) size

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	@strip $@

$(SERVER_TARGET): src/server/nanosrv_main.c src/net/server.c src/net/socket.c src/net/transport.c src/core/protocol.c src/crypto/random.c src/crypto/auth.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -lpthread -o $@
	@strip $@

server: $(SERVER_TARGET)

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

# Application Bundle (.app)
bundle: universal resources/AppIcon.icns
	@echo "Creating macOS application bundle: $(APP_BUNDLE)..."
	@rm -rf $(APP_BUNDLE)
	@mkdir -p $(APP_MACOS) $(APP_RESOURCES)
	@cp $(TARGET_UNIVERSAL) $(APP_MACOS)/nanoctrl
	@chmod +x $(APP_MACOS)/nanoctrl
	@echo "APPL????" > $(APP_CONTENTS)/PkgInfo
	@sed "s/%%VERSION%%/$(VERSION)/g" resources/Info.plist.template > $(APP_CONTENTS)/Info.plist
	@plutil -lint $(APP_CONTENTS)/Info.plist
	@cp resources/AppIcon.icns $(APP_RESOURCES)/AppIcon.icns
	@echo "Signing application bundle with Hardened Runtime..."
	@codesign --force --deep --options runtime --entitlements resources/entitlements.plist --sign - $(APP_BUNDLE)
	@codesign --verify --deep --strict $(APP_BUNDLE)
	@echo "[OK] Successfully built and signed $(APP_BUNDLE)"

# Disk Image (.dmg)
dmg: bundle
	@echo "Packaging disk image: build/$(APP_NAME)-v$(VERSION).dmg..."
	@rm -rf build/dmg_staging build/$(APP_NAME)-v$(VERSION).dmg
	@mkdir -p build/dmg_staging
	@cp -R $(APP_BUNDLE) build/dmg_staging/
	@ln -s /Applications build/dmg_staging/Applications
	@hdiutil create -volname "$(APP_NAME)" -srcfolder build/dmg_staging -ov -format UDZO build/$(APP_NAME)-v$(VERSION).dmg
	@rm -rf build/dmg_staging
	@echo "[OK] Created disk image: build/$(APP_NAME)-v$(VERSION).dmg"
	@ls -lh build/$(APP_NAME)-v$(VERSION).dmg

resources/AppIcon.icns:
	@mkdir -p resources build
	$(CC) -framework Cocoa tools/generate_icon.m -o build/gen_icon
	@build/gen_icon
	@rm -f build/gen_icon

version:
	@echo "$(VERSION)"

check-version:
	@echo "Validating version '$(VERSION)'..."
	@echo "$(VERSION)" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$$' > /dev/null || \
		(echo "Error: '$(VERSION)' is not a valid Semantic Version (MAJOR.MINOR.PATCH)" && exit 1)

size: $(TARGET) $(SERVER_TARGET)
	@echo "========================================"
	@echo "     NANOCTRL SIZE REPORT (v$(VERSION))  "
	@echo "========================================"
	@ls -lh $(TARGET) | awk '{printf "App binary size:    %s (Target: < 2.0 MB)\n", $$5}'
	@ls -lh $(SERVER_TARGET) | awk '{printf "Server binary size: %s (Target: < 100 KB)\n", $$5}'
	@echo "========================================"

# Test targets
COMMON_TEST_SRCS = $(SRC_CORE) $(SRC_NET) $(SRC_SCR) $(SRC_CRY) src/platform/macos/screen_mac.m src/platform/macos/input_mac.m

test: test_protocol test_diff test_loopback test_relay
	@echo "\n=== Running All Unit and Integration Tests ==="
	@build/test_protocol
	@build/test_diff
	@build/test_loopback
	@build/test_relay
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

test_relay: tests/test_relay.c $(COMMON_TEST_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o build/$@

clean:
	rm -rf build
