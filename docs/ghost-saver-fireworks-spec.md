# ghost-saver: Fireworks Mode Specification

Status: approved by the owner (2026-09-13); implemented in commit `a6a8d50`
Target: Windows 10 1703+ / Windows 11, x86-64
Deliverable: `ghost-saver.scr`, a native Win32 screen saver with fireworks mode
Extends: `docs/ghost-saver-spec.md`. Everything there still applies unless this document says otherwise.

Revisions:

- 2026-09-12: initial draft.
- 2026-09-13: corrected after the first implementation.
  - Dependencies: `AlphaBlend` is not GDI+ and isn't needed. No new libraries.
  - Positions and velocities are floats, and frame time is measured rather than assumed.
  - Filled in rotation speed, trail length, particle size, reset between black phases, and the drawing method.
  - Added section 9 on the bugs the first build had.
- 2026-09-13: owner tuning. Explosive force doubled (160–500 px/s), dots get single-pixel trails, fall time widened to 3.5–5.5 s.
- 2026-09-13: owner tuning. Rockets 50% larger with random ±20% scale, drawn as 1 px outlines; stars are 5-line pentagrams. Explosions unchanged.
- 2026-09-13: approved by the owner.

---

## 1. Purpose

Fireworks mode replaces the plain black rest period (the 30-second black phase between color cycles) with a fireworks display. The display still rests on black for 30 seconds, but instead of a solid black fill it shows falling shapes that explode into particles when they reach the bottom of the screen.

This mode is always active during the black phase; there is no toggle.

---

## 2. Integration with existing cycle

### 2.1 When it runs

Fireworks mode runs only during segment 16 of the cycle (the black phase, `BLACK_PHASE_MS = 30000`). During all other segments (fades and holds) the saver renders exactly as before: solid fills of the current color.

The saver is in the black phase when `msIntoCycle >= CycleLength(cycle) - BLACK_PHASE_MS`.

### 2.2 When it does not run

- Preview mode (`/p <hwnd>`): fireworks are disabled; preview shows only solid-color fills, including a solid black fill during the black phase.
- Configure mode (`/c`): no visual change.
- Unknown arguments: no visual change.

### 2.3 Start and end of each black phase

- **FW-1** When a frame is painted in the black phase and the previous frame was not, reset all fireworks state first (section 4.3). Rockets and particles from the previous black phase MUST NOT reappear.
- **FW-2** When the black phase ends, the next frame is the normal solid fill (the start of the fade to red). Rockets still falling are simply dropped; they are cleared at the next reset.

---

## 3. Visual behavior

### 3.1 Falling shapes (rockets)

- Shapes start at a random horizontal position along the **top edge** of the screen (y = 0).
- Each shape has a **random color** from the fireworks palette (section 3.4).
- Each shape falls at a **constant speed**, taking a random **3.5–5.5 seconds** to reach the explosion point (section 4.4).
- Shapes are circles, squares, or five-pointed stars, chosen at random.
- **FW-11** Shapes are drawn as **1-pixel outlines in the shape's color, never filled.** Filled shapes with a trail read as tadpoles, not fireworks.
  - Circle: an outline circle.
  - Square: an outline square.
  - Star: a **pentagram**, 5 tips joined by **5 straight lines** (tip 0 → 2 → 4 → 1 → 3 → 0, with tips spaced 72° apart and tip 0 pointing up before rotation). Not a 10-point filled polygon.
- Size is **16.5 pixels** across (50% larger than the original 10–12 px, average 11 px), multiplied by a random scale of **0.8–1.2** per shape, so shapes are **13.2–19.8 pixels** across. Size is the diameter of a circle, the side of a square, and the diameter of the circle through a star's 5 tips.
- Squares and stars rotate while falling at a random **0.5–2.0 radians per second**, clockwise or counter-clockwise at random. Circles don't rotate.
- Up to **12 shapes** on screen at once. When all 12 are falling, a spawn is skipped.
- A new shape spawns every **0.5–1.0 seconds** (random delay, re-drawn after each spawn).

### 3.2 Explosion particles

When a shape reaches the explosion point it disappears and spawns **particles**:

