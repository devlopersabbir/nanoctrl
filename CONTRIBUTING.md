# Contributing to NANOCTRL

Thank you for your interest in contributing to **NANOCTRL**! We welcome bug reports, feature discussions, and pull requests.

---

## 🎯 Core Design Principles

Before proposing changes or new features, please keep in mind NANOCTRL's core philosophy:
1. **Tiny Binary Size**: Budget < 2.0 MB (currently ~106 KB). Avoid adding heavy libraries or dependencies.
2. **Zero 3rd-Party Dependencies**: Pure C and native system frameworks only.
3. **Simplicity First**: "Tiny Remote Control, Nothing Else." No chat, accounts, cloud sync, or unnecessary bloat.

---

## 🛠️ Development & Building

### Requirements:
* macOS with Xcode Command Line Tools (`clang`) or Linux with standard `gcc`/`clang`.

### Quick Commands:
```bash
# Build binary
make

# Run test suite
make test

# Build application bundle (.app) and disk image (.dmg)
make bundle
make dmg
```

---

## 📬 Submitting Changes

1. **Fork** the repository and create a feature branch.
2. **Write clean C code** adhering to existing style conventions.
3. Ensure all tests pass with `make test`.
4. Open a **Pull Request** describing your changes clearly.
