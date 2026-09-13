# ghost-saver

A Windows screen saver designed to remove ghosting/burn-in artifacts from monitors by cycling through solid colors.

Source: https://github.com/dmongrel/ghost-saver

## How It Works

The screensaver follows a repeating cycle:

1. **Color Cycle (45–75 seconds)**: Fades the whole screen through black, red, green, blue, yellow, cyan, purple and white, then back to black. Each fade and each hold lasts a random 3–5 seconds.
2. **Black Phase with Fireworks (30 seconds)**: The screen stays black while a fireworks show plays over it (see below).
3. **Repeat**: A full cycle takes 75–105 seconds and repeats until any keyboard or mouse input is detected.

## Fireworks Mode

During every 30-second black phase, the screen shows fireworks against black:

- **Falling rockets**: Outline circles, squares and five-pointed stars (pentagrams drawn with 5 lines) drop from random points along the top edge. Each is 13–20 pixels across, and squares and stars rotate as they fall. Every rocket takes a random 3.5–5.5 seconds to reach the bottom and leaves a short fading trail. A new one appears every 0.5–1 second, up to 12 at a time.
- **Colors**: Red, green, blue, yellow, cyan, purple or white.
- **Explosions**: Near the bottom edge each rocket bursts into 30–60 particles, near-white with a tint of the rocket's color. They fly out in every direction, climb up to about 300 pixels, arc back down under gravity, and fade out over 1.5–2.5 seconds. Most particles are dots trailing a thin single-pixel streak; the rest are short line sparks.
- **Preview**: The small preview in Screen Saver Settings shows only the color cycle, no fireworks.

Fireworks are always on; there is nothing to configure. The full design is in [`docs/ghost-saver-fireworks-spec.md`](docs/ghost-saver-fireworks-spec.md).

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
g++ -std=c++11 -Wall -Wextra -O2 -municode -DUNICODE -D_UNICODE -o ghost-saver.scr main.cpp ghost-saver.res -mwindows -municode -static -lgdi32 -lshell32
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

- Fireworks show during each 30-second black phase
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
- **Rendering**: Solid GDI fills for the color cycle; fireworks are drawn with plain GDI into an off-screen bitmap and copied to the window each frame (~30 fps)

## License

Public Domain - Use freely for any purpose.
