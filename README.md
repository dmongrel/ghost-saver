# ghost-saver

A Windows screen saver designed to remove ghosting/burn-in artifacts from monitors by cycling through solid colors.

## How It Works

The screensaver follows a repeating cycle:

1. **Color Cycle (~75–105 seconds)**: Fades the whole screen through black, red, green, blue, yellow, cyan, purple and white, then back to black. Each fade and each hold lasts a random 3–5 seconds.
2. **Black Phase (30 seconds)**: Displays a completely black screen, allowing the display to fully rest before the next cycle.
3. **Repeat**: The cycle continues indefinitely until any keyboard or mouse input is detected.

## Building

### Prerequisites
- MinGW-w64 (x86_64) for Windows
- Make
- windres (part of MinGW-w64)

### Build Commands

```bash
# Build the screensaver
make

# Clean build artifacts
make clean

# Install to Windows System32 directory (requires Administrator)
make install
```

### Manual Build

```bash
windres ghost-saver.rc -O coff -o ghost-saver.res
g++ -std=c++11 -Wall -Wextra -O2 -DUNICODE -D_UNICODE -o ghost-saver.scr main.cpp ghost-saver.res -mwindows -static -lgdi32 -lshell32
```

## Installation

1. Build the project: `make`
2. Copy to Windows directory from an elevated prompt:
   ```cmd
   copy ghost-saver.scr %SystemRoot%\System32\
   ```
   Or right-click `ghost-saver.scr` and select **Install**.
3. Set as screensaver:
   - Right-click Desktop → Personalize → Lock screen → Screen saver
   - Select "Ghost Saver" from the dropdown
   - Set desired wait time
   - Click OK

## Command-Line Arguments

| Argument | Description |
|----------|-------------|
| (none) / `/s` | Run the screensaver in full-screen mode (all monitors) |
| `/p <hwnd>` or `/p:<hwnd>` | Show animation in a child window (preview for settings dialog) |
| `/c` or `/c:<hwnd>` | Show information message box (no configuration available) |
| Any other argument | Exit immediately with code 0 |

Arguments are case-insensitive and accept both `/` and `-` as the prefix.

## Features

- Covers all monitors via virtual screen sizing
- Topmost window that stays above all other content
- Hides cursor during full-screen mode
- Responds to any keyboard/mouse input by exiting immediately
- No configuration settings — simple and focused
- Pure Win32 API with no external dependencies
- DPI-aware (PerMonitorV2)

## Technical Details

- **Format**: `.scr` file (Windows screensavers are just renamed `.exe` files)
- **API**: Pure Win32 API with no external libraries
- **Language**: C++11, single translation unit
- **Window Style**: `WS_POPUP` with `WS_EX_TOPMOST`, covers virtual screen

## License

Public Domain - Use freely for any purpose.
