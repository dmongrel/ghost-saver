# ghost-saver: Build Specification

Status: ready to implement
Target: Windows 10 1703+ / Windows 11, x86-64
Deliverable: `ghost-saver.scr`, a native Win32 screen saver

This document is the single source of truth for building ghost-saver. It was derived from the repository (`main.cpp` with uncommitted changes, `README.md`, `Makefile`) plus later direction from the owner. Where those disagree, section 2 says which one wins. Section 9 lists defects in the current code that the implementation must fix; do not carry them forward.

Keywords: **MUST** is a hard requirement. **SHOULD** is expected unless there is a stated reason not to. **MAY** is optional.

Revisions:

- 2026-09-12: initial spec.
- 2026-09-12: BR-1 (preview shows background then black) added.
- 2026-09-12: color cycle replaced. The sine sweep is gone. The saver now steps through a fixed list of colors with random fade and hold times, then rests on black for 30 s.
- 2026-09-12: purple changed from magenta (255, 0, 255) to (128, 0, 128).
- 2026-09-12: findings from the first local build folded in (section 9b). The Makefile now uses a temp dir inside the project and adds `-municode`, `.rc` files must keep their `#include`, `ColorAt` must be a loop, and a second reference table uses unequal durations.
- 2026-09-12: BR-1 marked resolved (fixed in `73d8ac8`, confirmed by the owner).

---

## 1. Purpose

ghost-saver is a screen saver whose job is to exercise every sub-pixel of every connected display, to help clear image retention ("ghosting") and reduce burn-in. It fills the screen with one solid color at a time. Starting from black, it fades through red, green, blue, yellow, cyan, purple and white and back to black, holding each color for a few seconds. Then it rests on black for 30 seconds and starts over. It draws nothing else: no text, no shapes, no cursor.

## 2. Source of truth

| Source | Describes | Status |
|---|---|---|
| Owner direction, 2026-09-12 (section 3 of this spec) | Fixed color stops, random 3–5 s fades and holds, 30 s black phase | **Authoritative** for visual behavior |
| `main.cpp` (working tree, uncommitted) | Continuous sinusoidal RGB sweep, 10 s period | Superseded. Do not implement. |
| `main.cpp` (commit `633f40b`) | 6 stepped colors × brightness levels, then 5 min black | Superseded. Do not implement. |
| `README.md` | The commit `633f40b` cycle | Stale. Must be rewritten (section 8). |
| `Makefile` | g++ build, `clean`, `install` | Mostly valid; changes in section 7 |

## 3. Visual behavior

### 3.1 Color stops

The saver visits these nine stops in this order, every cycle:

| Stop | Name | R | G | B |
|---:|---|---:|---:|---:|
| 0 | black | 0 | 0 | 0 |
| 1 | red | 255 | 0 | 0 |
| 2 | green | 0 | 255 | 0 |
| 3 | blue | 0 | 0 | 255 |
| 4 | yellow | 255 | 255 | 0 |
| 5 | cyan | 0 | 255 | 255 |
| 6 | purple | 128 | 0 | 128 |
| 7 | white | 255 | 255 | 255 |
| 8 | black | 0 | 0 | 0 |

- **V-1** The stops MUST be exactly the table above, as a `static const` array. "Purple" means (128, 0, 128), the standard web purple. It is not magenta (255, 0, 255). Every sub-pixel is still driven fully on and fully off during a cycle by the other stops.

### 3.2 Cycle structure

One cycle is this sequence of segments, back to back with no gaps:

| # | Segment | Duration | Color during segment |
|---:|---|---|---|
| 1 | fade black → red | T0, random | interpolated |
| 2 | hold red | H0, random | red |
| 3 | fade red → green | T1, random | interpolated |
| 4 | hold green | H1, random | green |
| 5 | fade green → blue | T2, random | interpolated |
| 6 | hold blue | H2, random | blue |
| 7 | fade blue → yellow | T3, random | interpolated |
| 8 | hold yellow | H3, random | yellow |
| 9 | fade yellow → cyan | T4, random | interpolated |
| 10 | hold cyan | H4, random | cyan |
| 11 | fade cyan → purple | T5, random | interpolated |
| 12 | hold purple | H5, random | purple |
| 13 | fade purple → white | T6, random | interpolated |
| 14 | hold white | H6, random | white |
| 15 | fade white → black | T7, random | interpolated |
| 16 | black phase | 30 000 ms, fixed | black |

After segment 16 the next cycle starts at segment 1.