- Each explosion spawns a random **30–60 particles** (fewer only if the 2000-particle pool is full).
- About one in three particles is a short line; the rest are dots.
  - Dots are filled circles with a random radius of **1–2 pixels**. Each dot drags a single-pixel trail (section 3.3).
  - Lines are a random **2–5 pixels** long and trail behind the particle, pointing opposite its current direction of travel.
- Particles launch in a uniformly random direction (0–360 degrees) from the explosion point.
- Initial speed is a random **160–500 pixels per second**. The fastest particle launched straight up rises about 312 pixels before gravity turns it back down.
- **Gravity** pulls particles down at **400 pixels per second squared**.
- Particle color is **85% white + 15% the shape's color**, per channel. A red shape gives particles of (255, 216, 216).
- Particles fade linearly from fully opaque to invisible over a random lifetime of **1.5–2.5 seconds**, then disappear.
- Particles are also removed once they fall more than 10 pixels below the bottom edge. Anything outside the window is clipped by GDI as usual.

### 3.3 Trail effect

Each falling shape leaves a short **trail**:

- The trail joins the shape's last **9 positions** (one per frame, current position included) with **8** single-pixel line segments, about 0.27 seconds of travel at 30 fps.
- The newest segment is drawn at 100% opacity, the oldest at 10%, with linear steps in between.
- A freshly spawned shape's trail grows to full length over its first 9 frames.

Each explosion **dot** (not the line-shaped particles, which already look like streaks) leaves its own trail:

- The trail joins the dot's last **6 positions** with **5** single-pixel line segments, about 0.17 seconds of travel. Because particles arc under gravity, the trail curves with them.
- The newest segment is drawn at the dot's current opacity, the oldest at 10% of it, with linear steps in between. The trail fades out together with its dot.
- A new dot's trail grows to full length over its first 6 frames.

### 3.4 Color palette for fireworks

Fireworks colors are the saver's color stops without black:

| Color | R | G | B |
|---|---:|---:|---:|
| red | 255 | 0 | 0 |
| green | 0 | 255 | 0 |
| blue | 0 | 0 | 255 |
| yellow | 255 | 255 | 0 |
| cyan | 0 | 255 | 255 |
| purple | 128 | 0 | 128 |
| white | 255 | 255 | 255 |

Black is **not** used; it would be invisible against the background.

### 3.5 Opacity

The background is always black, so "opacity" is implemented by scaling the color toward black: a color `(r, g, b)` at opacity `a` is drawn as `(r·a, g·a, b·a)`, rounded to nearest. No alpha channel, `AlphaBlend`, or layered window is used.

---

## 4. Animation loop

### 4.1 Timing

- The animation uses the existing `WM_TIMER` with a **33 ms interval** (about 30 fps), shared with the color cycle. No additional timer.
- Fireworks are updated and drawn inside the existing `WM_PAINT` handler when the black phase is active.
- **FW-3** Movement MUST use measured frame time, not an assumed 33 ms: `dt = (now - g_lastUpdateMs) / 1000.0f`, capped at **0.1 s** so a stall (sleep, a slow frame) can't throw everything across the screen.

### 4.2 State

```cpp
enum ShapeType { SHAPE_CIRCLE, SHAPE_SQUARE, SHAPE_STAR };

static const int   MAX_ROCKETS        = 12;
static const int   MAX_PARTICLES      = 2000;
static const int   TRAIL_POINTS       = 9;      // 8 segments
static const int   DOT_TRAIL_POINTS   = 6;      // 5 segments behind each dot
static const int   EXPLOSION_Y_OFFSET = 20;
static const float ROCKET_BASE_SIZE   = 16.5f;  // px across, before random scaling
static const float GRAVITY            = 400.0f; // px/s^2, downward
static const float MAX_DT             = 0.1f;   // s

struct Rocket {
    float x, y;                 // position, px
    BYTE r, g, b;               // color
    float speed;                // px/s, downward
    float size;                 // px across, ROCKET_BASE_SIZE x 0.8-1.2
    ShapeType type;
    float rotation;             // radians
    float rotSpeed;             // radians/s, 0 for circles
    bool active;
    POINT trail[TRAIL_POINTS];  // trail[0] is the newest position
    int trailCount;
};

struct Particle {
    float x, y;                 // position, px
    float vx, vy;               // velocity, px/s
    BYTE r, g, b;               // tinted color at full opacity
    float alpha;                // 1.0 opaque .. 0.0 gone
    float lifetime;             // s, 1.5-2.5
    float age;                  // s
    bool active;
    bool isLine;
    int radius;                 // dots: 1-2 px
    int lineLen;                // lines: 2-5 px
    POINT trail[DOT_TRAIL_POINTS];  // dots only; trail[0] is the newest position
    int trailCount;
};
```

