# macOS Trust & Permissions Guide

---

## 🛡️ Gatekeeper Authorization

Because NANOCTRL is open-source software built outside the paid Apple Developer Program, macOS Gatekeeper may show a security notice on first launch:

> *"Apple could not verify NANOCTRL is free of malware..."*

You can authorize NANOCTRL using either of the following methods:

### ⚡ Method A: Terminal One-Liner (Instant)
Open your Terminal and run:
```bash
xattr -cr /Applications/NANOCTRL.app
```

### 🖱️ Method B: Finder Right-Click (No Terminal)
1. Open your `/Applications` folder in Finder.
2. **Right-click** (or Control-click) `NANOCTRL.app`.
3. Select **Open** from the menu.
4. Click **Open** in the confirmation alert dialog. *(You only need to do this once)*.

*(Alternatively: Go to **System Settings $\rightarrow$ Privacy & Security**, scroll down to Security, and click **"Open Anyway"**)*.

---

## 🔒 Required macOS Permissions

To enable remote screen capture and mouse/keyboard injection, macOS requires two permissions:

### 1. Screen Recording
* **Purpose**: Allows NANOCTRL to capture and stream the display.
* **Setup**: Go to **System Settings $\rightarrow$ Privacy & Security $\rightarrow$ Screen Recording** and toggle **NANOCTRL** to ON.

### 2. Accessibility
* **Purpose**: Allows NANOCTRL to inject remote mouse movements, clicks, and keystrokes.
* **Setup**: Go to **System Settings $\rightarrow$ Privacy & Security $\rightarrow$ Accessibility** and toggle **NANOCTRL** to ON.

---

## 🔍 Verification
To verify that the application bundle is properly signed with Hardened Runtime:
```bash
codesign -v /Applications/NANOCTRL.app
```
