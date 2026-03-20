#pragma once
#include <windows.h>
#include <string>
#include "OverlayModule.h"
#include "TranslationModule.h"

namespace BubbleModule {
    inline HWND g_hwndBubble = NULL;
    inline HWND g_hwndMainRef = NULL;
    inline HWND g_targetGameHwnd = NULL;
    inline std::wstring g_sourceCode;
    inline std::wstring g_targetCode;
    inline bool g_isScanning = false;

    const int B_WIDTH = 160;
    const int B_HEIGHT = 50;

    LRESULT CALLBACK BubbleProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lp);
            if (x > 5 && x < 55) { // Play/Pause Button
                g_isScanning = !g_isScanning;

                if (g_isScanning) {
                    // STARTING
                    SetTimer(hwnd, 1, 3000, NULL);
                    if (OverlayModule::g_hwndOverlay) {
                        ShowWindow(OverlayModule::g_hwndOverlay, SW_SHOW);
                    }
                }
                else {
                    // PAUSING
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
            else if (x > 60 && x < 105) {
                KillTimer(hwnd, 1);
                if (OverlayModule::g_hwndOverlay) ShowWindow(OverlayModule::g_hwndOverlay, SW_HIDE);
                ShowWindow(g_hwndMainRef, SW_SHOW);
                DestroyWindow(hwnd);
                return 0;
            }
            else if (x > 110 && x < 155) {
                PostQuitMessage(0);
                return 0;
            }
            else {
                PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
        }

        case WM_TIMER: {
            if (g_isScanning && g_targetGameHwnd) {
                // 1. Position the Overlay
                RECT gameRect;
                if (GetWindowRect(g_targetGameHwnd, &gameRect)) {
                    if (!OverlayModule::g_hwndOverlay) {
                        OverlayModule::CreateOverlay(GetModuleHandle(NULL), g_targetGameHwnd);
                    }

                    SetWindowPos(OverlayModule::g_hwndOverlay, g_hwndBubble,
                        gameRect.left, gameRect.top, gameRect.right - gameRect.left, gameRect.bottom - gameRect.top,
                        SWP_NOACTIVATE | SWP_SHOWWINDOW);
                }

                // 2. RUN OCR with Callback
                // We pass a lambda [] as the third argument to handle the results
                LanguageModule::RecognizeText(g_targetGameHwnd, g_sourceCode, [=](std::vector<TextBlock> blocks) {
                    if (!blocks.empty()) {
                        // Translate each block
                        for (TextBlock& block : blocks) {
                            if (!block.text.empty()) {
                                block.text = TranslationModule::GetTranslatedText(block.text, g_sourceCode, g_targetCode);
                            }
                        }
                        // Send to overlay
                        OverlayModule::UpdateOverlay(blocks);
                    }
                    else {
                        // Only show this if OCR literally found nothing
                        OverlayModule::UpdateText(L"Looking for text...", { 50, 50, 500, 100 });
                    }
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

            TextOutW(hdc, 22, 12, g_isScanning ? L"II" : L"▶", (g_isScanning ? 2 : 1));
            TextOutW(hdc, 72, 12, L"🏠", 2);
            TextOutW(hdc, 125, 12, L"✕", 1);

            DeleteObject(bgBrush);
            DeleteObject(borderPen);
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
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

        SetLayeredWindowAttributes(g_hwndBubble, 0, 220, LWA_ALPHA);
    }
}