- **V-2** A cycle has 8 fades (`T0`–`T7`), 7 holds (`H0`–`H6`, one for each of red through white) and one black phase. Neither the starting black (stop 0) nor the final black (stop 8) gets a random hold. The black phase is the rest between cycles. `BLACK_PHASE_MS = 30000`, not random.
- **V-3** Every `T` and `H` value is a random whole number of milliseconds, uniformly distributed over **3000–5000 inclusive**, drawn independently of the others. All 15 values for a cycle are drawn when that cycle begins. The next cycle draws a fresh set.
  - Seed once at startup: `srand((unsigned)qpc.QuadPart)`, where `qpc` comes from `QueryPerformanceCounter`.
  - Draw with `3000 + rand() % 2001`. The small modulo bias doesn't matter here.
- **V-4** A cycle therefore lasts between 75 s (15 × 3 s + 30 s) and 105 s (15 × 5 s + 30 s).
- **V-5** When the saver starts (full screen or preview), it begins at the start of segment 1 of a freshly drawn cycle. There is no black phase before the first fade. The first frame is black and starts brightening toward red at once.
- **V-6** The cycle repeats until the saver exits. Nothing else is drawn: no brightness steps, no sine sweep, no other colors.

### 3.3 Data model and color function

```cpp
struct Cycle {
    DWORD fadeMs[8];   // fadeMs[i]: stop i -> stop i+1
    DWORD holdMs[7];   // holdMs[i]: hold on stop i+1 (red..white)
};
```

- **V-13** Provide `static void RollCycle(Cycle& c)`, which fills all 15 durations per V-3, and `static ULONGLONG CycleLength(const Cycle& c)`, which returns the sum of the 15 durations plus `BLACK_PHASE_MS`.
- **V-14** Provide the pure function `static void ColorAt(const Cycle& c, ULONGLONG msIntoCycle, BYTE& r, BYTE& g, BYTE& b)`. It walks the segments in the order of 3.2 and returns the color for that instant. It reads nothing global except the stop table, makes no Win32 calls, and uses no randomness.
  - It MUST be a single loop over `i = 0..7` that uses `STOPS[i]`, `STOPS[i + 1]`, `fadeMs[i]` and (for `i < 7`) `holdMs[i]`. Do not write out each of the 16 segments by hand. The first build did, and copy-paste errors put the wrong durations into four segments (B-4).
  - Segments are half-open intervals `[start, end)`. At the exact end of a fade, the color belongs to the following hold.
  - In a fade from color `A` to color `B` of duration `D`, at `t` ms into the fade, compute each channel with integer math only, so results are exact and repeatable:
    `v = (A * (D - t) + B * t + D / 2) / D`
  - If `msIntoCycle >= CycleLength(c)`, return black. Callers never pass such a value (V-15), but the function must not read out of bounds.
- **V-15** Timekeeping. Keep file-scope state: the current `Cycle` and `cycleStartMs` (a `GetTickCount64()` value). At startup, roll a cycle and set `cycleStartMs` to now. Before each paint, read `now`. While `now - cycleStartMs >= CycleLength(current)`, add that length to `cycleStartMs` and roll a new cycle. Then call `ColorAt(current, now - cycleStartMs, ...)`. The loop lets the saver catch up after a long stall, such as the machine waking from sleep.
- **V-16** The animation MUST NOT reset, jump, or re-roll when the window is repainted for other reasons. Only the rule in V-15 advances the cycle.
- **V-17** No floating-point math and no `<cmath>` are needed. Do not use `M_PI`.

### 3.4 Reference values

`ColorAt` MUST pass **both** tables below exactly. The first uses equal durations, which is easy to reason about. It can't tell a fade duration from a hold duration, so it passes code that mixes them up. The second uses a different duration for every segment and catches that mistake.

#### 3.4.1 Equal durations

With every fade and hold set to exactly 4000 ms, the cycle length is 90 000 ms, and `ColorAt` MUST return exactly:

| msIntoCycle | Segment | R | G | B |
|---:|---|---:|---:|---:|
| 0 | fade black → red, start | 0 | 0 | 0 |
| 2000 | fade black → red, middle | 128 | 0 | 0 |
| 4000 | hold red | 255 | 0 | 0 |
| 7999 | hold red, last ms | 255 | 0 | 0 |
| 10000 | fade red → green, middle | 128 | 128 | 0 |
| 12000 | hold green | 0 | 255 | 0 |
| 18000 | fade green → blue, middle | 0 | 128 | 128 |
| 20000 | hold blue | 0 | 0 | 255 |
| 26000 | fade blue → yellow, middle | 128 | 128 | 128 |
| 28000 | hold yellow | 255 | 255 | 0 |
| 34000 | fade yellow → cyan, middle | 128 | 255 | 128 |
| 36000 | hold cyan | 0 | 255 | 255 |
| 42000 | fade cyan → purple, middle | 64 | 128 | 192 |
| 44000 | hold purple | 128 | 0 | 128 |
| 50000 | fade purple → white, middle | 192 | 128 | 192 |
| 52000 | hold white | 255 | 255 | 255 |
| 58000 | fade white → black, middle | 128 | 128 | 128 |
| 60000 | black phase, start | 0 | 0 | 0 |
| 89999 | black phase, last ms | 0 | 0 | 0 |

