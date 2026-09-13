#include <windows.h>
#include <windowsx.h>
#include <cstdlib>
#include <cmath>
#include <cwctype>

// ---------------------------------------------------------------------------
// Color cycle (unchanged)
// ---------------------------------------------------------------------------

static const struct { BYTE r, g, b; } STOPS[9] = {
    {  0,   0,   0 }, // black
    {255,   0,   0 }, // red
    {  0, 255,   0 }, // green
    {  0,   0, 255 }, // blue
    {255, 255,   0 }, // yellow
    {  0, 255, 255 }, // cyan
    {128,   0, 128 }, // purple
    {255, 255, 255 }, // white
    {  0,   0,   0 }, // black
};

struct Cycle {
    DWORD fadeMs[8];
    DWORD holdMs[7];
};

static const ULONGLONG BLACK_PHASE_MS = 30000;

static void RollCycle(Cycle& c) {
    for (int i = 0; i < 15; i++) {
        if (i < 8) c.fadeMs[i] = 3000 + rand() % 2001;
        else       c.holdMs[i - 8] = 3000 + rand() % 2001;
    }
}

static ULONGLONG CycleLength(const Cycle& c) {
    ULONGLONG sum = BLACK_PHASE_MS;
    for (int i = 0; i < 8; i++) sum += c.fadeMs[i];
    for (int i = 0; i < 7; i++) sum += c.holdMs[i];
    return sum;
}

static void ColorAt(const Cycle& c, ULONGLONG ms, BYTE& r, BYTE& g, BYTE& b) {
    for (int i = 0; i < 8; i++) {
        const auto& A = STOPS[i];
        const auto& B = STOPS[i + 1];
        if (ms < c.fadeMs[i]) {
            ULONGLONG D = c.fadeMs[i], t = ms;
            r = (BYTE)((A.r * (D - t) + B.r * t + D / 2) / D);
            g = (BYTE)((A.g * (D - t) + B.g * t + D / 2) / D);
            b = (BYTE)((A.b * (D - t) + B.b * t + D / 2) / D);
            return;
        }
        ms -= c.fadeMs[i];
        if (i < 7) {
            if (ms < c.holdMs[i]) { r = B.r; g = B.g; b = B.b; return; }
            ms -= c.holdMs[i];
        }
    }
    r = 0; g = 0; b = 0;
}

static Cycle g_cycle;
static ULONGLONG g_cycleStartMs;

static void AdvanceCycle() {
    ULONGLONG now = GetTickCount64();
    while (now - g_cycleStartMs >= CycleLength(g_cycle)) {
        g_cycleStartMs += CycleLength(g_cycle);
        RollCycle(g_cycle);
    }
}

// ---------------------------------------------------------------------------
// Fireworks mode (black phase only; see docs/ghost-saver-fireworks-spec.md)
// ---------------------------------------------------------------------------

enum ShapeType { SHAPE_CIRCLE, SHAPE_SQUARE, SHAPE_STAR };

static const BYTE FW_COLORS[7][3] = {
    {255,   0,   0 }, // red
    {  0, 255,   0 }, // green
    {  0,   0, 255 }, // blue
    {255, 255,   0 }, // yellow
    {  0, 255, 255 }, // cyan
    {128,   0, 128 }, // purple
    {255, 255, 255 }, // white
};

static const int   MAX_ROCKETS        = 12;
static const int   MAX_PARTICLES      = 2000;
static const int   TRAIL_POINTS       = 9;      // 8 segments, ~0.27 s at 30 fps
static const int   DOT_TRAIL_POINTS   = 6;      // 5 segments behind each explosion dot
static const int   EXPLOSION_Y_OFFSET = 20;
static const float ROCKET_BASE_SIZE   = 16.5f;  // px across, before random scaling
static const float GRAVITY            = 400.0f; // px/s^2, downward
static const float MAX_DT             = 0.1f;   // s, caps jumps after a stall
static const float PI_F               = 3.14159265f;

struct Rocket {
    float x, y;
    BYTE r, g, b;
    float speed;                // px/s, downward
    float size;                 // px across, ROCKET_BASE_SIZE scaled by 0.8-1.2
    ShapeType type;
    float rotation;             // radians
    float rotSpeed;             // radians/s, 0 for circles
    bool active;
    POINT trail[TRAIL_POINTS];  // trail[0] is the newest position
    int trailCount;
};

