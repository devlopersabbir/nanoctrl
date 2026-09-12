# NANOCTRL

### Tiny Remote Control, Nothing Else.

## 1. Mission

Build an extremely small, native, cross-platform remote-control application whose primary goal is:

> **Connect → Accept → Control**

Target:

* Core executable: **as small as practically possible**
* Stretch goal: **< 2 MB**
* No Electron
* No Chromium
* No embedded browser
* No database
* No account system
* No voice
* No chat
* No file manager
* No unnecessary UI
* No unnecessary dependencies

The application should feel almost like a tiny system utility rather than a conventional remote-desktop application.

---

# 2. MVP

The first version does only this:

### Host

```text
Start NANOCTRL
      ↓
Generate temporary code
      ↓
Wait
      ↓
Someone requests connection
      ↓
[ ACCEPT ]
      ↓
Remote user can control machine
```

### Controller

```text
Start NANOCTRL
      ↓
Enter code
      ↓
[ CONNECT ]
      ↓
See remote screen
      ↓
Move mouse
      ↓
Click
      ↓
Type
```

That's it.

---

# 3. Explicitly NOT in MVP

Do not add these:

* Voice
* Video calling
* Chat
* File transfer
* Clipboard synchronization
* Accounts
* User profiles
* Teams
* Cloud storage
* Recording
* Multi-monitor
* Remote printing
* Remote terminal
* Browser extension
* Automatic startup
* Persistent device registration
* Fancy animations
* Themes
* Plugin system

Every additional feature is a potential increase in:

* binary size
* attack surface
* complexity
* memory usage
* CPU usage
* maintenance

---

# 4. Size Philosophy

The project has a different rule from normal software:

> **Every dependency must justify its existence.**

Before adding a library:

```text
Does it save more code than it adds?
Does it increase binary size?
Can we implement the required functionality ourselves?
Can the operating system already provide this functionality?
```

If the OS already provides something, use the OS API.

---

# 5. Language Strategy

## Phase 1 — C

Use C as the primary implementation language.

Reasons:

* extremely small binaries are possible
* direct OS APIs
* no garbage collector
* no large runtime
* precise memory control
* excellent compiler/linker optimization
* easy integration with assembly

Possible structure:

```text
C
├── protocol
├── networking
├── authentication
├── screen capture
├── input
├── UI
└── platform abstraction
```

## Phase 2 — Assembly

Assembly is an optimization tool, not the foundation.

Only use assembly where benchmarking proves it helps.

Potential targets:

```text
screen comparison
memory copying
pixel operations
compression primitives
cryptographic primitives
bit manipulation
```

Do NOT rewrite the entire application in assembly just because it sounds smaller.

C + carefully selected assembly is likely a better engineering solution.

---

# 6. Platform Strategy

Do NOT attempt Windows + macOS + Linux simultaneously.

Start with:

> **Windows x64**

Why?

It lets us prove the core concept and measure the executable size quickly.

Then:

```text
Windows
   ↓
Linux
   ↓
macOS
```

Each platform gets a small native adapter.

Architecture:

```text
             NANOCTRL Core
                   │
        ┌──────────┼──────────┐
        │          │          │
     Windows     Linux      macOS
     adapter     adapter    adapter
```

The core protocol remains identical.

---

# 7. Repository

```text
nanoctrl/
│
├── src/
│   ├── core/
│   │   ├── session.c
│   │   ├── session.h
│   │   ├── protocol.c
│   │   └── protocol.h
│   │
│   ├── net/
│   │   ├── socket.c
│   │   ├── socket.h
│   │   ├── transport.c
│   │   └── transport.h
│   │
│   ├── screen/
│   │   ├── capture.c
│   │   ├── capture.h
│   │   ├── diff.c
│   │   └── diff.h
│   │
│   ├── input/
│   │   ├── mouse.c
│   │   ├── mouse.h
│   │   ├── keyboard.c
│   │   └── keyboard.h
│   │
│   ├── crypto/
│   │   ├── auth.c
│   │   ├── auth.h
│   │   └── random.c
│   │
│   ├── ui/
│   │   ├── host.c
│   │   ├── controller.c
│   │   └── ui.h
│   │
│   └── platform/
│       └── windows/
│           ├── screen_win.c
│           ├── input_win.c
│           └── socket_win.c
│
├── server/
│   └── signaling/
│
├── asm/
│   └── x64/
│
├── tests/
│
├── tools/
│
├── build/
│
└── README.md
```