#### 3.4.2 Unequal durations

Set `fadeMs[i] = 3000 + 100 * i` (3000, 3100, … 3700) and `holdMs[i] = 5000 - 100 * i` (5000, 4900, … 4400). The cycle length is 89 700 ms, and `ColorAt` MUST return exactly:

| msIntoCycle | Segment | R | G | B |
|---:|---|---:|---:|---:|
| 1500 | fade black → red, middle | 128 | 0 | 0 |
| 3000 | hold red, first ms | 255 | 0 | 0 |
| 7999 | hold red, last ms | 255 | 0 | 0 |
| 9550 | fade red → green, middle | 128 | 128 | 0 |
| 11100 | hold green, first ms | 0 | 255 | 0 |
| 15999 | hold green, last ms | 0 | 255 | 0 |
| 17600 | fade green → blue, middle | 0 | 128 | 128 |
| 19200 | hold blue, first ms | 0 | 0 | 255 |
| 23999 | hold blue, last ms | 0 | 0 | 255 |
| 25650 | fade blue → yellow, middle | 128 | 128 | 128 |
| 27300 | hold yellow, first ms | 255 | 255 | 0 |
| 31999 | hold yellow, last ms | 255 | 255 | 0 |
| 33700 | fade yellow → cyan, middle | 128 | 255 | 128 |
| 35400 | hold cyan, first ms | 0 | 255 | 255 |
| 39999 | hold cyan, last ms | 0 | 255 | 255 |
| 41750 | fade cyan → purple, middle | 64 | 128 | 192 |
| 43500 | hold purple, first ms | 128 | 0 | 128 |
| 47999 | hold purple, last ms | 128 | 0 | 128 |
| 49800 | fade purple → white, middle | 192 | 128 | 192 |
| 51600 | hold white, first ms | 255 | 255 | 255 |
| 55999 | hold white, last ms | 255 | 255 | 255 |
| 57850 | fade white → black, middle | 128 | 128 | 128 |
| 59700 | black phase, start | 0 | 0 | 0 |
| 89699 | black phase, last ms | 0 | 0 | 0 |

### 3.5 Frame pacing

- **V-7** Drive the animation with `SetTimer(hwnd, TIMER_ID, 33, nullptr)` (about 30 fps). On each `WM_TIMER`, call `InvalidateRect(hwnd, nullptr, FALSE)`. Keep the timer running through holds and the black phase. Kill it in `WM_DESTROY`.
- **V-8** At 33 ms per frame, the shortest fade (3 s) is drawn in about 90 steps, which looks smooth, and every hold stays on screen for at least 90 frames. Do not raise the interval above 50 ms.
- **V-9** `WM_PAINT` MUST fill the entire client rect with the current color using `BeginPaint`/`FillRect`/`EndPaint`. Either create and delete a brush per paint, or use the stock `DC_BRUSH` with `SetDCBrushColor`. The second option is preferred because it allocates nothing. No GDI object may leak: running for an hour must leave the process GDI object count flat (check in Task Manager, "GDI objects" column).
- **V-10** `WM_ERASEBKGND` MUST return 1 without erasing, to prevent flicker. The window class `hbrBackground` is `nullptr`.
- **V-11** Every step of the color cycle MUST be drawn into the window as a solid fill of the whole client area. In full-screen mode that is the full-screen window, covering all monitors. Each timer tick produces one new fill. A frame that shows the desktop, an unpainted window, or any color other than the one V-15 gives for that instant is a failure. Black is correct only where section 3.2 calls for it. See BR-1.
- **V-12** The first frame the user sees MUST already be the cycle color. Create the window **without** `WS_VISIBLE`, set up the cycle state (V-15) and start the timer, then call `ShowWindow(hwnd, SW_SHOW)` followed immediately by `UpdateWindow(hwnd)`. `UpdateWindow` forces a synchronous `WM_PAINT`, so no unpainted window is ever composited.

## 4. Command-line modes

Windows launches a `.scr` with one of the arguments below. Parsing MUST be case-insensitive and accept both `/` and `-` as the prefix.

| Invocation | Mode | Behavior |
|---|---|---|
| `/s` | Full screen | Section 5 |
| *(no arguments)* | Full screen | Same as `/s`. Keeps the current documented behavior, which makes testing from a terminal easy. |
| `/p <hwnd>` | Preview | Section 6 |
| `/c` or `/c:<hwnd>` | Configure | Section 6.3 |
| anything else (including `/a`) | — | Exit with code 0, show nothing |

