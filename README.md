# ghost-saver

A Windows 11 screen saver designed to remove ghosting/burn-in artifacts from monitors by cycling through colors at various brightness levels.

## How It Works

The screensaver follows a repeating cycle:

1. **Color Phase (~60 seconds)**: Cycles through 6 base colors (Red, Green, Blue, Cyan, Magenta, Yellow), each displayed for ~10 seconds. During this phase, the brightness gradually increases from 30% to 100%, helping to "unstuck" stuck pixels and clear phosphor burn-in.

2. **Black Phase (5 minutes)**: Displays a completely black screen, allowing the display to fully rest and reset.

3. **Repeat**: The cycle continues indefinitely until any keyboard or mouse input is detected.

## Building

### Prerequisites
- GCC (MinGW-w64) for Windows
- Make

### Build Commands

```bash
# Build the screensaver
make

# Clean build artifacts
make clean

# Install to Windows System32 directory
make install
```

### Manual Build

```bash
g++ -Wall -Wextra -O2 -o ghost-saver.scr main.cpp -mwindows -lgdi32
```

## Installation

1. Build the project: `make`
2. Copy to Windows directory:
   ```cmd
   copy ghost-saver.scr %windir%\System32\
   ```
3. Set as screensaver:
   - Right-click Desktop → Personalize → Lock screen → Screen saver
   - Select "Ghost Saver" from the dropdown
   - Set desired wait time
   - Click OK

## Command-Line Arguments

| Argument | Description |
|----------|-------------|
| (none) / `/s` | Run the screensaver |
| `/c` | Show configuration dialog |
| `/p <hwnd>` | Show in parent window (for desktop customization) |

## Features

- Full-screen exclusive mode with no taskbar or cursor
- Topmost window that stays above all other content
- Responds to any keyboard/mouse input by exiting immediately
- Simple configuration dialog accessible via right-click → Properties
- No external dependencies beyond the Windows API

## Technical Details

- **Format**: `.scr` file (Windows screensavers are just renamed `.exe` files)
- **API**: Pure Win32 API with no external libraries
- **Language**: C++11 compatible
- **Window Style**: `WS_EX_TOPMOST | WS_EX_NOACTIVATE` to prevent stealing focus

## License

Public Domain - Use freely for any purpose.
