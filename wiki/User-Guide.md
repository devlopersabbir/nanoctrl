# User Guide & Workflows

---

## 🖥️ Workflow 1: Sharing Your Screen (Host)

### Using Native macOS GUI:
1. Open **NANOCTRL**.
2. Click **Share This Mac (Host)**.
3. Your window displays:
   * **Your Device ID**: e.g., `842 190 345`
   * **One-Time PIN**: e.g., `582 914`
   * **Server Status**: e.g., `WAITING FOR CONTROLLER...`
4. Provide the **Device ID** and **PIN** to the controller.
5. When the connection prompt appears, click **[ ACCEPT ]**.
6. Click **[ STOP ]** at any time to immediately disconnect.

### Using Command Line:
```bash
# Connect to self-hosted relay server
./build/nanoctrl --host --server-addr vps.example.com:7443 --cli

# Direct local network hosting
./build/nanoctrl --host 7443 --cli
```

---

## 🕹️ Workflow 2: Controlling a Remote Computer (Controller)

### Using Native macOS GUI:
1. Open **NANOCTRL**.
2. Click **Control Remote Computer**.
3. In the **Device ID / IP:Port** field, enter the target's **Device ID** (e.g., `842 190 345`) or **Direct IP** (e.g., `192.168.1.50:7443`).
4. Enter the **6-digit PIN**.
5. (Optional) In **Relay Server**, enter your server address (default: `127.0.0.1:7443` or your VPS).
6. Click **[ CONNECT ]**.
7. Control the remote desktop in the dedicated viewport window.

### Using Command Line:
```bash
# Connect via self-hosted server by Device ID
./build/nanoctrl --controller 842190345 582914 --server-addr vps.example.com:7443 --cli

# Connect directly via LAN IP
./build/nanoctrl --controller 192.168.1.50:7443 582914 --cli
```