- **A-1** Use `CommandLineToArgvW(GetCommandLineW(), &argc)` and free the result with `LocalFree`.
- **A-2** For `/c:<hwnd>`, the handle follows the colon in the same token. For `/p <hwnd>`, the handle is the next token. Also accept `/p:<hwnd>`. Parse the handle as an unsigned decimal integer (`wcstoull`) and cast through `UINT_PTR` to `HWND`.
- **A-3** `/p` without a valid handle (missing, zero, or `IsWindow()` false) MUST exit with code 0.
- **A-4** The process MUST call `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` before creating any window. The manifest in section 7.2 is the preferred way and makes this call unnecessary. If the implementation uses the API call instead, it must fall back to `SetProcessDPIAware()` when the call fails.

## 5. Full-screen mode

### 5.1 Window

- **F-1** Register one window class, `L"GhostSaverClass"`, with `hCursor = nullptr` and `hbrBackground = nullptr`. Style `CS_HREDRAW | CS_VREDRAW` is fine.
- **F-2** Create one window that covers the entire **virtual screen**, meaning all monitors:
  - position `(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN))`
  - size `(GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN))`
  - style `WS_POPUP` (made visible later, per V-12), extended style `WS_EX_TOPMOST`

  The current code uses `SM_CXSCREEN`/`SM_CYSCREEN`, which covers only the primary monitor. A burn-in tool that skips secondary monitors is broken.
- **F-3** The window MUST NOT use `WS_EX_NOACTIVATE`. It must become the foreground window (call `SetForegroundWindow` after creation) so that it receives keyboard input. With `WS_EX_NOACTIVATE`, keystrokes go to whatever app had focus and the saver never sees them.
- **F-4** Set up the cycle state (V-15) right after the window is created and before it is shown (V-12).

### 5.2 Cursor

- **F-5** Hide the cursor by handling `WM_SETCURSOR`: call `SetCursor(nullptr)` and return `TRUE`. Do not use `ShowCursor`. Its counter is easy to unbalance, and the current code calls `ShowCursor(TRUE)` twice on exit.

### 5.3 Exit conditions

The saver MUST call `DestroyWindow(hwnd)` when any of these arrive:

- **F-6** `WM_KEYDOWN`, `WM_SYSKEYDOWN`
- **F-7** `WM_LBUTTONDOWN`, `WM_RBUTTONDOWN`, `WM_MBUTTONDOWN`, `WM_XBUTTONDOWN`, `WM_MOUSEWHEEL`, `WM_MOUSEHWHEEL`
- **F-8** `WM_MOUSEMOVE` where the cursor has moved **more than 4 pixels** on either axis from the position recorded on the first `WM_MOUSEMOVE` received. Windows sends a synthetic `WM_MOUSEMOVE` when the window appears even if the mouse is still. The current code exits on any `WM_MOUSEMOVE`, so it can close the moment it opens.
  - Get the position with `GET_X_LPARAM` / `GET_Y_LPARAM` from `<windowsx.h>`. Do not use `LOWORD`/`HIWORD`, because coordinates can be negative on multi-monitor setups.
- **F-9** `WM_ACTIVATEAPP` with `wParam == FALSE`, meaning another app took the foreground.
- **F-10** `WM_CLOSE`, passed to `DefWindowProc`, which destroys the window.

The saver MUST also:

- **F-11** Handle `WM_SYSCOMMAND` with `(wParam & 0xFFF0) == SC_SCREENSAVE` by returning 0, so Windows does not start a second copy on top of this one. Pass all other `WM_SYSCOMMAND` values to `DefWindowProc`.
- **F-12** In `WM_DESTROY`: kill the timer, then `PostQuitMessage(0)`.
- **F-13** Exit the message loop, unregister the class, and return 0.

## 6. Preview and configure modes

### 6.1 Preview window (`/p <hwnd>`)

This is the small monitor image in the Screen Saver Settings dialog. The current code opens the config dialog here instead, which is wrong.

- **P-1** Create a child window with style `WS_CHILD` (shown per V-12), no extended style, parented to the given HWND and sized to `GetClientRect(parent)`.
- **P-2** Render exactly as in section 3, using the same cycle state logic (V-15), `ColorAt`, timer and paint code. The preview rolls its own cycle and starts at segment 1 (V-5). Share the window procedure with full-screen mode and branch on a file-scope `static bool g_preview`.
- **P-3** In preview mode, input MUST NOT close the window, the cursor MUST NOT be hidden, and `WM_ACTIVATEAPP` MUST be ignored. Only `WM_DESTROY` ends it; Windows destroys the child when it closes the preview or the settings dialog.
- **P-4** No `SetForegroundWindow` and no `WS_EX_TOPMOST` in preview.

### 6.2 Shared window procedure summary