- **FW-4** Positions and velocities MUST be `float`. With whole-pixel integers, a 33 ms step truncates small movements to zero: slow particles stop moving sideways, and rockets fall about 10% too slowly.

File-scope state:

| Name | Type | Purpose |
|---|---|---|
| `g_rockets` | `Rocket[12]` | rocket pool |
| `g_particles` | `Particle[2000]` | particle pool |
| `g_lastSpawnMs` | `ULONGLONG` | `GetTickCount64()` at the last spawn |
| `g_nextSpawnDelayMs` | `int` | 500–1000, re-drawn after each spawn |
| `g_lastUpdateMs` | `ULONGLONG` | time of the previous update, for `dt` |
| `g_fireworksRunning` | `bool` | true while painting black-phase frames; drives the reset in FW-1 |
| `g_screenW`, `g_screenH` | `int` | client size, set every frame before updating |

Randomness uses `rand()`, already seeded at startup. A float in `[lo, hi]` is `lo + (hi - lo) * (rand() % 10001) / 10000.0f`.

### 4.3 Update logic (called from WM_PAINT)

Given `now = GetTickCount64()`:

0. **Reset** (only on the first black-phase frame, FW-1): deactivate every rocket and particle; set `g_lastSpawnMs = g_lastUpdateMs = now`; draw a new `g_nextSpawnDelayMs`.
1. Compute `dt` per FW-3 and store `g_lastUpdateMs = now`.
2. **Spawn**: if `now - g_lastSpawnMs >= g_nextSpawnDelayMs`, activate the first inactive rocket (if any) with the random properties from section 3.1. Its speed is `(g_screenH - EXPLOSION_Y_OFFSET) / travelTime`, where `travelTime` is 3.5–5.5 s. Set `g_lastSpawnMs = now` and draw a new delay.
3. **Rockets**: for each active rocket:
   - `y += speed * dt`; `rotation += rotSpeed * dt`.
   - Shift `trail` down one slot (dropping the oldest once 9 are stored) and put the current position in `trail[0]`.
   - If `y >= g_screenH - EXPLOSION_Y_OFFSET`, explode it (step 4) and deactivate it.
4. **Explosion**: spawn 30–60 particles at `(rocket.x, g_screenH - EXPLOSION_Y_OFFSET)`, filling inactive pool slots until the count is reached or the pool runs out. Each particle gets the properties from section 3.2.
   - **FW-5** Keep looping after each particle is placed. Returning after the first free slot gives one particle per explosion (section 9, F-1).
5. **Particles**: for each active particle:
   - `x += vx * dt`; `y += vy * dt`; `vy += GRAVITY * dt`; `age += dt`.
   - `alpha = max(0, 1 - age / lifetime)`.
   - For dots only: shift `trail` down one slot (dropping the oldest once 6 are stored) and put the current position in `trail[0]`. New particles start with `trailCount = 0`.
   - Deactivate if `age >= lifetime` or `y > g_screenH + 10`.

### 4.4 Explosion position

Explosions happen at `explosionY = g_screenH - 20`, 20 pixels above the bottom edge. The fastest upward particle (500 px/s against 400 px/s² gravity) rises about 312 px (500² ÷ 800), which stays well inside any real screen height.

### 4.5 Rendering (WM_PAINT, black phase only)

Draw each frame into an off-screen bitmap, then copy it to the window in one step so nothing flickers.

- **FW-6** The off-screen buffer is a memory DC from `CreateCompatibleDC(hdc)` with a `CreateCompatibleBitmap(hdc, w, h)` selected into it. Create it on the first fireworks frame and recreate it only if the client size changes. Save the bitmap that `SelectObject` returns, and select it back before deleting the DC and bitmap. Free the buffer in `WM_DESTROY`.
- **FW-7** Call `SetGraphicsMode(bufferDC, GM_ADVANCED)` when the buffer is created. Without it `SetWorldTransform` fails and nothing rotates (section 9, F-2). After drawing each rotated shape, call `ModifyWorldTransform(bufferDC, nullptr, MWT_IDENTITY)`.
- **FW-8** Select the stock `DC_PEN` and `DC_BRUSH` into the buffer once, and set colors with `SetDCPenColor` / `SetDCBrushColor`. Don't create and delete pens or brushes per shape or per particle. With up to 2000 particles, that is thousands of GDI allocations per frame.
- **FW-9** Copy the finished buffer to the window with `BitBlt(..., SRCCOPY)`. Do not use `AlphaBlend`: GDI drawing leaves the bitmap's alpha channel at zero, so a per-pixel-alpha blend treats the whole frame as transparent.