Keep the core extremely modular.

---

# 8. Architecture

```text
                  ┌──────────────────┐
                  │ Signaling Server │
                  │                  │
                  │ Code → endpoint  │
                  └────────┬─────────┘
                           │
                     setup only
                           │
             ┌─────────────┴─────────────┐
             │                           │
       ┌─────▼─────┐               ┌─────▼─────┐
       │   HOST    │◄─────────────►│ CONTROLLER│
       │ NANOCTRL  │   P2P traffic │ NANOCTRL  │
       └───────────┘               └───────────┘
             │
             ├── Screen capture
             ├── Screen diff
             └── Input injection
```

The signaling server should NOT transport the screen.

Its job is only:

```text
code
 ↓
find peer
 ↓
exchange connection information
 ↓
establish connection
 ↓
done
```

---

# 9. Connection Protocol

Keep the protocol binary.

Do NOT use JSON for the actual remote-control channel.

Instead:

```text
┌────────┬────────┬────────┬─────────────┐
│ TYPE   │ FLAGS  │ LENGTH │ PAYLOAD     │
│ 1 byte │ 1 byte │ 2/4 B  │ variable    │
└────────┴────────┴────────┴─────────────┘
```

Example messages:

```text
HELLO
AUTH
SCREEN
SCREEN_DIFF
MOUSE_MOVE
MOUSE_BUTTON
KEY_DOWN
KEY_UP
PING
PONG
DISCONNECT
```

This keeps the protocol tiny and efficient.

---

# 10. Pairing

Host generates a temporary code:

```text
847291
```

Controller enters:

```text
847291
```

But the code itself should NOT be the security mechanism.

Internally:

```text
Temporary Code
      ↓
Session lookup
      ↓
Cryptographic authentication
      ↓
Encrypted connection
```

The code should:

* expire
* be temporary
* become invalid after disconnect
* never be reused indefinitely

---

# 11. Host Approval

Never silently allow remote control.

Host receives:

```text
Connection request

Someone wants to control this computer.

[ ACCEPT ]   [ REJECT ]
```

After accepting:

```text
● REMOTE SESSION ACTIVE

[ STOP ]
```

The host must always have an obvious way to terminate the session.

---

# 12. Screen Capture

This is one of the most important components.

Do NOT immediately build a traditional video-streaming system.

Start with:

```text
Capture screen
      ↓
Compare with previous frame
      ↓
Find changed regions
      ↓
Send only changed regions
```

Example:

```text
Previous:

┌─────────────────────┐
│                     │
│      Terminal       │
│                     │
└─────────────────────┘

Current:

┌─────────────────────┐
│                     │
│      Terminal       │
│     new text        │
│                     │
└─────────────────────┘
```

Send:

```text
X
Y
WIDTH
HEIGHT
PIXEL DATA
```

instead of the entire screen.

---

# 13. Screen Optimization

Implement progressively.

### Level 1

Full frame.

```text
Capture → Send
```

Purpose:

> Prove the system works.

### Level 2

Dirty-region detection.

```text
Capture
 ↓
Compare
 ↓
Changed rectangles
 ↓
Send
```

### Level 3

Tile-based comparison.

Example:

```text
Screen
┌──┬──┬──┬──┐
│  │  │  │  │
├──┼──┼──┼──┤
│  │██│██│  │
├──┼──┼──┼──┤
│  │  │  │  │
└──┴──┴──┴──┘
```

Only changed tiles are transmitted.

### Level 4