| Message | Full screen | Preview |
|---|---|---|
| `WM_TIMER` | invalidate | invalidate |
| `WM_PAINT` | fill color | fill color |
| `WM_ERASEBKGND` | return 1 | return 1 |
| `WM_SETCURSOR` | hide, return TRUE | `DefWindowProc` |
| keyboard / mouse buttons / wheel | destroy | `DefWindowProc` |
| `WM_MOUSEMOVE` | destroy past 4 px threshold | `DefWindowProc` |
| `WM_ACTIVATEAPP` (FALSE) | destroy | `DefWindowProc` |
| `WM_SYSCOMMAND` `SC_SCREENSAVE` | return 0 | `DefWindowProc` |
| `WM_DESTROY` | kill timer, `PostQuitMessage` | kill timer, `PostQuitMessage` |

### 6.3 Configure (`/c`, `/c:<hwnd>`)

ghost-saver has no settings. The current code calls `DialogBoxParamW` with a `CONFIG_DIALOG` resource that does not exist, so nothing appears.

- **C-1** Show `MessageBoxW(owner, text, L"Ghost Saver", MB_OK | MB_ICONINFORMATION)`, where `owner` is the handle from `/c:<hwnd>` if it is valid (`IsWindow`), otherwise `nullptr`.
- **C-2** Text, verbatim:
  `Ghost Saver fades the whole screen through black, red, green, blue, yellow, cyan, purple and white, then rests on black, to help clear image retention and burn-in.\n\nThere are no settings to configure.`
- **C-3** Remove `ConfigDlgProc` and every reference to a dialog resource.

## 7. Build

### 7.1 Files

The finished repository contains exactly these source files. Nothing else is added.

| File | Purpose |
|---|---|
| `main.cpp` | All code. Single translation unit. |
| `ghost-saver.rc` | String table + manifest reference |
| `ghost-saver.manifest` | DPI awareness + OS compatibility |
| `Makefile` | Build, clean, install |
| `README.md` | User-facing docs (section 8) |
| `docs/ghost-saver-spec.md` | This file. Do not edit it as part of the build. |

Build outputs that are never committed: `ghost-saver.scr`, `ghost-saver.res`, and the `build/` directory (holds compiler temp files, section 7.4). Don't create any other directories, such as `tmpdir/`, in the project root.

### 7.2 Resources

`ghost-saver.rc`:

```rc
#include <windows.h>

STRINGTABLE
BEGIN
    1 "Ghost Saver"
END

CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST "ghost-saver.manifest"
```

String ID 1 is the name Windows shows in the Screen Saver Settings dropdown. Without it the list shows the file name, `ghost-saver`.

- **R-1** The `#include <windows.h>` line MUST stay. It defines `CREATEPROCESS_MANIFEST_RESOURCE_ID` (1) and `RT_MANIFEST` (24). Without it, windres doesn't fail. It silently stores the manifest as a custom resource named `"CREATEPROCESS_MANIFEST_RESOURCE_ID"` of type `"RT_MANIFEST"`, which Windows ignores (B-3).
- **R-2** Run windres with its default preprocessor. Do not pass `--preprocessor="cat"` or any other `--preprocessor` override. That turns off `#include`, and the manifest is lost as in R-1.
- **R-3** Check the result with `windres -J coff -i ghost-saver.res -O rc`. The output MUST contain a `STRINGTABLE` and a resource whose type is the number `24`, not the string `"RT_MANIFEST"`.

`ghost-saver.manifest`:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
    </windowsSettings>
  </application>
</assembly>
```

With the manifest in place, requirement A-4 is met and `main.cpp` needs no DPI API calls.

### 7.3 Toolchain and flags

- MinGW-w64, **x86_64** target (MSYS2 `mingw64` or `ucrt64`). A 32-bit build copied into `System32` gets silently redirected to `SysWOW64`.
- Language: C++11 (`-std=c++11`). No exceptions or RTTI are needed; nothing from the C++ standard library beyond `<cstdlib>` (`srand`/`rand`).
- Entry point: `int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)`. MinGW only links `wWinMain` when `-municode` is passed. Without it the link fails with ``undefined reference to `WinMain'`` (B-1).
- Compile: `-std=c++11 -Wall -Wextra -O2 -municode -DUNICODE -D_UNICODE`
- Link: `-mwindows -municode -static -lgdi32 -lshell32`
- Temp files: gcc, `collect2` and `ld` all write temp files to the directory named by `TMP`/`TEMP`/`TMPDIR`. If that directory is missing or not writable, the build fails with `Cannot create temporary file in <dir>: No such file or directory` (B-2). `-pipe` does not avoid it; the link step still needs a temp file. The Makefile MUST therefore point all three variables at `build/tmp` inside the project and create it (section 7.4). Do not rely on, or try to repair, the shell's own temp settings.
- `-static` keeps the `.scr` from depending on `libstdc++-6.dll` / `libgcc_s_*.dll`, which do not exist on a normal machine. The saver runs from `System32`, where no MinGW DLLs are on the path.
- The build MUST finish with **zero warnings**. Remove unused declarations such as the current `NUM_CHANNELS` and `g_hwnd` if they end up unused.
- Win32 calls that have `A`/`W` variants MUST use the `W` form or the `UNICODE`-mapped macro consistently.

