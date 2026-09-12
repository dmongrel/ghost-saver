/*
 * ghost-saver - A screen saver for Windows 11 designed to remove
 * ghosting/burn-in artifacts by cycling through colors at various
 * brightness levels.
 *
 * Build: g++ -o ghost-saver.scr main.cpp -mwindows -lgdi32
 */

#include <windows.h>
#include <cmath>

static const int COLOR_PHASE_MS    = 10000;   // 10s per color
static const int NUM_COLORS        = 6;       // R, G, B, C, M, Y
static const int BLACK_DURATION_MS = 300000;  // 5 min black

struct ColorPhase { unsigned char r, g, b; };

static const ColorPhase baseColors[NUM_COLORS] = {
    {255,   0,   0}, {  0, 255,   0}, {  0,   0, 255},
    {  0, 255, 255}, {255,   0, 255}, {255, 255,   0},
};

static const float brightnessLevels[] = {0.3f, 0.5f, 0.7f, 0.85f, 1.0f};
static const int NUM_BRIGHTNESS = 5;

static HWND g_hwnd = nullptr;
static HBRUSH g_hBrush = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: return 0;

        case WM_PAINT: {
            DWORD startTime = (DWORD)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            DWORD elapsed = GetTickCount() - startTime;
            int totalColorDuration = COLOR_PHASE_MS * NUM_COLORS;
            int fullCycleDuration = totalColorDuration + BLACK_DURATION_MS;

            if (elapsed >= (DWORD)fullCycleDuration)
                elapsed = elapsed % (DWORD)fullCycleDuration;

            unsigned char r, g, b;

            if (elapsed >= (DWORD)totalColorDuration) {
                r = 0; g = 0; b = 0;
            } else {
                int colorIndex = (int)(elapsed / COLOR_PHASE_MS) % NUM_COLORS;
                float progress = (float)elapsed / (float)totalColorDuration;
                int brightnessIdx = (int)(progress * NUM_BRIGHTNESS) % NUM_BRIGHTNESS;
                float brightness = brightnessLevels[brightnessIdx];
                r = (unsigned char)(baseColors[colorIndex].r * brightness);
                g = (unsigned char)(baseColors[colorIndex].g * brightness);
                b = (unsigned char)(baseColors[colorIndex].b * brightness);
            }

            if (g_hBrush) DeleteObject(g_hBrush);
            g_hBrush = CreateSolidBrush(RGB(r, g, b));

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, g_hBrush);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            if (g_hBrush) DeleteObject(g_hBrush);
            PostQuitMessage(0);
            return 0;

        case WM_KEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_SYSKEYDOWN:
            ShowCursor(TRUE);
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static INT_PTR CALLBACK ConfigDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowTextW(hwnd, L"Ghost Saver - Configuration");
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, LOWORD(wParam));
                return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)nCmdShow;

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    bool isConfig = false;
    bool isInParent = false;
    HWND parentHwnd = nullptr;

    for (int i = 1; i < argc; i++) {
        if (lstrcmpiW(argv[i], L"/c") == 0 || lstrcmpiW(argv[i], L"-c") == 0)
            isConfig = true;
        else if (lstrcmpiW(argv[i], L"/p") == 0 || lstrcmpiW(argv[i], L"-p") == 0) {
            isInParent = true;
            if (i + 1 < argc)
                parentHwnd = (HWND)(INT_PTR)wcstol(argv[i + 1], nullptr, 10);
        }
    }

    LocalFree(argv);

    if (isConfig || (isInParent && parentHwnd)) {
        DialogBoxParamW(hInstance, L"CONFIG_DIALOG", parentHwnd, ConfigDlgProc, 0);
        return 0;
    }

    const wchar_t* className = L"GhostSaverClass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = className;

    if (!RegisterClassExW(&wc)) return 1;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        className, L"Ghost Saver",
        WS_POPUP | WS_VISIBLE,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) { UnregisterClassW(className, hInstance); return 1; }

    SetWindowLongPtr(g_hwnd, GWLP_USERDATA, (LONG_PTR)GetTickCount());
    ShowCursor(FALSE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ShowCursor(TRUE);
    UnregisterClassW(className, hInstance);
    return (int)msg.wParam;
}
