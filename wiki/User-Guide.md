# User Guide & Workflows

---

## ⚙️ Initial Setup: Connecting to your Server

> **Important**: Both Host and Controller must connect to the **same** `nanoctrl-server` instance to find each other.

### In the Desktop Application:
1. Open **NANOCTRL**.
2. Under **Relay Server (Docker / VPS)**, enter your server address (e.g. `vps.example.com:7443` or your Docker container IP).
3. The server address is **automatically saved** permanently across all restarts.

### From Command Line (All Platforms):
```bash
# Save server address permanently on this machine:
./build/nanoctrl --set-server vps.example.com:7443

# Check currently active configured server:
./build/nanoctrl --get-server
```

---

## 🖥️ Workflow 1: Sharing Your Screen (Host)

### Using Graphical Interface:
1. Open **NANOCTRL** (verify **Relay Server** matches your controller's server).
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
# Connect via configured server automatically
./build/nanoctrl --host --cli

# Or specify a custom server on the fly
./build/nanoctrl --host --server-addr vps.example.com:7443 --cli

# Direct local network hosting (bypassing relay)
./build/nanoctrl --host 7443 --cli
```

---

## 🕹️ Workflow 2: Controlling a Remote Computer (Controller)

### Using Graphical Interface:
1. Open **NANOCTRL** (verify **Relay Server** matches your host's server).
2. Click **Control Remote Computer**.
3. In the **Device ID / IP:Port** field, enter the target's **Device ID** (e.g., `842 190 345`) or **Direct IP** (e.g., `192.168.1.50:7443`).
4. Enter the **6-digit PIN**.
5. The **Relay Server** field is prefilled with your configured server (or enter a custom one).
6. Click **[ CONNECT ]**.
7. Control the remote desktop in the dedicated viewport window.

### Using Command Line:
```bash
# Connect via configured server automatically
./build/nanoctrl --controller 842190345 582914 --cli

# Or specify a custom server on the fly
./build/nanoctrl --controller 842190345 582914 --server-addr vps.example.com:7443 --cli

# Connect directly via LAN IP
./build/nanoctrl --controller 192.168.1.50:7443 582914 --cli
```
