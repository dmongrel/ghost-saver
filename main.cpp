#include <windows.h>
#include <windowsx.h>
#include <cstdlib>
#include <cwctype>

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
    // black phase (or past the end of the cycle)
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
        BYTE r, g, b;
        ColorAt(g_cycle, GetTickCount64() - g_cycleStartMs, r, g, b);
        SetDCBrushColor(hdc, RGB(r, g, b));
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
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
        // Windows sends a WM_MOUSEMOVE when the window appears; only real movement exits.
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
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

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
        // skip prefix / or -
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
                // next token
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
