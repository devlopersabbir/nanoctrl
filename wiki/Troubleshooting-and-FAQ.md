# Troubleshooting & FAQ

---

## ❓ Frequently Asked Questions

### Q: Why is my remote screen showing a gradient color test pattern?
**A**: macOS Screen Recording permission is not granted for NANOCTRL.
* **Fix**: Go to **System Settings $\rightarrow$ Privacy & Security $\rightarrow$ Screen Recording** and toggle **NANOCTRL** to ON.

### Q: I can see the screen, but mouse clicks and keyboard typing have no effect.
**A**: Accessibility input injection permission is missing.
* **Fix**: Go to **System Settings $\rightarrow$ Privacy & Security $\rightarrow$ Accessibility** and toggle **NANOCTRL** to ON.

### Q: Controller says "Connection failed or timed out".
* **Direct LAN**: Verify both devices are on the same Wi-Fi/LAN and no local firewall blocks port `7443`.
* **Relay Mode**: Verify your self-hosted server is running (`nanosrv`) and TCP port `7443` is open in your cloud VPS firewall.

### Q: How do I run diagnostics?
Run the built-in self-diagnostic suite:
```bash
./build/nanoctrl --test
```
