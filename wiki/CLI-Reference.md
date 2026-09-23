# Command-Line Interface (CLI) Reference

---

## 💻 `nanoctrl` (Client Application)

```text
Usage:
  nanoctrl                              Launch native macOS GUI
  nanoctrl --host [port] [options]      Start hosting (sharing screen)
  nanoctrl --controller <target> <pin>  Connect to remote host
  nanoctrl --server [port]              Run embedded relay server
  nanoctrl --test                       Run built-in self-diagnostics
  nanoctrl --version, -v                Print version
  nanoctrl --help, -h                   Show help
```

### Options:
* `--server-addr <host:port>`: Connect through a self-hosted relay server.
* `--pin <6-digit-pin>`: Set a custom 6-digit PIN.
* `--id <9-digit-id>`: Set a custom 9-digit Device ID.
* `--auto-accept`: Automatically accept incoming connections without prompting.
* `--cli`: Run in command-line mode without opening GUI windows.

### Examples:
```bash
# 1. Host with self-hosted server:
./build/nanoctrl --host --server-addr vps.example.com:7443 --cli

# 2. Controller connecting by Device ID:
./build/nanoctrl --controller 842190345 582914 --server-addr vps.example.com:7443 --cli

# 3. Direct LAN connection:
./build/nanoctrl --controller 192.168.1.50:7443 582914 --cli

# 4. Self-test diagnostics:
./build/nanoctrl --test
```

---

## 🌐 `nanosrv` (Standalone Relay Server)

```text
Usage:
  nanosrv [port] [options]
```

### Arguments:
* `[port]`: TCP listen port (default: `7443`).

### Options:
* `-v, --version`: Print server version.
* `-h, --help`: Show help.
