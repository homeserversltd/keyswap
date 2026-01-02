# keyswap

Generic input remapping tool using libevdev. Remaps input events from any device (keyboard, mouse, gamepad) via JSON configuration.

## Quick Start

```bash
make
sudo ./controller.sh load config.json
sudo ./controller.sh list
```

## Building

```bash
make
```

**Dependencies:**
- `libevdev` (libevdev-dev)
- `jansson` (libjansson-dev)

## Usage

### Direct Execution

```bash
sudo ./keyswap [config_file]
```

- Default config: `index.json` in current directory
- Options: `-l, --list` (list devices), `-h, --help`

### Controller Commands

| Command | Description |
|---------|-------------|
| `load <config>` | Load config as systemd service `keyswap-{name}` |
| `list` | List active services with status |
| `delete <name\|number>` | Stop, disable, and remove service |
| `listen [ID\|config]` | Monitor device events in real-time (interactive) |
| `keys` | Show key reference mapping (supported key names and aliases) |

**Examples:**
```bash
sudo ./controller.sh load kensington.json
sudo ./controller.sh list
sudo ./controller.sh delete 1
sudo ./controller.sh listen
sudo ./controller.sh listen 046d:c08b
sudo ./controller.sh listen /dev/input/event8
sudo ./controller.sh listen config.json
./controller.sh keys
```

**Service Management:**
```bash
systemctl status keyswap-{name}.service
journalctl -u keyswap-{name}.service -f
systemctl start|stop|restart keyswap-{name}.service
```

### Device Discovery

```bash
./keyswap --list
```

Lists all input devices with paths, names, and types. Use device name for `name_match` in config.

### Listen Mode

Monitor device events in real-time without grabbing device:

```bash
# Listen to devices from config
sudo ./controller.sh listen

# Listen to specific device by vendor:product
sudo ./controller.sh listen 046d:c08b

# Listen to specific event path
sudo ./controller.sh listen /dev/input/event8

# Listen to devices from specific config
sudo ./controller.sh listen config.json
```

Displays events as they occur. Press Ctrl+C to stop.

## Configuration

JSON schema following infiniteIndex pattern:

```json
{
  "metadata": {
    "schema_version": "1.0.0",
    "name": "keyswap"
  },
  "config": {
    "debug": false,
    "devices": [{
      "uuid": "device-id",
      "name_match": "Device Name Pattern",
      "remaps": [
        {"source": "back", "target": "enter"}
      ]
    }]
  }
}
```

### Key Name Syntax

| Format | Examples |
|--------|----------|
| Human-readable | `"back"`, `"enter"`, `"space"`, `"left_click"` |
| Canonical Linux | `"BTN_SIDE"`, `"KEY_ENTER"`, `"KEY_SPACE"` |
| Numeric codes | `275`, `28`, `57` |

- Case-insensitive matching
- Multiple aliases supported (e.g., `back`, `back_button`, `side_button` → BTN_SIDE)

### Debug Mode

1. Set `"debug": true` in config
2. Run: `sudo ./keyswap config.json`
3. Press keys on device
4. Check log: `/tmp/keyswap-debug.log`

## Architecture

```
keyswap/
├── keyswap.c              # Main orchestrator
├── key-database.c/h        # Key name lookup table
├── config-loader.c/h      # JSON config loading (jansson)
├── device-matcher.c/h     # Device discovery and matching
├── event-processor.c/h    # Event processing loop
├── debug-logger.c/h       # Debug logging
└── controller.sh          # Systemd service management
```

**Processing Flow:**
1. Load config → resolve key names to event codes
2. Discover devices → scan `/dev/input/event*`, match by `name_match`
3. Setup devices → grab exclusively, create virtual uinput devices
4. Process events → consume matched, inject remapped, forward unmatched

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Service fails to start | Check logs: `journalctl -u keyswap-{name}.service` |
| Device not found | Verify device name with `./keyswap --list` |
| Permission denied | Run with `sudo` (requires root for `/dev/input` access) |
| Key not remapping | Enable debug mode, check log for key codes |
| Service not starting on boot | Verify enabled: `systemctl is-enabled keyswap-{name}.service` |

## License

GPLv3 (see LICENSE file)
