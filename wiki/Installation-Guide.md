# Installation Guide

---

## 🍏 macOS Client Installation

### Option 1: Disk Image (.dmg) — Recommended
1. Download `NANOCTRL-<version>-macos.dmg` from the [GitHub Releases](https://github.com/devlopersabbir/nanoctrl/releases) page.
2. Double-click the `.dmg` file to mount it.
3. Drag **NANOCTRL.app** into `/Applications`.

### Option 2: Application Bundle (.zip)
1. Download `NANOCTRL-<version>-macos.app.zip`.
2. Extract the archive and place `NANOCTRL.app` in `/Applications`.

*(For Gatekeeper and macOS permissions, see the **[macOS Trust & Permissions](macOS-Trust-and-Permissions)** guide).*

---

## 🐧 Linux Server Installation (`nanosrv`)

### Option 1: Standalone Binary
```bash
# Download and extract binary
curl -LO https://github.com/devlopersabbir/nanoctrl/releases/download/v0.1.0/nanosrv-v0.1.0-linux-x86_64.tar.gz
tar -xzf nanosrv-v0.1.0-linux-x86_64.tar.gz
sudo cp nanosrv /usr/local/bin/

# Run server on port 7443
nanosrv 7443
```

### Option 2: Systemd Service (Auto-Start on Boot)
```bash
# Install and enable service
sudo cp nanosrv.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now nanosrv
```

---

## 🐳 Docker Deployment

### 1-Line Run (GitHub Container Registry)
```bash
docker run -d \
  --name nanoctrl-server \
  --restart unless-stopped \
  -p 7443:7443 \
  ghcr.io/devlopersabbir/nanoctrl:latest
```

---

## 🪟 Windows Server Installation

1. Download `nanosrv-v0.1.0-windows-x86_64.zip` from [GitHub Releases](https://github.com/devlopersabbir/nanoctrl/releases).
2. Extract the `.zip` archive.
3. Double-click `run-server.bat` to launch the relay server on port `7443`.
