# NANOCTRL (macOS Native & Self-Hosted WAN)

> **Tiny Remote Control, Nothing Else.**

NANOCTRL is an ultra-lightweight, zero-dependency remote-control suite written in pure C and Cocoa for macOS.

* **Binary Size**: ~106 KB (App) | ~51 KB (Standalone Self-Hosted Server `nanosrv`).
* **Zero 3rd-party dependencies**: Uses only native macOS system frameworks (`Cocoa`, `ScreenCaptureKit`, `CoreGraphics`, `CoreMedia`, `CoreVideo`) for client, and pure POSIX C for server.
* **Self-Hosted WAN Support (like RustDesk)**: Host your own ultra-fast rendezvous and relay server (`nanosrv`) on any Linux VPS or cloud instance for seamless remote control across NAT, firewalls, and the Internet.
* **Device ID & PIN Pairing**: Connect using a **9-digit Device ID** (`842 190 345`) and **6-digit PIN** (`582 914`) over the Internet, or connect directly via LAN (`IP:port`).
* **Fast binary protocol**: 8-byte message header, tile-based dirty rect detection (64x64 tiles), RLE run-length encoding.
* **Security first**: Temporary 6-digit PIN with HMAC-SHA256 challenge-response authentication and mandatory host approval prompt running end-to-end through relay tunnels.

---

## Architecture

```text
                       ┌───────────────────────────────┐
                       │  Self-Hosted Server (nanosrv) │
                       │    • Rendezvous & Registry    │
                       │    • High-Performance Relay   │
                       └───────▲───────────────▲───────┘
                               │               │
                     Outbound  │               │ Outbound
                    TCP Tunnel │               │ TCP Tunnel
                               │               │
              ┌────────────────┴───┐       ┌───┴────────────────┐
              │ Controller (WAN)   │       │ Host (WAN)         │
              │ Behind NAT / Home  │       │ Behind NAT / Home  │
              │ Enters: ID + PIN   │       │ ID:  842 190 345   │
              │                    │       │ PIN: 582 914       │
              └────────────────────┘       └────────────────────┘
```

---

## Building

Requires Xcode Command Line Tools (`clang` and macOS SDK).

```bash
# Build native GUI app and standalone server binary
make

# Build universal macOS application bundle (NANOCTRL.app)
make bundle

# Package distributable disk image (NANOCTRL.dmg)
make dmg
```

To run the automated test suite (protocol framing, tile diffing, RLE compression, network loopback, and self-hosted relay integration):

```bash
make test
```

To view the binary size report:

```bash
make size
```

---

## Self-Hosting (VPS / Docker)

You can run your own zero-dependency NANOCTRL relay server on any Linux/Cloud server (Ubuntu, Debian, Alpine, AWS, DigitalOcean, etc.).

### Option A: GitHub Container Registry (Fastest)
Run the pre-built multi-arch container image directly from GHCR with zero build step:

```bash
docker run -d \
  --name nanoctrl-server \
  --restart unless-stopped \
  -p 7443:7443 \
  ghcr.io/devlopersabbir/nanoctrl:latest
```

### Option B: Docker Compose
```bash
cd docker
docker compose up -d
```

### Option C: Standalone Binary (`nanosrv`)
```bash
# Compile standalone server on Linux or macOS (no frameworks needed)
make server

# Run on default port 7443 (or custom port)
./build/nanosrv 7443
```

---

## Usage

### 1. Native macOS Graphical Interface (Default)

Launch without arguments to open the native GUI:

```bash
./build/nanoctrl
```

* **Share This Mac (Host)**: Displays your unique 9-digit Device ID (e.g. `842 190 345`) and 6-digit PIN (e.g. `582 914`), registers with the relay server, prompts to ACCEPT or REJECT incoming requests, and allows stopping at any time.
* **Control Remote Computer (Controller)**: Enter the target **Device ID** or **Direct IP:Port**, enter the 6-digit PIN, click **[ CONNECT ]**, and control the remote desktop directly in a high-performance viewport window.

---

### 2. Command Line Interface (CLI)

#### A. Host via Self-Hosted Relay Server (WAN / Internet)
```bash
./build/nanoctrl --host --server-addr vps.example.com:7443 --cli
```

#### B. Controller via Self-Hosted Relay Server (WAN / Internet)
```bash
./build/nanoctrl --controller 842190345 582914 --server-addr vps.example.com:7443 --cli
```

#### C. Direct LAN Connection (Local Network)
Host on local port:
```bash
./build/nanoctrl --host 7443 --cli
```

Controller connecting directly via LAN IP:
```bash
./build/nanoctrl --controller 192.168.1.50:7443 582914 --cli
```

#### D. Run Built-In Diagnostics
```bash
./build/nanoctrl --test
```

---

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