struct Particle {
    float x, y;
    float vx, vy;               // px/s
    BYTE r, g, b;               // tinted color at full opacity
    float alpha;                // 1.0 = opaque, 0.0 = gone
    float lifetime;             // s
    float age;                  // s
    bool active;
    bool isLine;
    int radius;                 // dots: 1-2 px
    int lineLen;                // lines: 2-5 px
    POINT trail[DOT_TRAIL_POINTS];  // dots only; trail[0] is the newest position
    int trailCount;
};

static Rocket    g_rockets[MAX_ROCKETS];
static Particle  g_particles[MAX_PARTICLES];
static ULONGLONG g_lastSpawnMs;
static ULONGLONG g_lastUpdateMs;
static int       g_nextSpawnDelayMs;
static bool      g_fireworksRunning;
static int       g_screenW, g_screenH;

static float RandRange(float lo, float hi) {
    return lo + (hi - lo) * (float)(rand() % 10001) / 10000.0f;
}

// Called when a black phase starts, so nothing carries over from the last one.
static void ResetFireworks(ULONGLONG now) {
    for (auto& rk : g_rockets) rk.active = false;
    for (auto& p : g_particles) p.active = false;
    g_lastSpawnMs = now;
    g_lastUpdateMs = now;
    g_nextSpawnDelayMs = 500 + rand() % 501;
}

static void SpawnRocket() {
    if (g_screenW <= 0 || g_screenH <= EXPLOSION_Y_OFFSET) return;
    for (auto& rk : g_rockets) {
        if (rk.active) continue;
        int c = rand() % 7;
        rk.x = (float)(rand() % g_screenW);
        rk.y = 0.0f;
        rk.r = FW_COLORS[c][0];
        rk.g = FW_COLORS[c][1];
        rk.b = FW_COLORS[c][2];
        rk.speed = (float)(g_screenH - EXPLOSION_Y_OFFSET) / RandRange(3.5f, 5.5f);
        rk.size = ROCKET_BASE_SIZE * RandRange(0.8f, 1.2f);
        rk.type = (ShapeType)(rand() % 3);
        rk.rotation = RandRange(0.0f, 2.0f * PI_F);
        rk.rotSpeed = rk.type == SHAPE_CIRCLE ? 0.0f
                    : RandRange(0.5f, 2.0f) * (rand() % 2 ? 1.0f : -1.0f);
        rk.trailCount = 0;
        rk.active = true;
        return;
    }
}

static void ExplodeRocket(const Rocket& rk) {
    int count = 30 + rand() % 31;
    for (auto& p : g_particles) {
        if (count == 0) break;
        if (p.active) continue;
        float angle = RandRange(0.0f, 2.0f * PI_F);
        float speed = RandRange(160.0f, 500.0f);
        p.x = rk.x;
        p.y = (float)(g_screenH - EXPLOSION_Y_OFFSET);
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        // 85% white + 15% rocket color
        p.r = (BYTE)((rk.r * 15 + 255 * 85) / 100);
        p.g = (BYTE)((rk.g * 15 + 255 * 85) / 100);
        p.b = (BYTE)((rk.b * 15 + 255 * 85) / 100);
        p.lifetime = RandRange(1.5f, 2.5f);
        p.age = 0.0f;
        p.alpha = 1.0f;
        p.isLine = rand() % 3 == 0;
        p.radius = 1 + rand() % 2;
        p.lineLen = 2 + rand() % 4;
        p.trailCount = 0;
        p.active = true;
        count--;
    }
}

