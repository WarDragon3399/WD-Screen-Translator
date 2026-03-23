#pragma once
#include <windows.h>
#include <string>
#include "OverlayModule.h"
#include "TranslationModule.h"
#include <algorithm>

extern int g_OCRRefreshRate; // Link to the variable in Main.cpp
extern HWND g_hwndRefreshRate;
inline HBRUSH g_hBubbleBrush = CreateSolidBrush(RGB(38, 38, 38));

namespace BubbleModule {
    inline HWND g_hwndBubble = NULL;
    inline HWND g_hwndMainRef = NULL;
    inline HWND g_targetGameHwnd = NULL;
    inline HWND g_hwndRateLabel = NULL;
    inline HWND g_hwndRateEdit = NULL;
    inline bool g_isUpdatingRateUI = false;
    inline std::wstring g_sourceCode;
    inline std::wstring g_targetCode;
    inline bool g_isScanning = false;
    inline std::wstring g_lastMegaString = L"";


    inline bool IsTextSimilar(const std::wstring& s1, const std::wstring& s2) {
        if (s1 == s2) return true;
        if (s1.length() != s2.length()) return false;

        // If the length is wildly different, it's definitely new text
        if (abs((int)s1.length() - (int)s2.length()) > 5) return false;

        // Count how many characters match
        int matches = 0;
        size_t len = (std::min)(s1.length(), s2.length());
        for (size_t i = 0; i < len; i++) {
            if (s1[i] == s2[i]) matches++;
        }

        float similarity = (float)matches / (float)s1.length();
        return (similarity > 0.90f); // 90% match threshold
    }

    const int B_WIDTH = 300;
    const int B_HEIGHT = 50;

    inline void MirrorRateTextToMainWindow() {
        if (g_isUpdatingRateUI) return;
        if (!g_hwndRateEdit || !IsWindow(g_hwndRateEdit)) return;
        if (!g_hwndRefreshRate || !IsWindow(g_hwndRefreshRate)) return;

        wchar_t rateBuf[16] = {};
        GetWindowTextW(g_hwndRateEdit, rateBuf, 16);

        g_isUpdatingRateUI = true;
        SetWindowTextW(g_hwndRefreshRate, rateBuf);
        g_isUpdatingRateUI = false;
    }

    inline void ApplyRateFromFloatingInput() {
        if (!g_hwndRateEdit || !IsWindow(g_hwndRateEdit)) return;

        wchar_t rateBuf[16] = {};
        GetWindowTextW(g_hwndRateEdit, rateBuf, 16);

        int newRate = _wtoi(rateBuf);
        if (newRate < 500) newRate = 500;

        g_OCRRefreshRate = newRate;

        wchar_t finalBuf[16];
        wsprintfW(finalBuf, L"%d", g_OCRRefreshRate);

        g_isUpdatingRateUI = true;

        // Normalize bubble field (example: user typed 300 -> becomes 500)
        SetWindowTextW(g_hwndRateEdit, finalBuf);

        // Keep main window field synced too
        if (g_hwndRefreshRate && IsWindow(g_hwndRefreshRate)) {
            SetWindowTextW(g_hwndRefreshRate, finalBuf);
        }

        g_isUpdatingRateUI = false;
    }