Draw order, each frame:

1. Fill the buffer with black.
2. Trails (section 3.3), faded per section 3.5.
3. Rockets as 1 px outlines in their own color (FW-11), rotated about their center (squares and stars). Select the stock `NULL_BRUSH` while drawing them so `Ellipse` and `Rectangle` don't fill, then restore `DC_BRUSH`; draw the pentagram with one `Polyline` of 6 points.
4. Particles (section 3.2), each faded by its `alpha`: for a dot, first its trail segments (section 3.3) with `MoveToEx`/`LineTo`, then the dot with `Ellipse`; for a line particle, one `MoveToEx`/`LineTo`.
5. `BitBlt` the buffer to the window.

If the buffer can't be created, fill the window with black for that frame instead.

---

## 5. Exit conditions

Fireworks mode doesn't change any exit conditions:

- Any key press, mouse button click, mouse wheel, or mouse movement beyond 4 pixels still closes the saver immediately.
- The fireworks display is replaced by the next color cycle (fade to red) when the black phase ends.
- No extra input handling is needed.

---

## 6. Build changes

### 6.1 Source files

No new source files. All fireworks code lives in `main.cpp`.

### 6.2 Dependencies

**None added.** Every call used is plain GDI or user32, already linked through `-lgdi32` and the default libraries: `CreateCompatibleDC`, `CreateCompatibleBitmap`, `SelectObject`, `DeleteObject`, `DeleteDC`, `SetGraphicsMode`, `SetWorldTransform`, `ModifyWorldTransform`, `SetDCPenColor`, `SetDCBrushColor`, `FillRect`, `MoveToEx`, `LineTo`, `Ellipse`, `Rectangle`, `Polygon`, `BitBlt`.

- Add `#include <cmath>` for `std::sin`, `std::cos` and `std::sqrt`.
- **FW-10** Call GDI functions directly. Do not load them at runtime with `LoadLibrary`/`GetProcAddress`. Hand-written function-pointer types don't match the real signatures, and some functions aren't in `gdi32.dll` at all (`FillRect` is in `user32.dll`), so the pointer is null and the first call crashes (section 9, F-7).
- GDI+ (`gdiplus.dll`, `GdiplusStartup`) is not used. `AlphaBlend` lives in `msimg32.dll`, not GDI+, and isn't used either (FW-9).

### 6.3 Makefile

No changes. In particular, keep the windres rule exactly as `docs/ghost-saver-spec.md` section 7.4 gives it. Do not add `--preprocessor="cat"` (rule R-2 there); it silently drops the manifest. Do not add `-lgdiplus` or `-lmsimg32`.

The build MUST still finish with zero warnings.

---

## 7. Acceptance criteria

**Visual** (run `ghost-saver.scr /s` and wait for the black phase, 45–75 s after launch):

1. During the 30-second black phase, shapes fall from random positions along the top edge.
2. Each shape takes 3.5–5.5 seconds to reach the bottom; some noticeably faster than others.
3. Shapes are outline circles, outline squares and 5-line pentagram stars, about 13–20 px across and visibly varied in size; squares and stars visibly rotate. No shape is filled.
4. Each shape leaves a short fading trail.
5. At the bottom, each shape bursts into a spray of 30–60 particles in all directions. The upward half climbs up to roughly a third of a 1080p screen before falling back.
6. Particles are near-white with a hint of the shape's color, arc downward under gravity, and fade out over 1.5–2.5 seconds. Each dot drags a short curved single-pixel trail that fades with it.
7. Key presses and mouse movement still close the saver immediately.
8. Preview (`/p`) never shows fireworks.
9. When the black phase ends, the fade to red starts cleanly. At the next black phase the screen starts empty, with no leftover rockets or particles.
10. Running for 30 minutes shows a stable GDI object count and memory use.