static void UpdateFireworks(ULONGLONG now) {
    float dt = (float)(now - g_lastUpdateMs) / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;
    g_lastUpdateMs = now;

    if (now - g_lastSpawnMs >= (ULONGLONG)g_nextSpawnDelayMs) {
        SpawnRocket();
        g_lastSpawnMs = now;
        g_nextSpawnDelayMs = 500 + rand() % 501;
    }

    for (auto& rk : g_rockets) {
        if (!rk.active) continue;
        rk.y += rk.speed * dt;
        rk.rotation += rk.rotSpeed * dt;

        int n = rk.trailCount < TRAIL_POINTS ? rk.trailCount + 1 : TRAIL_POINTS;
        for (int j = n - 1; j > 0; j--) rk.trail[j] = rk.trail[j - 1];
        rk.trail[0].x = (LONG)rk.x;
        rk.trail[0].y = (LONG)rk.y;
        rk.trailCount = n;

        if (rk.y >= (float)(g_screenH - EXPLOSION_Y_OFFSET)) {
            ExplodeRocket(rk);
            rk.active = false;
        }
    }

    for (auto& p : g_particles) {
        if (!p.active) continue;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += GRAVITY * dt;

        if (!p.isLine) {
            int n = p.trailCount < DOT_TRAIL_POINTS ? p.trailCount + 1 : DOT_TRAIL_POINTS;
            for (int j = n - 1; j > 0; j--) p.trail[j] = p.trail[j - 1];
            p.trail[0].x = (LONG)p.x;
            p.trail[0].y = (LONG)p.y;
            p.trailCount = n;
        }
        p.age += dt;
        p.alpha = 1.0f - p.age / p.lifetime;
        if (p.alpha < 0.0f) p.alpha = 0.0f;
        if (p.age >= p.lifetime || p.y > (float)(g_screenH + 10)) p.active = false;
    }
}

// ---------------------------------------------------------------------------
// Fireworks render
// ---------------------------------------------------------------------------

static HDC     g_fwDC = nullptr;
static HBITMAP g_fwBM = nullptr;
static HBITMAP g_fwOldBM = nullptr;
static int     g_fwW = 0, g_fwH = 0;

// Blend a color over black at the given opacity.
static COLORREF Faded(BYTE r, BYTE g, BYTE b, float alpha) {
    return RGB((BYTE)(r * alpha + 0.5f), (BYTE)(g * alpha + 0.5f), (BYTE)(b * alpha + 0.5f));
}

static void FreeFireworkBuffer() {
    if (g_fwDC) {
        SelectObject(g_fwDC, g_fwOldBM);
        DeleteDC(g_fwDC);
    }
    if (g_fwBM) DeleteObject(g_fwBM);
    g_fwDC = nullptr;
    g_fwBM = nullptr;
    g_fwOldBM = nullptr;
    g_fwW = g_fwH = 0;
}

static bool EnsureFireworkBuffer(HDC hdc, int w, int h) {
    if (g_fwDC && g_fwW == w && g_fwH == h) return true;
    FreeFireworkBuffer();
    g_fwDC = CreateCompatibleDC(hdc);
    g_fwBM = CreateCompatibleBitmap(hdc, w, h);
    if (!g_fwDC || !g_fwBM) { FreeFireworkBuffer(); return false; }
    g_fwOldBM = (HBITMAP)SelectObject(g_fwDC, g_fwBM);
    SetGraphicsMode(g_fwDC, GM_ADVANCED);   // required for SetWorldTransform
    SelectObject(g_fwDC, GetStockObject(DC_PEN));
    SelectObject(g_fwDC, GetStockObject(DC_BRUSH));
    g_fwW = w;
    g_fwH = h;
    return true;
}

// Rockets are 1 px outlines, never filled.
static void DrawShape(HDC hdc, int cx, int cy, float size, ShapeType type, float rotation, COLORREF color) {
    SetDCPenColor(hdc, color);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    if (type != SHAPE_CIRCLE) {
        // Rotate about (cx, cy)
        float c = std::cos(rotation), s = std::sin(rotation);
        XFORM xf;
        xf.eM11 = c;
        xf.eM12 = s;
        xf.eM21 = -s;
        xf.eM22 = c;
        xf.eDx = (float)cx - (float)cx * c + (float)cy * s;
        xf.eDy = (float)cy - (float)cx * s - (float)cy * c;
        SetWorldTransform(hdc, &xf);
    }

    int px = (int)(size + 0.5f);
    int left = cx - px / 2, top = cy - px / 2;
    switch (type) {
    case SHAPE_CIRCLE:
        Ellipse(hdc, left, top, left + px, top + px);
        break;
    case SHAPE_SQUARE:
        Rectangle(hdc, left, top, left + px, top + px);
        break;
    case SHAPE_STAR: {
        // Pentagram: 5 tips joined by 5 straight lines (tip 0 -> 2 -> 4 -> 1 -> 3 -> 0)
        static const int ORDER[6] = { 0, 2, 4, 1, 3, 0 };
        POINT pts[6];
        for (int i = 0; i < 6; i++) {
            float angle = (float)ORDER[i] * 2.0f * PI_F / 5.0f - PI_F / 2.0f;
            pts[i].x = cx + (LONG)std::floor(std::cos(angle) * size / 2.0f + 0.5f);
            pts[i].y = cy + (LONG)std::floor(std::sin(angle) * size / 2.0f + 0.5f);
        }
        Polyline(hdc, pts, 6);
        break;
    }
    }

    if (type != SHAPE_CIRCLE) ModifyWorldTransform(hdc, nullptr, MWT_IDENTITY);
    SelectObject(hdc, oldBrush);
}

