# How NANOCTRL Works

NANOCTRL operates in two primary networking modes: **Direct LAN Mode** and **Self-Hosted WAN Relay Mode**.

---

## 1. Direct LAN Mode

Used when both computers are on the same local network (e.g. home Wi-Fi or office network):

```text
┌────────────────────────┐                    ┌────────────────────────┐
│   Controller Computer  │──── Direct TCP ───▶│      Host Computer     │
│   (Remote Viewer)      │   (e.g., :7443)    │   (Display Source)     │
└────────────────────────┘                    └────────────────────────┘
```

* **Flow**: Controller connects directly to Host's IP address and port (e.g., `192.168.1.50:7443`).
* **Latency**: Near-zero latency with direct network routing.

---

## 2. Self-Hosted WAN Relay Mode

Used when computers are behind residential routers, NAT, cellular networks, or corporate firewalls:

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

1. **Host Registration**: Host connects outbound to the relay server and registers its **9-digit Device ID** (e.g., `842 190 345`).
2. **Controller Request**: Controller requests connection to the Host's Device ID.
3. **Session Token Pairing**: The server assigns a unique 64-bit session token and signals the Host.
4. **Relay Data Channel**: Both peers join the relay room, and the server bridges their raw encrypted streams at line speed.

---

## 3. Tile-Based Screen Streaming

To minimize CPU and network bandwidth:

* Display frames are segmented into **64x64 pixel tiles**.
* Each tile is compared against the prior frame in memory.
* Unchanged tiles are skipped entirely (0 bytes).
* Modified tiles are compressed using **Run-Length Encoding (RLE)** and transmitted.

---

## 4. Input Event Pipeline

* Controller coordinates are normalized into 16-bit integers (`0` to `65535`).
* Host receives events and converts them to native macOS `CoreGraphics` events (`CGEventCreateMouseEvent`, `CGEventCreateKeyboardEvent`) for pixel-accurate cursor and keyboard injection.