### 7.4 Makefile

```make
CXX      = g++
WINDRES  = windres
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -municode -DUNICODE -D_UNICODE
LDFLAGS  = -mwindows -municode -static
LDLIBS   = -lgdi32 -lshell32
TARGET   = ghost-saver.scr
RES      = ghost-saver.res
BUILDTMP = build/tmp

# Keep compiler/linker temp files inside the project; don't depend on TMP/TEMP.
export TMP    := $(abspath $(BUILDTMP))
export TEMP   := $(TMP)
export TMPDIR := $(TMP)

.PHONY: all clean install

all: $(TARGET)

$(BUILDTMP):
	mkdir -p $@

$(RES): ghost-saver.rc ghost-saver.manifest | $(BUILDTMP)
	$(WINDRES) ghost-saver.rc -O coff -o $@

$(TARGET): main.cpp $(RES) | $(BUILDTMP)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(RES) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TARGET) $(RES) build

install: $(TARGET)
	cp $(TARGET) "$(SYSTEMROOT)/System32/"
```

Notes:

- Recipe lines are indented with a tab, not spaces.
- `| $(BUILDTMP)` is an order-only prerequisite: the directory is created before windres and g++ run, but its timestamp never triggers a rebuild.
- The recipes use `mkdir -p` and `rm -rf`, so this Makefile needs a POSIX shell. `mingw32-make` uses `sh` when one is on `PATH` (for example `C:\msys64\usr\bin`); `make` from MSYS2 always has it. Both were verified to build successfully with `TMP`/`TEMP` pointing at a directory that doesn't exist.
- Either toolchain command works: `make` (MSYS2 `usr/bin`) or `mingw32-make` (MSYS2 `ucrt64/bin`), as long as `ucrt64/bin` (or `mingw64/bin`) is first on `PATH` so `g++` and `windres` resolve.
- `make` may print `using default temporary directory ...` when `TMPDIR` is broken in the calling shell. That message comes from make itself and is harmless.
- The original `install` recipe, `cp $(TARGET) "$(windir)\System32\"`, is broken: the trailing `\"` escapes the closing quote. `install` needs an elevated (Administrator) shell.

`.gitignore` MUST include `*.res` and `build/`.

## 8. README rewrite

Replace the "How It Works" section, which describes the old stepped-color and black-phase cycle, with a description of section 3: fades through black, red, green, blue, yellow, cyan, purple, white and back to black, with each fade and each hold lasting a random 3–5 seconds, then 30 seconds of black before the next cycle. Also:

