#include <windows.h>

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
    ULONGLONG t = ms;
    // fade black->red
    if (t < c.fadeMs[0]) {
        int D = (int)c.fadeMs[0], ti = (int)t;
        r = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        g = 0; b = 0; return;
    } t -= c.fadeMs[0];
    // hold red
    if (t < c.holdMs[0]) { r = 255; g = 0; b = 0; return; } t -= c.holdMs[0];
    // fade red->green
    if (t < c.fadeMs[1]) {
        int D = (int)c.fadeMs[1], ti = (int)t;
        r = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        g = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        b = 0; return;
    } t -= c.fadeMs[1];
    // hold green
    if (t < c.holdMs[1]) { r = 0; g = 255; b = 0; return; } t -= c.holdMs[1];
    // fade green->blue
    if (t < c.fadeMs[2]) {
        int D = (int)c.fadeMs[2], ti = (int)t;
        r = 0;
        g = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        b = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        return;
    } t -= c.fadeMs[2];
    // hold blue
    if (t < c.holdMs[2]) { r = 0; g = 0; b = 255; return; } t -= c.holdMs[2];
    // fade blue->yellow
    if (t < c.fadeMs[3]) {
        int D = (int)c.fadeMs[3], ti = (int)t;
        r = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        g = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        b = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        return;
    } t -= c.fadeMs[3];
    // hold yellow
    if (t < c.holdMs[3]) { r = 255; g = 255; b = 0; return; } t -= c.fadeMs[3];
    // fade yellow->cyan
    if (t < c.fadeMs[4]) {
        int D = (int)c.fadeMs[4], ti = (int)t;
        r = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        g = 255;
        b = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        return;
    } t -= c.fadeMs[4];
    // hold cyan
    if (t < c.holdMs[4]) { r = 0; g = 255; b = 255; return; } t -= c.fadeMs[4];
    // fade cyan->purple
    if (t < c.fadeMs[5]) {
        int D = (int)c.fadeMs[5], ti = (int)t;
        r = (BYTE)((0 * (D - ti) + 128 * ti + D / 2) / D);
        g = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        b = (BYTE)((255 * (D - ti) + 128 * ti + D / 2) / D);
        return;
    } t -= c.fadeMs[5];
    // hold purple
    if (t < c.holdMs[5]) { r = 128; g = 0; b = 128; return; } t -= c.fadeMs[5];
    // fade purple->white
    if (t < c.fadeMs[6]) {
        int D = (int)c.fadeMs[6], ti = (int)t;
        r = (BYTE)((128 * (D - ti) + 255 * ti + D / 2) / D);
        g = (BYTE)((0 * (D - ti) + 255 * ti + D / 2) / D);
        b = (BYTE)((128 * (D - ti) + 255 * ti + D / 2) / D);
        return;
    } t -= c.fadeMs[6];
    // hold white
    if (t < c.holdMs[6]) { r = 255; g = 255; b = 255; return; } t -= c.fadeMs[6];
    // fade white->black
    if (t < c.fadeMs[7]) {
        int D = (int)c.fadeMs[7], ti = (int)t;
        r = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        g = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        b = (BYTE)((255 * (D - ti) + 0 * ti + D / 2) / D);
        return;
    } t -= c.fadeMs[7];
    // black phase
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
    case WM_MOUSEWHEEL:
        if (!g_preview) DestroyWindow(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if (!g_preview) DestroyWindow(hwnd);
        return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (!g_preview) DestroyWindow(hwnd);
        return 0;
    case WM_ACTIVATEAPP:
        if (!g_preview && !wp) DestroyWindow(hwnd);
        return 0;
    case WM_SYSCOMMAND:
        if (wp == SC_SCREENSAVE) return 0;
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

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetTimer(hwnd, 1, 33, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
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

    HWND hwnd = CreateWindowExW(0, L"GhostSaverClass", nullptr,
        WS_CHILD | WS_VISIBLE,
        0, 0, 320, 240,
        hparent, nullptr, GetModuleHandle(nullptr), nullptr);

    RollCycle(g_cycle);
    g_cycleStartMs = GetTickCount64();

    SetTimer(hwnd, 1, 33, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
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