static void RenderFireworks(HDC hdc, int w, int h) {
    RECT rc = { 0, 0, w, h };
    if (!EnsureFireworkBuffer(hdc, w, h)) {
        SetDCBrushColor(hdc, RGB(0, 0, 0));
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
        return;
    }

    SetDCBrushColor(g_fwDC, RGB(0, 0, 0));
    FillRect(g_fwDC, &rc, (HBRUSH)GetStockObject(DC_BRUSH));

    // Trails: newest segment at 100% opacity, oldest at 10%
    for (auto& rk : g_rockets) {
        if (!rk.active) continue;
        int segs = rk.trailCount - 1;
        for (int j = 0; j < segs; j++) {
            float alpha = segs > 1 ? 1.0f - 0.9f * (float)j / (float)(segs - 1) : 1.0f;
            SetDCPenColor(g_fwDC, Faded(rk.r, rk.g, rk.b, alpha));
            MoveToEx(g_fwDC, rk.trail[j].x, rk.trail[j].y, nullptr);
            LineTo(g_fwDC, rk.trail[j + 1].x, rk.trail[j + 1].y);
        }
    }

    for (auto& rk : g_rockets) {
        if (!rk.active) continue;
        DrawShape(g_fwDC, (int)rk.x, (int)rk.y, rk.size, rk.type, rk.rotation, RGB(rk.r, rk.g, rk.b));
    }

    for (auto& p : g_particles) {
        if (!p.active || p.alpha <= 0.0f) continue;
        COLORREF col = Faded(p.r, p.g, p.b, p.alpha);
        int x = (int)p.x, y = (int)p.y;
        if (p.isLine) {
            // Short line trailing behind the particle's direction of travel
            float speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
            int x2 = x, y2 = y;
            if (speed > 0.001f) {
                x2 = x - (int)(p.vx / speed * (float)p.lineLen);
                y2 = y - (int)(p.vy / speed * (float)p.lineLen);
            }
            SetDCPenColor(g_fwDC, col);
            MoveToEx(g_fwDC, x, y, nullptr);
            LineTo(g_fwDC, x2, y2);
        } else {
            // Single-pixel trail: newest segment at the dot's opacity, oldest at 10% of it
            int segs = p.trailCount - 1;
            for (int j = 0; j < segs; j++) {
                float fade = segs > 1 ? 1.0f - 0.9f * (float)j / (float)(segs - 1) : 1.0f;
                SetDCPenColor(g_fwDC, Faded(p.r, p.g, p.b, p.alpha * fade));
                MoveToEx(g_fwDC, p.trail[j].x, p.trail[j].y, nullptr);
                LineTo(g_fwDC, p.trail[j + 1].x, p.trail[j + 1].y);
            }
            SetDCPenColor(g_fwDC, col);
            SetDCBrushColor(g_fwDC, col);
            Ellipse(g_fwDC, x - p.radius, y - p.radius, x + p.radius + 1, y + p.radius + 1);
        }
    }

    BitBlt(hdc, 0, 0, w, h, g_fwDC, 0, 0, SRCCOPY);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static bool g_preview;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        AdvanceCycle();
        ULONGLONG msIntoCycle = GetTickCount64() - g_cycleStartMs;

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // The black phase (fireworks) opens each cycle; the color fades follow it
        bool inBlackPhase = (msIntoCycle < BLACK_PHASE_MS);
        BYTE r = 0, g = 0, b = 0;
        if (!inBlackPhase) ColorAt(g_cycle, msIntoCycle - BLACK_PHASE_MS, r, g, b);

        if (inBlackPhase && !g_preview) {
            ULONGLONG now = GetTickCount64();
            if (!g_fireworksRunning) {
                ResetFireworks(now);
                g_fireworksRunning = true;
            }
            g_screenW = w;
            g_screenH = h;
            UpdateFireworks(now);
            RenderFireworks(hdc, w, h);
        } else {
            g_fireworksRunning = false;
            SetDCBrushColor(hdc, RGB(r, g, b));
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (g_preview) return DefWindowProc(hwnd, msg, wp, lp);
        SetCursor(nullptr);
        return TRUE;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (g_preview) return DefWindowProc(hwnd, msg, wp, lp);
        DestroyWindow(hwnd);
        return 0;
    case WM_MOUSEMOVE: {
        if (g_preview) return DefWindowProc(hwnd, msg, wp, lp);
        static bool haveStart = false;
        static int startX, startY;
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (!haveStart) { haveStart = true; startX = x; startY = y; return 0; }
        if (abs(x - startX) > 4 || abs(y - startY) > 4) DestroyWindow(hwnd);
        return 0;
    }
    case WM_ACTIVATEAPP:
        if (g_preview) return DefWindowProc(hwnd, msg, wp, lp);
        if (!wp) DestroyWindow(hwnd);
        return 0;
    case WM_SYSCOMMAND:
        if (!g_preview && (wp & 0xFFF0) == SC_SCREENSAVE) return 0;
        return DefWindowProc(hwnd, msg, wp, lp);
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        FreeFireworkBuffer();
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

static int RunFullScreen() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"GhostSaverClass";
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, L"GhostSaverClass", nullptr,
        WS_POPUP,
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_CYVIRTUALSCREEN),
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    RollCycle(g_cycle);
    g_cycleStartMs = GetTickCount64();

    SetTimer(hwnd, 1, 33, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnregisterClassW(L"GhostSaverClass", wc.hInstance);
    return 0;
}

static int RunPreview(HWND hparent) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"GhostSaverClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    RECT prc;
    GetClientRect(hparent, &prc);
    HWND hwnd = CreateWindowExW(0, L"GhostSaverClass", nullptr,
        WS_CHILD,
        0, 0, prc.right - prc.left, prc.bottom - prc.top,
        hparent, nullptr, GetModuleHandle(nullptr), nullptr);

    RollCycle(g_cycle);
    g_cycleStartMs = GetTickCount64();

    SetTimer(hwnd, 1, 33, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnregisterClassW(L"GhostSaverClass", wc.hInstance);
    return 0;
}

static int RunConfigure(HWND hparent) {
    MessageBoxW(hparent,
        L"Ghost Saver fades the whole screen through black, red, green, blue, yellow, cyan, purple and white, then rests on black, to help clear image retention and burn-in.\n\nThere are no settings to configure.",
        L"Ghost Saver", MB_OK | MB_ICONINFORMATION);
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    (void)hInstance; (void)nCmdShow;

    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    srand((unsigned)qpc.QuadPart);

    int argc;
    PWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 0;

    bool fullScreen = true;
    bool previewMode = false;
    HWND hparent = nullptr;

    if (argc > 1) {
        WCHAR* arg = argv[1];
        if (*arg == L'/' || *arg == L'-') arg++;
        WCHAR lower = towlower(*arg);

        if (lower == L's') {
            fullScreen = true;
        } else if (lower == L'p') {
            fullScreen = false;
            previewMode = true;
            arg++;
            if (*arg == L':') arg++;
            else {
                if (argc < 3) { LocalFree(argv); return 0; }
                arg = argv[2];
            }
            unsigned long hv = wcstoul(arg, nullptr, 10);
            hparent = (HWND)(UINT_PTR)hv;
            if (!hparent || !IsWindow(hparent)) { LocalFree(argv); return 0; }
        } else if (lower == L'c') {
            fullScreen = false;
            arg++;
            if (*arg == L':') arg++;
            else {
                if (argc < 3) arg = nullptr;
                else arg = argv[2];
            }
            if (arg && *arg) {
                unsigned long hv = wcstoul(arg, nullptr, 10);
                hparent = (HWND)(UINT_PTR)hv;
                if (!IsWindow(hparent)) hparent = nullptr;
            }
        } else {
            LocalFree(argv);
            return 0;
        }
    }

    LocalFree(argv);

    g_preview = previewMode;

    if (fullScreen) return RunFullScreen();
    if (previewMode) return RunPreview(hparent);
    return RunConfigure(hparent);
}