- Update the build commands and flags to match section 7.
- Command-line table: add `/c:<hwnd>` and `/p <hwnd>`, and describe preview correctly (it renders the animation into the settings dialog's preview, not the config dialog).
- Features: replace "`WS_EX_TOPMOST | WS_EX_NOACTIVATE` to prevent stealing focus" with "covers all monitors; topmost; hides cursor; exits on key, click, wheel, or mouse movement".
- Installation: build 64-bit, copy to `%SystemRoot%\System32` from an elevated prompt, *or* right-click `ghost-saver.scr` → **Install**.
- Keep the License section as is.

## 9. Defects in the current `main.cpp` that must not survive

| # | Defect | Effect | Fixed by |
|---|---|---|---|
| D-1 | No timer or `InvalidateRect`; `WM_PAINT` fires only when Windows asks | Color is painted once and never animates | V-7 |
| D-2 | `WS_EX_NOACTIVATE` on the saver window | Keyboard never reaches the saver, so keys don't dismiss it | F-3 |
| D-3 | Exit on any `WM_MOUSEMOVE` | Saver can close the instant it appears | F-8 |
| D-4 | `SM_CXSCREEN`/`SM_CYSCREEN` sizing | Secondary monitors are never exercised | F-2 |
| D-5 | `/p` opens the config dialog | Settings preview shows nothing / a stray dialog | P-1..P-4 |
| D-6 | `DialogBoxParamW` with nonexistent `CONFIG_DIALOG` resource | `/c` silently does nothing | C-1..C-3 |
| D-7 | `GetColor` is a continuous sine sweep (`128 + 127·sin`, truncated) | Wrong pattern: no color stops, holds or black phase. Channels never reach 0. | Section 3; delete `GetColor` |
| D-8 | `M_PI` without `_USE_MATH_DEFINES` | Fails to compile under strict flags | V-17 |
| D-9 | `ShowCursor(TRUE)` called in `WndProc` and again after the loop | Unbalanced cursor counter | F-5 |
| D-10 | `/c:<hwnd>` form not parsed (only exact `/c`) | Right-click → Configure from Settings may not match | A-2 |
| D-11 | No string resource ID 1 | Dropdown shows `ghost-saver`, not `Ghost Saver` | 7.2 |
| D-12 | Makefile `install` quoting | `make install` fails | 7.4 |
| D-13 | Window created `WS_VISIBLE` with no background brush and no forced first paint | An unpainted black window can be composited before the first fill | V-12 |

## 9a. Bug reports

Field reports against the installed build. Each one is a requirement: the implementation is not done until its acceptance checks pass.

### BR-1: Preview shows background then black, no color cycling

**Status: RESOLVED** (2026-09-12). Fixed in commit `73d8ac8` and installed to `System32`. The owner confirmed that preview works. The acceptance checks below stay as regression tests for future builds.

**Reported behavior.** With ghost-saver installed, clicking **Preview** in Screen Saver Settings makes the screen "flip": the desktop background shows briefly, then the screen goes black. No color cycling happens.

**Expected behavior.** Clicking **Preview** shows the full-screen saver. From the first visible frame, the whole screen (every monitor) is a solid fill of the current cycle color, and the fill follows the cycle of section 3 (fades, holds, black phase) until the user moves the mouse or presses a key. Every step of the cycle is drawn into the full-screen window as a fill; nothing else is ever visible.

**Black is not automatically a failure.** Under the section 3 cycle the first frame is black (V-5), but it starts brightening toward red right away and is fully red within 3–5 s. The black phase (30 s) is also correct. The bug is a black screen that *stays* black, or black at any moment section 3.2 doesn't call for it.

**Clarification.** The **Preview** *button* launches the saver with `/s`, i.e. full-screen mode (section 5). It is different from the small monitor image in the dialog, which uses `/p <hwnd>` (section 6.1). Both MUST animate; this report concerns the button.

**Causes in the current code**, all of which the implementation must remove:

- No timer drives repaints (D-1), so at most one frame is ever drawn and the cycle never advances.
- The window is shown before anything has been painted into it (D-13), so the compositor can present it as black.
- Any `WM_MOUSEMOVE` closes the saver (D-3). The click on **Preview** leaves the mouse in motion, which can close the saver at once and show the background. Windows then reopens the settings dialog.
- `WS_EX_NOACTIVATE` (D-2) keeps the window from becoming foreground, so Windows may treat the preview as ended.

**Requirements met by:** V-5, V-7, V-9, V-11, V-12, V-15, F-3, F-8.

**Acceptance:**

- BR-1.a Install the build, open Screen Saver Settings, select Ghost Saver, click **Preview**, then take your hand off the mouse. The desktop is never visible. The screen is covered by the saver at once, and within 2 seconds it is visibly turning red.
- BR-1.b Within 75 seconds the screen shows red, green, blue, yellow, cyan, purple and white in that order, fading between them and holding each one. It then fades to black, stays black for about 30 seconds, and starts again with a fade to red.
- BR-1.c With two monitors, both show the same changing fill.
- BR-1.d A key press or real mouse movement ends the preview and returns to the Screen Saver Settings dialog.
- BR-1.e The small preview monitor inside the dialog animates the same cycle before and after clicking **Preview**.

## 9b. Findings from the first local build (2026-09-12)

The first implementation attempt was compiled and reviewed against this spec. Each finding is now covered by a requirement; the list exists so the same mistakes aren't repeated.

| # | Finding | Symptom | Now covered by |
|---|---|---|---|
| B-1 | `wWinMain` entry point without `-municode` | Link fails: ``undefined reference to `WinMain'`` | 7.3, 7.4 |
| B-2 | Build depends on the shell's `TMP`/`TEMP`; `tmpdir/` left in the project root | `Cannot create temporary file in <dir>` when those point at a missing or unwritable directory | 7.1, 7.3, 7.4 |
| B-3 | `#include <windows.h>` removed from `ghost-saver.rc`, windres run with `--preprocessor="cat"` | Builds cleanly, but the manifest is stored under a string type Windows ignores, so no DPI awareness | R-1, R-2, R-3 |
| B-4 | `ColorAt` written out segment by segment; after the yellow, cyan, purple and white holds it subtracts `fadeMs[i]` instead of `holdMs[i]` | Passes table 3.4.1, but with unequal durations the cycle drifts: the end of the cyan hold shows purple, purple shows white, white shows black | V-14 loop rule, table 3.4.2 |
| B-5 | Any `WM_MOUSEMOVE` closes the saver | Saver can close as soon as it opens (a cause of BR-1) | F-8 |
| B-6 | Preview child window hard-coded to 320×240 and shown before its first paint | Preview doesn't fill the settings dialog's monitor image | P-1, V-12 |
| B-7 | `WM_XBUTTONDOWN` and `WM_MOUSEHWHEEL` not handled | Those inputs don't close the saver | F-7 |
| B-8 | `WM_SYSCOMMAND` compared to `SC_SCREENSAVE` without masking `wParam & 0xFFF0` | Check can miss | F-11 |
| B-9 | `main.cpp` staged as deleted in git with an untracked copy alongside (`git rm --cached`) | Committing would delete `main.cpp` from the repo | Don't run `git rm`; leave git index changes to the owner |

## 10. Out of scope

Do not add any of these. They are not part of the current design.

- Settings, registry storage, or a real configuration dialog (including settings for colors or durations)
- Any pattern other than section 3: brightness steps, sine sweeps, extra colors, gradients, noise, moving bars
- Easing curves; fades are linear (V-14)
- Per-monitor windows or per-monitor colors; one window over the virtual screen is enough
- Password / `/a` handling
- OpenGL, Direct2D, DirectX, or any library beyond the Win32 API
- An installer, code signing, or a 32-bit build
- Logging or telemetry

## 11. Acceptance tests

The implementation is done when every check passes. Run them in order.

**Build**

1. `make clean && make` in an MSYS2 x86_64 shell finishes with zero warnings and produces `ghost-saver.scr`.
1a. The build also succeeds when the calling shell's temp settings are broken: `make clean && TMP='C:\nope' TEMP='C:\nope' TMPDIR=/nope make`. No `tmpdir/` or other stray directory appears in the project root; only `build/` is created.
2. `objdump -p ghost-saver.scr | grep "DLL Name"` lists only Windows system DLLs (`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, and `msvcrt` or `api-ms-win-crt-*`). No `libstdc++`, `libgcc`, or `libwinpthread`.
3. `windres` embeds the string table: in Screen Saver Settings, after install, the dropdown entry reads **Ghost Saver**.
3a. `windres -J coff -i ghost-saver.res -O rc` shows a `STRINGTABLE` and a resource of type `24` (R-3). A type shown as `"RT_MANIFEST"` in quotes is a failure.

**Color math**

4. Checked with a throwaway harness that includes the functions (don't commit it):
   - `ColorAt` matches every row of table 3.4.1 (equal durations) **and** table 3.4.2 (unequal durations) **exactly**.
   - Over 10 000 calls to `RollCycle`, every duration is within 3000–5000, both 3000 and 5000 show up at least once, and `CycleLength` is always within 75 000–105 000.
   - Two cycles rolled in a row have different durations.

**Full screen (`ghost-saver.scr /s`)**

5. The screen follows section 3: starts black, fades to red, then green, blue, yellow, cyan, purple, white, black. Fades are smooth, holds are steady, and nothing flickers or jumps.
5a. With a stopwatch over two full cycles: every fade and every hold lasts roughly 3–5 s, the black phase lasts about 30 s, the cycle restarts with a fade to red, and the two cycles' timings are not identical.
6. With two or more monitors attached, every monitor shows the color, including one placed to the left of or above the primary (negative coordinates).
7. The cursor is not visible.
8. The saver keeps running for 30 seconds while the mouse sits untouched.
9. Each of these, tried separately in a fresh launch, closes it immediately: any letter key, Esc, Alt, left click, right click, middle click, scroll wheel, moving the mouse a few centimeters.
10. Nudging the mouse by 1–2 pixels does not close it.
11. Alt+Tab or Win key closes it (it loses foreground).
12. After exit, the cursor is visible again and focus returns to the previous app.
13. Left running for 60 minutes, Task Manager shows a stable GDI object count and memory use.

**Preview**

14. Open Screen Saver Settings and select Ghost Saver. The small preview monitor shows the section 3 color cycle.
15. Switching to another saver in the dropdown, or closing the dialog, ends the `ghost-saver.scr` process (check Task Manager).

**Configure**

16. Clicking **Settings…** in Screen Saver Settings shows the message box from C-2, owned by the settings dialog. OK closes it.
17. `ghost-saver.scr /c` from a terminal shows the same message box with no owner.

**Arguments**

18. `ghost-saver.scr` with no arguments runs full screen. `ghost-saver.scr /S` and `-s` do too.
19. `ghost-saver.scr /p` (no handle) and `ghost-saver.scr /x` exit immediately with code 0 and show nothing.

**Real-world**

20. Set Ghost Saver with a 1-minute wait. Leave the machine idle. It starts on its own, animates, and dismisses on a key press.

**Bug reports**

21. BR-1.a through BR-1.e (section 9a) all pass.