Optimize pixel comparison with SIMD/assembly where useful.

---

# 14. Compression

Do not immediately introduce a huge codec.

Benchmark several approaches:

```text
Raw changed pixels
       ↓
RLE
       ↓
Lightweight custom compression
       ↓
Existing tiny codec
```

The winner should be selected based on:

```text
binary size
CPU usage
bandwidth
latency
image quality
```

The goal is not "best compression."

The goal is:

> **best compression/size/latency ratio for remote desktop content.**

---

# 15. Input System

Controller sends tiny events.

Mouse:

```text
MOVE x y
LEFT_DOWN
LEFT_UP
RIGHT_DOWN
RIGHT_UP
```

Keyboard:

```text
KEY_DOWN key
KEY_UP key
```

Avoid sending large structures.

The host translates these into native OS input events.

---

# 16. Networking

Start simple.

### Prototype

TCP:

```text
Controller ───────── TCP ──────── Host
```

This gives us the simplest possible implementation.

Then benchmark latency.

If necessary:

```text
UDP
 ↓
custom reliable protocol
```

Later:

```text
UDP
 ↓
NAT traversal
 ↓
direct P2P
```

Do not build complicated NAT traversal before the basic remote desktop works.

---

# 17. Encryption

Security is mandatory.

But again:

> Do not import a huge framework just for one feature.

Use small, audited cryptographic primitives where possible.

Architecture:

```text
Connection
    ↓
Handshake
    ↓
Authentication
    ↓
Session key
    ↓
Encrypted packets
```

Never send:

```text
keyboard
mouse
screen
```

over plaintext production connections.

---

# 18. UI

The UI should be almost nothing.

Host:

```text
┌────────────────────┐
│      NANOCTRL      │
│                    │
│      847 291       │
│                    │
│      WAITING       │
│                    │
│      [ STOP ]      │
└────────────────────┘
```

Controller:

```text
┌────────────────────┐
│      NANOCTRL      │
│                    │
│   Connection Code  │
│                    │
│      [______]      │
│                    │
│     [ CONNECT ]    │
└────────────────────┘
```

No web UI.

No embedded HTML.

No giant UI framework.

Use native OS controls.

---

# 19. Build System

The build must aggressively optimize size.

Conceptually:

```text
-Oz
-LTO
-dead code elimination
-section garbage collection
-strip symbols
-minimal runtime
-static only where beneficial
```

But:

> Never sacrifice security or stability merely to remove a few KB.

Create a size report after every build.

Example:

```text
NANOCTRL BUILD

Binary:       418 KB
Code:         281 KB
Data:          43 KB
Resources:     19 KB
Imports:       75 KB

Target:      < 2 MB
Status:        PASS
```

---

# 20. Size Budget

Set an aggressive budget from day one.

```text
                  Target
────────────────────────────
Core code          300 KB
Networking         150 KB
Crypto             150 KB
Screen             300 KB
Input               50 KB
UI                 100 KB
Protocol            50 KB
Resources           50 KB
────────────────────────────
Target              < 1.2 MB
```

These are engineering targets, not guaranteed final numbers.

Then leave:

```text
~800 KB
```

for platform-specific code, optimization differences, and future requirements.

---

# 21. Assembly Strategy

After the C implementation works:

### Benchmark

Find:

```text
Top CPU consumers
Top binary-size contributors
```

Then consider assembly.

Potential candidates:

```text
memcpy-like operations
pixel comparison
CRC/checksum
bit manipulation
SIMD operations
compression hot paths
```

Do NOT optimize blindly.

The rule:

```text
C implementation
      ↓
Benchmark
      ↓
Identify bottleneck
      ↓
Assembly implementation
      ↓
Benchmark again
      ↓
Keep only if better
```

---

# 22. Development Milestones

## M0 — Tiny executable

Goal:

```text
hello.exe
```

under an extremely small size.

Learn exactly what the compiler/linker adds.

---

## M1 — Native UI

Build:

```text
NANOCTRL
847291
Waiting...
```

