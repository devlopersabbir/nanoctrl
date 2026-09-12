# NANOCTRL (macOS Native)

> **Tiny Remote Control, Nothing Else.**

NANOCTRL is an ultra-lightweight, zero-dependency, native remote-control application written in C and Cocoa for macOS.

* **Binary Size**: ~89 KB (budget < 2.0 MB)
* **Zero 3rd-party dependencies**: Uses only native macOS system frameworks (`Cocoa`, `ScreenCaptureKit`, `CoreGraphics`, `CoreMedia`, `CoreVideo`).
* **Fast binary protocol**: 8-byte message header, tile-based dirty rect detection (64x64 tiles), RLE run-length encoding.
* **Security first**: Temporary 6-digit PIN with HMAC-SHA256 challenge-response authentication and mandatory host approval prompt.

---

## Architecture

```text
                           NANOCTRL Core (Pure C)
  ┌─────────────────┬─────────────────┬─────────────────┬─────────────────┐
  │  Protocol &     │   Network &     │   Tile Diff &   │  Crypto & Auth  │
  │  Framing        │   Transport     │   Compression   │  (PIN & Token)  │
  │  (protocol.c)   │   (socket.c)    │   (diff.c)      │  (auth.c)       │
  └────────┬────────┴────────┬────────┴────────┬────────┴────────┬────────┘
           │                 │                 │                 │
           ▼                 ▼                 ▼                 ▼
  ┌───────────────────────────────────────────────────────────────────────┐
  │                 Platform Abstraction Layer (macOS)                    │
  ├───────────────────┬───────────────────┬───────────────────────────────┤
  │ ScreenCaptureKit  │ CoreGraphics      │ Native Cocoa AppKit Window    │
  │ (screen_mac.m)    │ Input Injection   │ & Remote Viewport             │
  │                   │ (input_mac.m)     │ (ui_mac.m)                    │
  └───────────────────┴───────────────────┴───────────────────────────────┘
```

---

## Building

Requires Xcode Command Line Tools (`clang` and macOS SDK).

```bash
# Build native CLI binary
make

# Build universal macOS application bundle (NANOCTRL.app)
make bundle

# Package distributable disk image (NANOCTRL.dmg)
make dmg
```

To run the automated test suite (protocol framing, tile diffing, RLE compression, and network loopback integration):

```bash
make test
```

To view the binary size report:

```bash
make size
```

---

## Usage

### 1. Native macOS Graphical Interface (Default)

Launch without arguments to open the native GUI:

```bash
./build/nanoctrl
```

From the launcher window:
- **Share This Mac (Host)**: Generates a temporary 6-digit PIN, waits for a connection, prompts to ACCEPT or REJECT incoming requests, and allows stopping at any time.
- **Control Remote Computer (Controller)**: Enter the host address (`IP:port`) and 6-digit PIN, click **[ CONNECT ]**, and control the remote desktop directly in a native high-performance viewport window.

### 2. Command Line Interface (CLI)

Host on custom port with auto-generated PIN:
```bash
./build/nanoctrl --host 7443 --cli
```

Host with a fixed custom PIN:
```bash
./build/nanoctrl --host 7443 --pin 847291 --cli
```

Controller connecting to host:
```bash
./build/nanoctrl --controller 127.0.0.1:7443 847291 --cli
```

Self-diagnostics:
```bash
./build/nanoctrl --test
```

## macOS First Launch & Gatekeeper

When you download NANOCTRL from the web or GitHub, macOS Gatekeeper may show:
> *"Apple could not verify NANOCTRL is free of malware..."*

This is standard macOS behavior for open-source software built outside the paid Apple Developer Program. To open it:

* **Method 1 (Quick GUI)**:
  1. **Right-click** (or Control-click) `NANOCTRL.app` in Finder or `/Applications`.
  2. Click **Open** from the context menu.
  3. Click **Open** in the confirmation alert. (You only need to do this once).
  *(Alternatively, open **System Settings > Privacy & Security** and click **"Open Anyway"**)*.

* **Method 2 (Terminal)**:
  ```bash
  xattr -cr /Applications/NANOCTRL.app
  ```

---

## macOS Permissions

macOS requires two permissions for remote control:
- **Screen Recording**: Required to capture display frames. (System Settings > Privacy & Security > Screen Recording). If not enabled, NANOCTRL falls back to a synthetic animated test pattern for testing.
- **Accessibility**: Required to inject mouse and keyboard input events. (System Settings > Privacy & Security > Accessibility).
