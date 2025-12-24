# keyswap - Generic Input Remapping Tool

A configurable input remapping tool that reads remapping rules from JSON and remaps input events from any device (keyboard, mouse, gamepad, etc.) using libevdev.

## Features

- **JSON-based configuration** - Easy to configure and share remap profiles
- **Human-readable key names** - Use names like "back", "enter", "space" instead of numeric codes
- **Device auto-detection** - Automatically finds devices by name pattern
- **Multiple device support** - Remap multiple devices simultaneously
- **Debug mode** - Keylogging for discovering key codes
- **Generic event support** - Works with any input device (mouse, keyboard, gamepad, etc.)

## Building

```bash
make
```

Dependencies:
- `libevdev` (libevdev-dev)
- `jansson` (libjansson-dev)

## Usage

### Basic Usage

```bash
sudo ./keyswap [OPTIONS] [config_file]
```

If no config file is specified, it defaults to `index.json` in the current directory.

### Command Line Options

- `-l, --list` - List all available input devices and exit
- `-h, --help` - Show help message and exit

### Listing Available Devices

To discover which devices are available for remapping:

```bash
./keyswap --list
```

or

```bash
./keyswap -l
```

This will display all accessible input devices with their paths, names, physical locations, and device types. Use the device name to create a `name_match` pattern in your configuration.

### Configuration

The configuration follows the infiniteIndex pattern with remapping-specific schema:

```json
{
  "metadata": {
    "schema_version": "1.0.0",
    "name": "keyswap",
    "description": "Generic input remapping tool"
  },
  "paths": {
    "debug_log": "/tmp/keyswap-debug.log"
  },
  "config": {
    "debug": false,
    "devices": [
      {
        "uuid": "kensington-expert-mouse",
        "name_match": "Kensington",
        "remaps": [
          {
            "source": "back",
            "target": "enter"
          }
        ]
      }
    ]
  }
}
```

### Key Name Syntax

Keys can be specified in three ways:

1. **Human-readable names**: `"back"`, `"enter"`, `"space"`, `"left_click"`, etc.
2. **Canonical Linux names**: `"BTN_SIDE"`, `"KEY_ENTER"`, `"KEY_SPACE"`, etc.
3. **Numeric codes**: `275`, `28`, `57`, etc.

All matching is case-insensitive. Multiple aliases are supported (e.g., `back`, `back_button`, `side_button` all map to BTN_SIDE).

### Discovery Mode

To discover key codes for unknown keys:

1. Set `"debug": true` in config
2. Run keyswap: `sudo ./keyswap`
3. Press keys on your device
4. Check the debug log (default: `/tmp/keyswap-debug.log`)

Example log output:
```
[1234567890.123456] Kensington Expert Mouse: type=EV_KEY(1) code=BTN_SIDE(275) value=1
[1234567890.234567] Kensington Expert Mouse: type=EV_KEY(1) code=254 value=1  (unknown key)
```

### Example Configurations

**Mouse Button to Keyboard Key:**
```json
{
  "config": {
    "devices": [{
      "name_match": "Kensington",
      "remaps": [
        {"source": "back", "target": "enter"},
        {"source": "forward", "target": "space"}
      ]
    }]
  }
}
```

**Mouse Button to Mouse Button:**
```json
{
  "config": {
    "devices": [{
      "name_match": "Mouse",
      "remaps": [
        {"source": "left_click", "target": "middle_click"}
      ]
    }]
  }
}
```

**Keyboard Remapping:**
```json
{
  "config": {
    "devices": [{
      "name_match": "Keyboard",
      "remaps": [
        {"source": "caps_lock", "target": "left_control"}
      ]
    }]
  }
}
```

## How It Works

1. **Configuration Loading**: Loads `index.json` and resolves key names to event codes
2. **Device Discovery**: Scans `/dev/input/event*` devices and matches by name pattern
3. **Device Setup**: Opens device, grabs it exclusively, creates virtual uinput devices
4. **Event Processing**: 
   - Reads events from device
   - Matches against remapping rules
   - **CONSUME** matched events (don't forward)
   - **INJECT** remapped events to virtual keyboard/mouse
   - **FORWARD** non-matched events to virtual device

## Architecture

Following the infiniteIndex pattern:
- `index.json` - Configuration file
- `keyswap.c` - Main orchestrator
- `key-database.c/h` - Built-in key name lookup table
- `config-loader.c/h` - JSON config loading (jansson)
- `device-matcher.c/h` - Device discovery and matching
- `event-processor.c/h` - Event processing loop
- `debug-logger.c/h` - Debug logging

## License

GPLv3 (see LICENSE file)