**Harness** (throwaway, not committed; drives `UpdateFireworks` with controlled timestamps and renders into memory DCs):

11. 2000 explosions each produce between 30 and 60 particles, and both limits occur.
12. Particle speeds are within 160–500 px/s, lifetimes within 1.5–2.5 s, dot radius 1–2, line length 2–5; a red rocket's particles are (255, 216, 216).
13. A particle with a 2 s lifetime has `alpha` ≈ 0.5 after 1 s and is inactive after 2 s. Launched straight up at 200 px/s, it rises about 50 px.
14. Rocket fall time, stepped at 33 ms, is within 3.5–5.5 s; size is within 13.2–19.8 px and both ends of the range occur; circles have `rotSpeed == 0`, squares and stars 0.5–2.0 rad/s in magnitude.
15. After 20 frames a rocket's trail holds 9 points, newest first.
15a. A dot launched straight up at 500 px/s rises about 312 px. Its trail grows to 6 points, newest first, and line particles record no trail.
16. A 5-second gap between updates moves a rocket no more than `speed × 0.1 s`.
17. `SetWorldTransform` succeeds on the buffer DC, and a square drawn at 45° is measurably wider than one at 0°.
17a. Drawn at 80 px: the circle and square have a dark center with lit edges; the star lights all 5 tips and the midpoint of the tip 0 → tip 2 line, keeps a dark center, and lights fewer than 600 pixels in a 200×200 area (lines only, no fill).
18. Over 900 rendered frames (30 s), `GetGuiResources(GR_GDIOBJECTS)` doesn't change; freeing the buffer lowers it.
19. The color-cycle reference tables in `docs/ghost-saver-spec.md` section 3.4 still pass exactly.

---

## 8. Out of scope

- Per-particle physics beyond gravity (wind, air resistance).
- Sound effects.
- Configurable fireworks parameters (particle count, gravity, speed range).
- Fireworks during preview mode.
- Separate fireworks per monitor; one display covers the virtual screen.
- Custom color palettes.
- Glow or bloom effects.

---

## 9. Findings from the first implementation (2026-09-13)

The first build of this spec failed to compile and, once it compiled, had these problems. Each is now covered by a requirement above.

| # | Finding | Symptom | Covered by |
|---|---|---|---|
| F-1 | `explodeRocket` returned after placing the first particle | One particle per explosion instead of 30–60 | FW-5 |
| F-2 | `SetWorldTransform` called without `SetGraphicsMode(GM_ADVANCED)` | Squares and stars never rotate | FW-7 |
| F-3 | Particle `alpha` computed but never used when drawing | Particles don't fade | 3.5, 4.5 |
| F-4 | Integer positions and velocities; fixed `dt = 0.033` | Slow particles don't move sideways; rockets fall ~10% slow; speed drifts with timer jitter | FW-3, FW-4 |
| F-5 | Trail appended at the end until full, then shifted from the front | Trail order flips once full; the trail line jumps | 3.3, 4.3 step 3 |
| F-6 | Rocket radius 5–12 px | Shapes 10–24 px across instead of 10–12 | 3.1 |
| F-7 | GDI functions loaded through `GetProcAddress` with made-up types; `FillRect` looked up in `gdi32.dll` | 14 compile errors, 13 warnings; would crash on first use | FW-10 |
| F-8 | Missing `#include <cmath>` | `sin`/`cos` not declared | 6.2 |
| F-9 | `AlphaBlend` with per-pixel alpha on a GDI-drawn bitmap | Frame treated as transparent, smearing over the previous frame | FW-9 |
| F-10 | Rockets and particles not cleared between black phases | Frozen leftovers resume at the next black phase | FW-1 |
| F-11 | Pen and brush created and deleted per shape and per particle; selected bitmap deleted without restoring the old one | Thousands of GDI allocations per frame; buffer bitmap not actually freed | FW-6, FW-8 |
| F-12 | `--preprocessor="cat"` added back to the Makefile's windres rule | Manifest stored under a type Windows ignores | 6.3 |
| F-13 | Rockets drawn as filled shapes; star drawn as a 10-point filled polygon | Falling shapes with trails looked like tadpoles rather than fireworks (owner review) | FW-11 |
