#include <windows.h>
#include <shellapi.h>
#include <string>
#include <uxtheme.h>      
#include <vssym32.h>      
#include <dwmapi.h>      
#include "LanguageModule.h"
#include "BubbleModule.h"

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


bool IsDarkModeEnabled() {
    HKEY hKey;
    DWORD value = 1; // Default to Light
    DWORD size = sizeof(value);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }
    return value == 0;
}

// Global Handles
HWND g_hwndMain;
HWND g_hwndGameList, g_hwndSourceLang, g_hwndTargetLang, g_hwndFontColor, g_hwndShadowColor, g_hwndRefreshRate;

// Settings Variables (Defaults)
COLORREF g_SelectedFontColor = RGB(255, 255, 255); // White
COLORREF g_SelectedShadowColor = RGB(0, 0, 0);     // Black
int g_OCRRefreshRate = 10000;                       // 10 Seconds

// Function to refresh the list of active windows
void RefreshWindowList(HWND comboBox) {
    if (!comboBox || !IsWindow(comboBox)) return; // Safety check
    SendMessage(comboBox, CB_RESETCONTENT, 0, 0);
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindow(hwnd)) return TRUE; // Skip if window handle is dead
        if (IsWindowVisible(hwnd) && GetWindowTextLength(hwnd) > 0) {
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);

            // 1. Add the string
            int index = (int)SendMessage((HWND)lParam, CB_ADDSTRING, 0, (LPARAM)title);
            // 2. Attach the HWND handle to that specific index
            SendMessage((HWND)lParam, CB_SETITEMDATA, index, (LPARAM)hwnd);
        }
        return TRUE;
        }, (LPARAM)comboBox);
    SendMessage(comboBox, CB_SETCURSEL, 0, 0);
}

