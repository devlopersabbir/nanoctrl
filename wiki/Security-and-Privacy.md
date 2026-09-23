# Security & Privacy Model

---

## 🔐 Cryptographic Handshake

NANOCTRL does not transmit PINs in plain text. Instead, it uses **HMAC-SHA256 Challenge-Response**:

```text
Controller                                          Host
    │                                                 │
    ├──────────── 1. NANO_MSG_HELLO ─────────────────▶│
    │                                                 │
    │◀─────────── 2. AUTH_CHALLENGE (32-byte Nonce) ──┤
    │                                                 │
    │ (Computes HMAC-SHA256(Nonce, 6-digit PIN))      │
    ├──────────── 3. AUTH_RESPONSE (32-byte Hash) ───▶│
    │                                                 │
    │                                   (Verifies HMAC-SHA256)
    │                                   (Prompts Host User: [ACCEPT / REJECT])
    │                                                 │
    │◀─────────── 4. AUTH_RESULT (OK) ────────────────┤
```

* **Nonce Freshness**: Every connection uses a newly generated 32-byte random cryptographic nonce, rendering replay attacks impossible.
* **Brute-Force Immunity**: PIN verification happens on the host; failed attempts immediately terminate the TCP session.

---

## 🛡️ Host Authorization (Zero Unattended Access by Default)

Even if a connecting peer knows the PIN, control is never granted automatically. A critical native modal alert requires physical user confirmation:

```text
>>> Remote Control Request <<<
A controller at [IP / Relay Peer] wants to control your computer.
[ ACCEPT ]     [ REJECT ]
```

---

## 👁️ Zero Telemetry & Privacy

* **No Cloud Accounts**: No logins, no registration, and no centralized databases.
* **No Analytics**: NANOCTRL does not make background HTTP requests, track IP addresses, or collect telemetry.
* **Blind Relay Forwarding**: The relay server (`nanosrv`) forwards opaque byte packets based on random 64-bit session tokens. It cannot tamper with the authenticated data stream.
