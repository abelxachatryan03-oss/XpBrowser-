#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include "WebView2.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

// ─────────── globals ───────────
static HWND g_hMain        = nullptr;
static HWND g_hAddress     = nullptr;
static HWND g_hWebViewHost = nullptr;

static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2*           g_webview    = nullptr;

static const int TOOLBAR_H = 46;
static const int BTN_W     = 30;
static const int BTN_H     = 30;

// XP Luna Blue цвета
static const COLORREF XP_BLUE_TOP   = RGB(  0,  88, 238);
static const COLORREF XP_BLUE_MID   = RGB( 63, 140, 243);
static const COLORREF XP_BLUE_DARK  = RGB(  0,  60, 170);
static const COLORREF XP_BTN_TOP    = RGB(248, 252, 255);
static const COLORREF XP_BTN_BOT    = RGB(186, 210, 241);
static const COLORREF XP_BTN_BORDER = RGB(  0,  60, 116);

#define ID_BACK    1001
#define ID_FORWARD 1002
#define ID_RELOAD  1003
#define ID_HOME    1004
#define ID_GO      1005
#define ID_ADDR    1006

// ─────────── хелперы ───────────
static void GradientRect(HDC hdc, RECT& rc, COLORREF top, COLORREF bot) {
    TRIVERTEX v[2];
    v[0].x = rc.left;  v[0].y = rc.top;
    v[0].Red   = (COLOR16)(GetRValue(top) << 8);
    v[0].Green = (COLOR16)(GetGValue(top) << 8);
    v[0].Blue  = (COLOR16)(GetBValue(top) << 8);
    v[0].Alpha = 0xFF00;

    v[1].x = rc.right; v[1].y = rc.bottom;
    v[1].Red   = (COLOR16)(GetRValue(bot) << 8);
    v[1].Green = (COLOR16)(GetGValue(bot) << 8);
    v[1].Blue  = (COLOR16)(GetBValue(bot) << 8);
    v[1].Alpha = 0xFF00;

    GRADIENT_RECT g{0, 1};
    GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V);
}

static HFONT MakeFont(int size, bool bold = false) {
    return CreateFontW(size, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Tahoma");
}

static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
    return dir;
}

// ─────────── WebView ───────────
static void UpdateNavButtons() {
    if (!g_webview) return;
    InvalidateRect(g_hMain, nullptr, FALSE);
}

static void CreateWebView() {
    if (!g_hWebViewHost) return;

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) return result;

                env->CreateCoreWebView2Controller(
                    g_hWebViewHost,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT r2, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(r2) || !ctrl) return r2;
                            g_controller = ctrl;
                            g_controller->AddRef();
                            g_controller->get_CoreWebView2(&g_webview);

                            ICoreWebView2Settings* s = nullptr;
                            if (SUCCEEDED(g_webview->get_Settings(&s)) && s) {
                                s->put_IsScriptEnabled(TRUE);
                                s->put_AreDefaultScriptDialogsEnabled(TRUE);
                                s->put_IsWebMessageEnabled(TRUE);
                                s->Release();
                            }

                            EventRegistrationToken tok;
                            g_webview->add_NavigationCompleted(
                                Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* sender,
                                       ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                        LPWSTR uri = nullptr;
                                        sender->get_Source(&uri);
                                        if (uri) {
                                            SetWindowTextW(g_hAddress, uri);
                                            CoTaskMemFree(uri);
                                        }
                                        UpdateNavButtons();
                                        return S_OK;
                                    }).Get(),
                                &tok);

                            RECT rc;
                            GetClientRect(g_hWebViewHost, &rc);
                            g_controller->put_Bounds(rc);
                            g_controller->put_IsVisible(TRUE);

                            std::wstring startUrl = L"file:///" + GetExeDir() + L"start.html";
                            for (auto& c : startUrl) if (c == L'\\') c = L'/';

                            g_webview->Navigate(startUrl.c_str());
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

// ─────────── рисование ───────────
static void DrawToolbar(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right;

    RECT tb = {0, 0, W, TOOLBAR_H};
    GradientRect(hdc, tb, XP_BLUE_MID, XP_BLUE_TOP);

    RECT topLine = {0, 0, W, 1};
    HBRUSH hb = CreateSolidBrush(RGB(155, 190, 245));
    FillRect(hdc, &topLine, hb);
    DeleteObject(hb);

    RECT botLine = {0, TOOLBAR_H - 1, W, TOOLBAR_H};
    hb = CreateSolidBrush(XP_BLUE_DARK);
    FillRect(hdc, &botLine, hb);
    DeleteObject(hb);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    HFONT hOld = (HFONT)SelectObject(hdc, MakeFont(11, true));
    RECT logoRc = {10, 0, 200, TOOLBAR_H};
    DrawTextW(hdc, L"XpBrowser", -1, &logoRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
}

static void DrawXpButton(LPDRAWITEMSTRUCT dis, const wchar_t* text, bool round = false) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    bool pressed  = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF top = pressed ? XP_BTN_BOT : XP_BTN_TOP;
    COLORREF bot = pressed ? XP_BTN_TOP : XP_BTN_BOT;
    if (disabled) { top = RGB(230,230,230); bot = RGB(210,210,210); }

    GradientRect(hdc, rc, top, bot);

    HPEN pen = CreatePen(PS_SOLID, 1, XP_BTN_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    if (round) {
        Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
    } else {
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? RGB(140,140,140) : RGB(0,0,0));
    HFONT hFont = MakeFont(14, true);
    HFONT oldF = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldF);
    DeleteObject(hFont);
}

