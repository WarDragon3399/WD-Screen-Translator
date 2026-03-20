#include <windows.h>
#include <shellapi.h>
#include <string>
#include "LanguageModule.h"
#include "BubbleModule.h"

// Global Handles
HWND g_hwndMain;
HWND g_hwndGameList, g_hwndSourceLang, g_hwndTargetLang;

// Function to refresh the list of active windows
void RefreshWindowList(HWND comboBox) {
    SendMessage(comboBox, CB_RESETCONTENT, 0, 0);
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
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
LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        CreateWindowW(L"STATIC", L"WD Screen Translator\nVersion 1.0.0\n\nDeveloped by Parthkumar",
            WS_VISIBLE | WS_CHILD | SS_CENTER, 10, 40, 260, 100, hwnd, NULL, NULL, NULL);
    }
    if (msg == WM_CLOSE) DestroyWindow(hwnd);
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Main Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;

        // --- TOP BUTTONS ---
        CreateWindowW(L"BUTTON", L"Donation", WS_VISIBLE | WS_CHILD, width - 100, 10, 85, 28, hwnd, (HMENU)202, NULL, NULL);
        CreateWindowW(L"BUTTON", L"About", WS_VISIBLE | WS_CHILD, width - 195, 10, 85, 28, hwnd, (HMENU)201, NULL, NULL);

        // --- GAME SELECTION ---
        CreateWindowW(L"STATIC", L"Select Active Window / Game:", WS_VISIBLE | WS_CHILD, 30, 60, 250, 20, hwnd, NULL, NULL, NULL);
        g_hwndGameList = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 30, 85, 330, 300, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Refresh", WS_VISIBLE | WS_CHILD, 370, 83, 90, 28, hwnd, (HMENU)101, NULL, NULL);

        // --- LANGUAGES ---
        CreateWindowW(L"STATIC", L"Source (OCR):", WS_VISIBLE | WS_CHILD, 30, 135, 200, 20, hwnd, NULL, NULL, NULL);
        g_hwndSourceLang = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 30, 160, 205, 200, hwnd, NULL, NULL, NULL);
        LanguageModule::PopulateSourceLanguages(g_hwndSourceLang);

        CreateWindowW(L"STATIC", L"Target (Translate):", WS_VISIBLE | WS_CHILD, 255, 135, 200, 20, hwnd, NULL, NULL, NULL);
        g_hwndTargetLang = CreateWindowW(L"COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 255, 160, 205, 200, hwnd, NULL, NULL, NULL);
        LanguageModule::PopulateTargetLanguages(g_hwndTargetLang);

        // --- START BUTTON ---
        CreateWindowW(L"BUTTON", L"START TRANSLATOR", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 230, 430, 50, hwnd, (HMENU)102, NULL, NULL);

        RefreshWindowList(g_hwndGameList);
        break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case 101: { // Refresh
            RefreshWindowList(g_hwndGameList);
            break;
        }

        case 102: { // START TRANSLATOR
            int gameIdx = (int)SendMessage(g_hwndGameList, CB_GETCURSEL, 0, 0);
            HWND selectedGameHwnd = (HWND)SendMessage(g_hwndGameList, CB_GETITEMDATA, gameIdx, 0);

            if (!IsWindow(selectedGameHwnd)) {
                MessageBoxW(hwnd, L"Invalid Window Selected. Please Refresh.", L"Error", MB_OK);
                break; // Exit case 102
            }

            int srcIdx = (int)SendMessage(g_hwndSourceLang, CB_GETCURSEL, 0, 0);
            int tgtIdx = (int)SendMessage(g_hwndTargetLang, CB_GETCURSEL, 0, 0);

            if (srcIdx != CB_ERR && tgtIdx != CB_ERR) {
                std::wstring srcTag = LanguageModule::GetSteamLanguages()[srcIdx].ocrTag;
                std::wstring tgtCode = LanguageModule::GetGoogleLanguages()[tgtIdx].code;
                
                if (LanguageModule::IsLanguagePackInstalled(srcTag)) {
                    ShowWindow(hwnd, SW_HIDE);
                    BubbleModule::g_sourceCode = LanguageModule::GetSteamLanguages()[srcIdx].ocrTag;
                    BubbleModule::g_targetCode = LanguageModule::GetGoogleLanguages()[tgtIdx].code;
                    BubbleModule::g_targetGameHwnd = selectedGameHwnd;
                    BubbleModule::CreateTranslatorBubble(GetModuleHandle(NULL), hwnd, srcTag, tgtCode);
                }
                else {
                    MessageBoxW(hwnd, L"OCR Pack not found!", L"Error", MB_OK);
                }
            }
            break; // <--- CRITICAL: Stops it from going to case 201
        }

        case 201: { // About
            static bool reg = false;
            if (!reg) {
                WNDCLASSW ac = { 0 };
                ac.lpfnWndProc = AboutProc;
                ac.hInstance = GetModuleHandle(NULL);
                ac.lpszClassName = L"AboutWin";
                ac.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                ac.hCursor = LoadCursor(NULL, IDC_ARROW);
                RegisterClassW(&ac);
                reg = true;
            }
            CreateWindowExW(WS_EX_TOPMOST, L"AboutWin", L"About Software",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, hwnd, NULL, GetModuleHandle(NULL), NULL);
            break;
        }

        case 202: { // Donation
            ShellExecuteW(0, L"open", L"https://www.patreon.com/", 0, 0, SW_SHOWNORMAL);
            break;
        }
        } // End Switch LOWORD(wp)
        break; // End WM_COMMAND
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    winrt::init_apartment();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TranslatorMain";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hwndMain = CreateWindowExW(0, L"TranslatorMain", L"WD Screen Translator",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 350, NULL, NULL, hInst, NULL);

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}