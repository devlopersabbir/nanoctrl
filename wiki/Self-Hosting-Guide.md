# Self-Hosting Guide

Running your own NANOCTRL relay server (`nanosrv`) gives you complete privacy and global connectivity behind NAT and firewalls.

---

## 🚀 Recommended: Docker Deployment

### Run Container Directly:
```bash
docker run -d \
  --name nanoctrl-server \
  --restart unless-stopped \
  -p 7443:7443 \
  ghcr.io/devlopersabbir/nanoctrl:latest
```

### Or using Docker Compose:
```yaml
services:
  nanosrv:
    image: ghcr.io/devlopersabbir/nanoctrl:latest
    container_name: nanoctrl-relay-server
    restart: unless-stopped
    ports:
      - "7443:7443/tcp"
```

---

## 🐧 Linux VPS (Systemd Service)

1. **Install Binary**:
   ```bash
   curl -LO https://github.com/devlopersabbir/nanoctrl/releases/download/v0.1.0/nanosrv-v0.1.0-linux-x86_64.tar.gz
   tar -xzf nanosrv-v0.1.0-linux-x86_64.tar.gz
   sudo cp nanosrv /usr/local/bin/
   ```

2. **Configure Systemd**:
   ```bash
   sudo cp nanosrv.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now nanosrv
   ```

3. **Check Logs**:
   ```bash
   journalctl -u nanosrv -f
   ```

---

## 🔒 Firewall Configuration

Ensure TCP port `7443` (or your customized port) is open:

* **UFW (Ubuntu/Debian)**:
  ```bash
  sudo ufw allow 7443/tcp
  ```
* **Firewalld (CentOS/RHEL/Fedora)**:
  ```bash
  sudo firewall-cmd --permanent --add-port=7443/tcp
  sudo firewall-cmd --reload
  ```
* **AWS / Cloud Security Groups**: Add an Inbound Rule for **Custom TCP**, Port `7443`, Source `0.0.0.0/0`.