// Window Procedure for the "About" popup
// Window Procedure for the "About" popup
LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH hbrBkgnd = NULL;
    bool darkMode = IsDarkModeEnabled();

    switch (msg) {
    case WM_CREATE: {
        if (darkMode) {
            BOOL useDarkMode = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
            hbrBkgnd = CreateSolidBrush(RGB(32, 32, 32));
        }

        // --- Improved Spacing and Sizing ---
        int winW = 400; // Expected width for calculations

        // Software Info
        CreateWindowW(L"STATIC", L"WD Screen Translator\nVersion 1.0",
            WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 20, 360, 40, hwnd, NULL, NULL, NULL);

        // Instructions
        CreateWindowW(L"STATIC", L"How to use:\n1. Select your game from the list.\n2. Choose languages & colors.\n3. Click Start.\n4. Click Play on the floating bubble.",
            WS_VISIBLE | WS_CHILD, 20, 80, 360, 80, hwnd, NULL, NULL, NULL);

        // Settings Info (Moved down to prevent overlap)
        CreateWindowW(L"STATIC", L"Note: Scan rate is in milliseconds.\n(1000ms = 1 second) and 10000 = 10sec is more suitable",
            WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 170, 360, 40, hwnd, NULL, NULL, NULL);

        // Credits
        CreateWindowW(L"STATIC", L"Developed by & All © Reserved to Wardraon3399",
            WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 230, 360, 20, hwnd, NULL, NULL, NULL);

        // GitHub Button
        CreateWindowW(L"BUTTON", L"Visit GitHub",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 125, 260, 150, 40, hwnd, (HMENU)105, NULL, NULL);
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wp;
        if (darkMode) {
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkColor(hdcStatic, RGB(32, 32, 32));
            return (LRESULT)hbrBkgnd;
        }
        break;
    }

    case WM_COMMAND: {
        if (LOWORD(wp) == 105) {
            ShellExecuteW(0, L"open", L"https://github.com/WarDragon3399", 0, 0, SW_SHOWNORMAL);
        }
        break;
    }

    case WM_DESTROY: {
        if (hbrBkgnd) DeleteObject(hbrBkgnd);
        break;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Main Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH hbrBkgnd = NULL;

    switch (msg) {
    case WM_CREATE: {
        bool darkMode = IsDarkModeEnabled();
        hbrBkgnd = CreateSolidBrush(darkMode ? RGB(32, 32, 32) : GetSysColor(COLOR_WINDOW));

        int margin = 30;
        int fullWidth = 470; // Based on 550 window width minus margins

        // --- SECTION 1: GAME SELECTION ---
        CreateWindowW(L"STATIC", L"Target Game Window:", WS_VISIBLE | WS_CHILD, margin, 20, 300, 20, hwnd, NULL, NULL, NULL);
        g_hwndGameList = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            margin, 45, 400, 200, hwnd, NULL, NULL, NULL);

        // REFRESH BUTTON (↻)
        HWND hBtnRef = CreateWindowW(L"BUTTON", L"↻", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            margin + 410, 45, 40, 28, hwnd, (HMENU)107, NULL, NULL);

        // --- SECTION 2: LANGUAGES ---
        CreateWindowW(L"STATIC", L"Source Language:", WS_VISIBLE | WS_CHILD, margin, 90, 180, 20, hwnd, NULL, NULL, NULL);
        g_hwndSourceLang = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            margin, 115, 200, 200, hwnd, NULL, NULL, NULL);
        LanguageModule::PopulateSourceLanguages(g_hwndSourceLang);

        CreateWindowW(L"STATIC", L"Target Language:", WS_VISIBLE | WS_CHILD, margin + 250, 90, 180, 20, hwnd, NULL, NULL, NULL);
        g_hwndTargetLang = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            margin + 250, 115, 200, 200, hwnd, NULL, NULL, NULL);
        LanguageModule::PopulateTargetLanguages(g_hwndTargetLang);

        // --- SECTION 3: VISUALS ---
        CreateWindowW(L"STATIC", L"Font Color:", WS_VISIBLE | WS_CHILD, margin, 170, 180, 20, hwnd, NULL, NULL, NULL);
        g_hwndFontColor = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            margin, 195, 200, 200, hwnd, NULL, NULL, NULL);

        const wchar_t* colors[] = { L"White", L"Black", L"Red", L"Blue", L"Yellow", L"Cyan", L"Magenta" };
        for (auto c : colors) SendMessage(g_hwndFontColor, CB_ADDSTRING, 0, (LPARAM)c);
        SendMessage(g_hwndFontColor, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"STATIC", L"Shadow Color:", WS_VISIBLE | WS_CHILD, margin + 250, 170, 180, 20, hwnd, NULL, NULL, NULL);
        g_hwndShadowColor = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            margin + 250, 195, 200, 200, hwnd, NULL, NULL, NULL);

        const wchar_t* shadows[] = { L"Black", L"Dark Gray", L"White", L"Red", L"Blue" };
        for (auto s : shadows) SendMessage(g_hwndShadowColor, CB_ADDSTRING, 0, (LPARAM)s);
        SendMessage(g_hwndShadowColor, CB_SETCURSEL, 0, 0);

        // --- SECTION 4: SCAN RATE ---
        CreateWindowW(L"STATIC", L"Scan Rate (Milliseconds):", WS_VISIBLE | WS_CHILD, margin, 250, 250, 20, hwnd, NULL, NULL, NULL);
        g_hwndRefreshRate = CreateWindowW(L"EDIT", L"3000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            margin, 275, 120, 25, hwnd, NULL, NULL, NULL);

        // --- SECTION 5: ACTION BUTTONS ---
        CreateWindowW(L"BUTTON", L"START TRANSLATOR", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            margin, 350, fullWidth, 60, hwnd, (HMENU)102, NULL, NULL);

        CreateWindowW(L"BUTTON", L"About Info", WS_VISIBLE | WS_CHILD,
            margin, 430, 215, 35, hwnd, (HMENU)104, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Support Project", WS_VISIBLE | WS_CHILD,
            margin + 255, 430, 215, 35, hwnd, (HMENU)106, NULL, NULL);

        RefreshWindowList(g_hwndGameList);
        break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case 107: // Refresh
            RefreshWindowList(g_hwndGameList);
            break;

        case 102: { // START
            int gameIdx = (int)SendMessage(g_hwndGameList, CB_GETCURSEL, 0, 0);
            if (gameIdx == CB_ERR) {
                MessageBoxW(hwnd, L"Please select a game from the list.", L"Error", MB_OK | MB_ICONWARNING);
                break;
            }
           HWND selectedGameHwnd = (HWND)SendMessage(g_hwndGameList, CB_GETITEMDATA, gameIdx, 0);

            if (!IsWindow(selectedGameHwnd)) {
                MessageBoxW(hwnd, L"Please select a valid game window.", L"Error", MB_OK | MB_ICONWARNING);
                // Optional: Refresh the list automatically if the window is gone
                RefreshWindowList(g_hwndGameList);
                break;
            }

            // Capture Colors & Rate
            int fIdx = (int)SendMessage(g_hwndFontColor, CB_GETCURSEL, 0, 0);
            COLORREF fontColors[] = { RGB(255,255,255), RGB(0,0,0), RGB(255,0,0), RGB(0,0,255), RGB(255,255,0), RGB(0,255,255), RGB(255,0,255) };
            g_SelectedFontColor = fontColors[fIdx];

            int sIdx = (int)SendMessage(g_hwndShadowColor, CB_GETCURSEL, 0, 0);
            COLORREF shadowColors[] = { RGB(0,0,0), RGB(64,64,64), RGB(255,255,255), RGB(255,0,0), RGB(0,0,255) };
            g_SelectedShadowColor = shadowColors[sIdx];

            wchar_t rateBuf[10];
            GetWindowTextW(g_hwndRefreshRate, rateBuf, 10);
            g_OCRRefreshRate = _wtoi(rateBuf);
            if (g_OCRRefreshRate < 500) g_OCRRefreshRate = 500;

            // Execute
            int srcIdx = (int)SendMessage(g_hwndSourceLang, CB_GETCURSEL, 0, 0);
            int tgtIdx = (int)SendMessage(g_hwndTargetLang, CB_GETCURSEL, 0, 0);

            std::wstring srcTag = LanguageModule::GetSteamLanguages()[srcIdx].ocrTag;
            std::wstring tgtCode = LanguageModule::GetGoogleLanguages()[tgtIdx].code;

            if (LanguageModule::IsLanguagePackInstalled(srcTag)) {
                ShowWindow(hwnd, SW_HIDE);
                BubbleModule::g_targetGameHwnd = selectedGameHwnd;
                BubbleModule::g_sourceCode = srcTag;
                BubbleModule::g_targetCode = tgtCode;
                OverlayModule::CreateOverlay(GetModuleHandle(NULL), selectedGameHwnd);
                BubbleModule::CreateTranslatorBubble(GetModuleHandle(NULL), hwnd, srcTag, tgtCode);
            }
            break;
        }

        case 104: { // ABOUT WINDOW (Fixed ID)
            static bool registered = false;
            if (!registered) {
                HICON hAboutIcon = (HICON)LoadImageW(NULL, L"WD.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
                WNDCLASSEXW ax = { sizeof(WNDCLASSEXW) };
                ax.lpfnWndProc = AboutProc;
                ax.hInstance = GetModuleHandle(NULL);
                ax.lpszClassName = L"WDAboutWin";
                ax.hIcon = hAboutIcon;   // <--- Add this for Taskbar
                ax.hIconSm = hAboutIcon; // <--- Add this for Title Bar
                ax.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                RegisterClassExW(&ax);
                registered = true;
            }
            // Create About window as a POPUP so it stays on top of the main UI
            CreateWindowExW(WS_EX_TOPMOST, L"WDAboutWin", L"About Software",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, 415, 350, hwnd, NULL, GetModuleHandle(NULL), NULL);
            break;
        }

        case 106: // Donation
            ShellExecuteW(0, L"open", L"https://www.patreon.com/cw/Wardragon3399", 0, 0, SW_SHOWNORMAL);
            break;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wp;
        if (IsDarkModeEnabled()) {
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)hbrBkgnd;
        }
        else {
            SetTextColor(hdcStatic, RGB(0, 0, 0));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
    }

    case WM_SIZE: {
        int newWidth = LOWORD(lp);
        int newHeight = HIWORD(lp);
        int margin = 30;
        int controlWidth = newWidth - (margin * 2);

        // Resize the Start Button to always span the full width
        MoveWindow(GetDlgItem(hwnd, 102), margin, 350, controlWidth, 60, TRUE);

        // Reposition the Bottom Buttons
        int halfBtnWidth = (controlWidth / 2) - 10;
        MoveWindow(GetDlgItem(hwnd, 104), margin, 430, halfBtnWidth, 35, TRUE); // About
        MoveWindow(GetDlgItem(hwnd, 106), margin + halfBtnWidth + 20, 430, halfBtnWidth, 35, TRUE); // Donate

        // Resize Game List and move Refresh button
        MoveWindow(g_hwndGameList, margin, 45, controlWidth - 50, 200, TRUE);
        MoveWindow(GetDlgItem(hwnd, 107), margin + controlWidth - 45, 45, 40, 28, TRUE);

        return 0;
    }

    case WM_DESTROY: {
        if (hbrBkgnd) DeleteObject(hbrBkgnd);
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Get the path of the current EXE to find the icon in the same folder
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring pathStr = exePath;
    size_t lastSlash = pathStr.find_last_of(L"\\/");
    std::wstring iconPath = pathStr.substr(0, lastSlash + 1) + L"WD.ico";
    
    // 1. Initialize COM/WinRT
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    // 2. DPI Awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool darkMode = IsDarkModeEnabled();
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101));
    if (!hIcon) {
        // Optional: Debug message if icon fails to load
        OutputDebugStringW(L"[WD] Icon failed to load. Check if app_icon.ico is in the EXE folder.\n");
    }

    // 3. Register and Create Main Window
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TranslatorMain";
    wc.hIcon = hIcon;         // Taskbar Icon
    wc.hIconSm = hIcon;       // Title Bar Icon
    wc.hbrBackground = darkMode ? CreateSolidBrush(RGB(32, 32, 32)) : (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hwndMain = CreateWindowExW(0, L"TranslatorMain", L"WD Screen Translator",
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 650, NULL, NULL, hInst, NULL);
    // Inside your initialization:
    BOOL value = TRUE;
    // This makes the Title Bar dark (Windows 10 1903+ / Windows 11)
    DwmSetWindowAttribute(g_hwndMain, 20, &value, sizeof(value));

    if (!g_hwndMain) return 0;



    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    // 4. THE CRITICAL MESSAGE LOOP
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}