// ─────────── layout ───────────
static void LayoutChildren(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    int x = 130;
    int y = (TOOLBAR_H - BTN_H) / 2;

    MoveWindow(g_hAddress, x, y, W - x - 80 - 12, BTN_H, TRUE);

    if (g_hWebViewHost)
        MoveWindow(g_hWebViewHost, 0, TOOLBAR_H, W, H - TOOLBAR_H, TRUE);

    if (g_controller) {
        RECT b = {0, 0, W, H - TOOLBAR_H};
        g_controller->put_Bounds(b);
    }
}

static void DoNavigate() {
    if (!g_webview) return;
    wchar_t buf[2048] = {};
    GetWindowTextW(g_hAddress, buf, 2048);
    if (buf[0] == 0) return;

    std::wstring url = buf;
    if (url.find(L"://") == std::wstring::npos &&
        url.rfind(L"file:", 0) != 0 &&
        url.rfind(L"about:", 0) != 0) {
        url = L"https://" + url;
    }
    g_webview->Navigate(url.c_str());
}

// ─────────── WndProc ───────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

        DWORD st = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW;
        int navY = (TOOLBAR_H - BTN_H) / 2;
        CreateWindowW(L"BUTTON", L"←", st, 130, navY, BTN_W, BTN_H,
                      hwnd, (HMENU)ID_BACK, hi, nullptr);
        CreateWindowW(L"BUTTON", L"→", st, 130 + BTN_W, navY, BTN_W, BTN_H,
                      hwnd, (HMENU)ID_FORWARD, hi, nullptr);
        CreateWindowW(L"BUTTON", L"⟳", st, 130 + BTN_W*2, navY, BTN_W, BTN_H,
                      hwnd, (HMENU)ID_RELOAD, hi, nullptr);
        CreateWindowW(L"BUTTON", L"⌂", st, 130 + BTN_W*3, navY, BTN_W, BTN_H,
                      hwnd, (HMENU)ID_HOME, hi, nullptr);

        g_hAddress = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            130 + BTN_W*4 + 8, navY, 400, BTN_H,
            hwnd, (HMENU)ID_ADDR, hi, nullptr);
        SendMessage(g_hAddress, WM_SETFONT, (WPARAM)MakeFont(10, false), TRUE);

        CreateWindowW(L"BUTTON", L"→ Перейти",
                      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      0, navY, 80, BTN_H,
                      hwnd, (HMENU)ID_GO, hi, nullptr);

        g_hWebViewHost = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, TOOLBAR_H, 100, 100, hwnd, nullptr, hi, nullptr);

        // ─── звук запуска XP ───
        std::wstring startupPath = GetExeDir() + L"startup.wav";
        PlaySoundW(startupPath.c_str(), nullptr,
                   SND_FILENAME | SND_ASYNC | SND_NODEFAULT);

        CreateWebView();
        return 0;
    }

    case WM_SIZE:
        LayoutChildren(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawToolbar(hwnd, hdc);
        RECT rc;
        GetClientRect(hwnd, &rc);
        RECT body = {0, TOOLBAR_H, rc.right, rc.bottom};
        HBRUSH hb = CreateSolidBrush(RGB(236, 233, 216));
        FillRect(hdc, &body, hb);
        DeleteObject(hb);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)l;
        switch (dis->CtlID) {
        case ID_BACK:    DrawXpButton(dis, L"←", true);  return TRUE;
        case ID_FORWARD: DrawXpButton(dis, L"→", true);  return TRUE;
        case ID_RELOAD:  DrawXpButton(dis, L"⟳", true);  return TRUE;
        case ID_HOME:    DrawXpButton(dis, L"⌂", true);  return TRUE;
        case ID_GO:      DrawXpButton(dis, L"Перейти", false); return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        switch (id) {
        case ID_BACK:    if (g_webview) g_webview->GoBack();     break;
        case ID_FORWARD: if (g_webview) g_webview->GoForward();  break;
        case ID_RELOAD:  if (g_webview) g_webview->Reload();     break;
        case ID_HOME: {
            if (g_webview) {
                std::wstring u = L"file:///" + GetExeDir() + L"start.html";
                for (auto& c : u) if (c == L'\\') c = L'/';
                g_webview->Navigate(u.c_str());
            }
            break;
        }
        case ID_GO:
            DoNavigate();
            break;
        }
        return 0;
    }

    case WM_DESTROY: {
        if (g_controller) { g_controller->Release(); g_controller = nullptr; }
        if (g_webview)    { g_webview->Release();    g_webview    = nullptr; }

        // ─── звук выхода XP ───
        std::wstring shutdownPath = GetExeDir() + L"shutdown.wav";
        PlaySoundW(shutdownPath.c_str(), nullptr,
                   SND_FILENAME | SND_SYNC);

        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

// ─────────── entry ───────────
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"XpBrowserClass";
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    g_hMain = CreateWindowExW(
        0, L"XpBrowserClass", L"XpBrowser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        sw * 3 / 4, sh * 3 / 4,
        nullptr, nullptr, hInst, nullptr);

    if (!g_hMain) return 0;

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);

    MSG m{};
    while (GetMessage(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    CoUninitialize();
    return (int)m.wParam;
}