No networking yet.

Measure binary size.

---

## M2 — Local screen capture

Capture the screen.

No networking.

Save/display frames.

Measure:

```text
CPU
RAM
binary size
capture latency
```

---

## M3 — Local input

Implement:

```text
mouse
keyboard
```

and verify the native OS APIs.

---

## M4 — LAN remote control

Two computers on the same network.

```text
Host
 ↓
TCP
 ↓
Controller
```

No Internet yet.

Goal:

> Full remote control over LAN.

---

## M5 — Screen optimization

Implement:

```text
frame diff
 ↓
dirty rectangles
 ↓
tiles
 ↓
compression
```

Benchmark each version.

---

## M6 — Security

Implement:

```text
pairing
authentication
encryption
session expiration
```

---

## M7 — Signaling server

Add:

```text
temporary code
peer discovery
connection negotiation
```

---

## M8 — Internet connection

Move from:

```text
LAN
```

to:

```text
Internet
```

Implement NAT traversal progressively.

---

## M9 — Windows release

Produce:

```text
nanoctrl.exe
```

and measure:

```text
file size
RAM
CPU
latency
bandwidth
startup time
```

---

## M10 — Linux

Reuse the core.

Replace:

```text
screen_win.c
input_win.c
```

with Linux implementations.

---

## M11 — macOS

Add:

```text
screen capture
accessibility/input
native packaging
```

with the required macOS permissions.

---

# 23. Performance Targets

Eventually aim for:

```text
Startup:       < 100 ms
Idle RAM:      as low as practical
CPU idle:      ~0%
Input latency: very low
LAN FPS:       30+
Internet FPS:  usable 15–30+
Binary:        < 2 MB target
```

Again, these are targets, not promises.

---

# 24. Security Rules

This project controls people's computers, so security is more important than size.

Mandatory:

```text
Temporary pairing codes
Explicit host approval
Encrypted session
Session expiration
Connection timeout
Visible active-session indicator
Instant disconnect
No hidden persistence
No silent control
No credential collection
```

And before public release:

> independent security review / serious penetration testing.

---

# 25. What Makes NANOCTRL Different

The positioning isn't:

> "We're another AnyDesk."

It's:

> **"Why does a remote-control application need hundreds of megabytes of technology around it?"**

NANOCTRL's philosophy:

```text
One executable.

One code.

One connection.

One purpose.
```

---

# 26. Final Architecture

```text
                         INTERNET
                            │
                            │
                    ┌───────▼───────┐
                    │   SIGNALING   │
                    │     SERVER    │
                    │               │
                    │ Code + Handshake
                    └───────┬───────┘
                            │
                    connection setup
                            │
               ┌────────────┴────────────┐
               │                         │
        ┌──────▼──────┐           ┌──────▼──────┐
        │    HOST     │           │ CONTROLLER  │
        │             │           │             │
        │ Screen      │           │ Screen view │
        │ Capture     │           │             │
        │             │           │ Mouse       │
        │ Input       │◄─────────►│ Keyboard    │
        │ Injection   │   P2P     │             │
        └─────────────┘           └─────────────┘
```

---

# 27. The Golden Rule

Throughout development, keep asking:

> **"Do we actually need this?"**

If the answer is no:

**Delete it.**

That includes code, libraries, abstractions, UI, protocols and features.

NANOCTRL should be deliberately boring.

That's the point.

---

# 28. Long-Term Vision

If the project succeeds:

```text
NANOCTRL
│
├── Windows
├── Linux
└── macOS
```

with:

```text
< 2 MB target
native
fast
secure
P2P
no account
no cloud dependency for actual screen traffic
```

Future optional features can exist, but the core should remain tiny.

The ideal experience:

```text
DOWNLOAD
   ↓
OPEN
   ↓
847291
   ↓
"Send this code to your friend."
   ↓
ACCEPT
   ↓
CONTROL
```

### Project motto

> **NANOCTRL**
>
> **Tiny enough to disappear. Powerful enough to control.**