    LRESULT CALLBACK BubbleProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lp);

            if (x > 5 && x < 38) { // Play/Pause Button
                g_isScanning = !g_isScanning;

                if (g_isScanning) {
                    // STARTING
                    ApplyRateFromFloatingInput(); // Apply/clamp OCR rate only when Play is pressed
                    SetTimer(hwnd, 1, g_OCRRefreshRate, NULL);
                    PostMessage(hwnd, WM_TIMER, 1, 0);

                    if (OverlayModule::g_hwndOverlay) {
                        ShowWindow(OverlayModule::g_hwndOverlay, SW_SHOW);
                    }
                }
                else {
                    // PAUSING
                    g_isScanning = false;
                    g_lastMegaString = L""; // Reset cache on pause
                    KillTimer(hwnd, 1);
                    if (OverlayModule::g_hwndOverlay) {
                        // We must HIDE the red box so it doesn't stay stuck on screen
                        ShowWindow(OverlayModule::g_hwndOverlay, SW_HIDE);
                    }
                }

                // Force the bubble to redraw so the icon changes (from II to ▶)
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            else if (x > 205 && x < 240) {
                KillTimer(hwnd, 1);
                if (OverlayModule::g_hwndOverlay) ShowWindow(OverlayModule::g_hwndOverlay, SW_HIDE);
                ShowWindow(g_hwndMainRef, SW_SHOW);
                DestroyWindow(hwnd);
                return 0;
            }
            else if (x > 250 && x < 290) {
                PostQuitMessage(0);
                return 0;
            }
            else {
                PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
        }
        
        case WM_COMMAND: {
            if (LOWORD(wp) == 2001 && HIWORD(wp) == EN_CHANGE) {
                if (g_isUpdatingRateUI) return 0;

                // Only mirror text to main window while typing.
                // DO NOT apply OCR timer changes here.
                MirrorRateTextToMainWindow();
                return 0;
            }
            break;
        }

        case WM_TIMER: {

            if (g_isScanning && g_targetGameHwnd) {

                // 1. Position the Overlay precisely over the Client (Content) Area
                if (!IsWindow(g_targetGameHwnd)) {
                    OverlayModule::UpdateText(L"Error: Game Window Lost", { 50, 50, 500, 100 });
                    g_isScanning = false;
                    KillTimer(hwnd, 1);
                    return 0;
                }

                POINT pt = { 0, 0 };
                ClientToScreen(g_targetGameHwnd, &pt); // Get the screen X,Y of the game content

                RECT clientRc;
                if (GetClientRect(g_targetGameHwnd, &clientRc)) {
                    int w = clientRc.right - clientRc.left;
                    int h = clientRc.bottom - clientRc.top;

                    if (!OverlayModule::g_hwndOverlay) {
                        OverlayModule::CreateOverlay(GetModuleHandle(NULL), g_targetGameHwnd);
                    }

                    // Move and Resize the overlay to match the inner game window exactly
                    SetWindowPos(OverlayModule::g_hwndOverlay, HWND_TOPMOST,
                        pt.x, pt.y, w, h,
                        SWP_NOACTIVATE | SWP_SHOWWINDOW);
                }

                if (g_targetGameHwnd == NULL) {
                    OutputDebugStringW(L"[WD] ERROR: Target Game HWND is NULL!\n");
                }

                // 1. Hide the overlay so OCR doesn't "see" its own translation
                if (OverlayModule::g_hwndOverlay) {
                    ShowWindow(OverlayModule::g_hwndOverlay, SW_HIDE);
                }

                // 2. Small delay (20ms) to ensure the window is gone from the desktop buffer
                Sleep(20);

                // 2. RUN OCR
                LanguageModule::RecognizeText(g_targetGameHwnd, g_sourceCode, [=](std::vector<TextBlock> blocks) {
                    if (blocks.empty()) {
                        if (g_lastMegaString != L"") {
                            g_lastMegaString = L"";
                            OverlayModule::UpdateText(L"Looking for text...", { 0, 0, 0, 0 });
                        }
                        return;
                    }
                    else {

                        // If no text was found, we still show the overlay (it might be empty/clear)
                        if (OverlayModule::g_hwndOverlay) {
                            ShowWindow(OverlayModule::g_hwndOverlay, SW_SHOWNOACTIVATE);

                        }
                    }
                    std::vector<TextBlock> translatedBlocks;
                    std::wstring currentMegaString = L"";
                    for (auto& b : blocks) {
                        // FILTER: Ignore very short text (usually noise/icons)
                        if (b.text.length() < 2) continue;

                        // FILTER: Ignore pure numbers (like HP/MP or Money)
                        if (std::all_of(b.text.begin(), b.text.end(), iswdigit)) continue;

                        std::wstring translated = TranslationModule::GetTranslatedText(b.text, g_sourceCode, g_targetCode);

                        if (!translated.empty() && translated != b.text) {
                            b.text = translated;
                            translatedBlocks.push_back(b);
                        }

                        if (!translatedBlocks.empty()) {
                            OverlayModule::UpdateOverlay(translatedBlocks);
                        }
                    }

                    if (IsTextSimilar(currentMegaString, g_lastMegaString)) return;
                    g_lastMegaString = currentMegaString;

                    // One Batch Translation
                    std::wstring input = L"";
                    for (size_t i = 0; i < blocks.size(); i++)
                        input += blocks[i].text + (i == blocks.size() - 1 ? L"" : L" || ");

                    std::wstring translated = TranslationModule::GetTranslatedText(input, g_sourceCode, g_targetCode);

                    // Split results back correctly
                    std::wstringstream ss(translated);
                    std::wstring segment;
                    size_t idx = 0;
                    while (std::getline(ss, segment, L'|') && idx < blocks.size()) {
                        // Remove the pipe or separators Google might have left
                        segment.erase(std::remove(segment.begin(), segment.end(), L'|'), segment.end());
                        if (!segment.empty() && segment[0] == L' ') segment.erase(0, 1);

                        blocks[idx].text = segment;
                        idx++;
                    }

                    OverlayModule::UpdateOverlay(blocks);
                    });
            }

            if (!g_isScanning) {
                KillTimer(hwnd, 1);
                return 0;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
            HPEN borderPen = CreatePen(PS_SOLID, 2, g_isScanning ? RGB(0, 255, 120) : RGB(120, 120, 120));
            SelectObject(hdc, bgBrush);
            SelectObject(hdc, borderPen);
            RoundRect(hdc, 0, 0, B_WIDTH, B_HEIGHT, 25, 25);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            HFONT hFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Symbol");
            SelectObject(hdc, hFont);

            TextOutW(hdc, 14, 12, g_isScanning ? L"II" : L"▶", (g_isScanning ? 2 : 1));
            TextOutW(hdc, 212, 12, L"🏠", 2);
            TextOutW(hdc, 262, 12, L"✕", 1);

            DeleteObject(bgBrush);
            DeleteObject(borderPen);
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, RGB(235, 235, 235));
            SetBkColor(hdc, RGB(38, 38, 38));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)g_hBubbleBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkColor(hdc, RGB(52, 52, 52));
            return (LRESULT)g_hBubbleBrush;
        }

        case WM_DESTROY: {
            g_hwndBubble = NULL;
            if (OverlayModule::g_hwndOverlay) {
                DestroyWindow(OverlayModule::g_hwndOverlay);
                OverlayModule::g_hwndOverlay = NULL;
            }
            return 0;
        }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    inline void CreateTranslatorBubble(HINSTANCE hInst, HWND mainHwnd, std::wstring src, std::wstring tgt) {
        g_hwndMainRef = mainHwnd;
        g_sourceCode = src;
        g_targetCode = tgt;
        g_isScanning = false;

        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW bc = { sizeof(WNDCLASSEXW) };
            bc.lpfnWndProc = BubbleProc;
            bc.hInstance = hInst;
            bc.lpszClassName = L"WDBubbleDock";
            bc.hCursor = LoadCursor(NULL, IDC_HAND);
            RegisterClassExW(&bc);
            registered = true;
        }

        g_hwndBubble = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"WDBubbleDock", L"WD_Dock", WS_POPUP | WS_VISIBLE,
            100, 100, B_WIDTH, B_HEIGHT, NULL, NULL, hInst, NULL);

        wchar_t rateBuf[16];
        wsprintfW(rateBuf, L"%d", g_OCRRefreshRate);

        g_hwndRateLabel = CreateWindowW(
            L"STATIC",
            L"Rate",
            WS_CHILD | WS_VISIBLE,
            38, 14, 34, 20,
            g_hwndBubble,
            NULL,
            hInst,
            NULL
        );

        g_hwndRateEdit = CreateWindowExW(
            0,
            L"EDIT",
            rateBuf,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
            80, 10, 62, 28,
            g_hwndBubble,
            (HMENU)2001,
            hInst,
            NULL
        );

        HWND hwndRateMs = CreateWindowW(
            L"STATIC",
            L"ms",
            WS_CHILD | WS_VISIBLE,
            143, 14, 22, 20,
            g_hwndBubble,
            NULL,
            hInst,
            NULL
        );

        SetLayeredWindowAttributes(g_hwndBubble, 0, 220, LWA_ALPHA);
    }


}