#pragma once
#include <windows.h>
#include <string>

namespace OverlayModule {
    inline HWND g_hwndOverlay = NULL;
    inline std::wstring g_currentTranslation = L"WAITING FOR SCAN...";
    inline RECT g_textRect = { 20, 20, 800, 100 };
    std::vector<TextBlock> g_currentBlocks;

    LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 1. Draw the RED BOX (Debug Background)
            if (!g_currentBlocks.empty()) {
                for (const auto& block : g_currentBlocks) {
                    // Calculate height (with a minimum of 18 for readability)
                    int fontHeight = (block.box.bottom - block.box.top);
                    if (fontHeight < 18) fontHeight = 22;

                    HFONT hFont = CreateFontW(fontHeight, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Nirmala UI");
                    SelectObject(hdc, hFont);

                    // Style: Black background box with White text (High Contrast)
                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkMode(hdc, OPAQUE);
                    SetBkColor(hdc, RGB(0, 0, 0));

                    DrawTextW(hdc, block.text.c_str(), -1, (RECT*)&block.box, DT_LEFT | DT_TOP | DT_WORDBREAK);

                    DeleteObject(hFont);
                }
            }
            else {
                // Only show this if no blocks exist yet
                SetTextColor(hdc, RGB(255, 255, 0)); // Yellow for status
                SetBkMode(hdc, TRANSPARENT);
                TextOutW(hdc, 20, 20, g_currentTranslation.c_str(), (int)g_currentTranslation.length());
            }

            EndPaint(hwnd, &ps);
            return 0;
            return 0;

            for (const auto& block : g_currentBlocks) {

                // 1. Calculate Font Size based on the Japanese box height
                int fontHeight = block.box.bottom - block.box.top;
                HFONT hFont = CreateFontW(fontHeight, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Nirmala UI");
                SelectObject(hdc, hFont);

                // 2. Set Colors (White text with black background for readability)
                SetTextColor(hdc, RGB(0, 0, 0)); // Black
                SetBkMode(hdc, TRANSPARENT);
                RECT shadowRect = { block.box.left + 2, block.box.top + 2, block.box.right + 2, block.box.bottom + 2 };
                DrawTextW(hdc, block.text.c_str(), -1, &shadowRect, DT_LEFT | DT_TOP | DT_WORDBREAK);

                // 3. Draw the translated text exactly in the box
                SetTextColor(hdc, RGB(255, 255, 255)); // White
                DrawTextW(hdc, block.text.c_str(), -1, (RECT*)&block.box, DT_LEFT | DT_TOP | DT_WORDBREAK);

                DeleteObject(hFont);
            }
            EndPaint(hwnd, &ps);
        }
        case WM_ERASEBKGND: return 1; // Prevent flickering
        default: return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    inline void CreateOverlay(HINSTANCE hInst, HWND targetGame) {
        if (g_hwndOverlay && IsWindow(g_hwndOverlay)) return;

        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW oc = { sizeof(WNDCLASSEXW) };
            oc.lpfnWndProc = OverlayProc;
            oc.hInstance = hInst;
            oc.lpszClassName = L"WDOverlay";
            oc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            if (!RegisterClassExW(&oc)) {
                OutputDebugStringW(L"[WD] ERROR: Failed to register Overlay Class!\n");
                return;
            }
            registered = true;
        }

        RECT rect;
        GetWindowRect(targetGame, &rect);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        // Use WS_EX_LAYERED but NOT WS_EX_TRANSPARENT for now 
        // (Transparent makes it click-through, but sometimes hard to see during debug)
        g_hwndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            L"WDOverlay", L"", WS_POPUP,
            rect.left, rect.top, w, h,
            NULL, NULL, hInst, NULL);

        if (!g_hwndOverlay) {
            OutputDebugStringW(L"[WD] ERROR: Failed to create Overlay Window!\n");
            return;
        }

        // Set to Semi-Transparent Red (Alpha 180)
        SetLayeredWindowAttributes(g_hwndOverlay, 0, 100, LWA_ALPHA);

        ShowWindow(g_hwndOverlay, SW_SHOW);
        UpdateWindow(g_hwndOverlay);
        OutputDebugStringW(L"[WD] Overlay Created Successfully!\n");
    }

    inline void UpdateText(std::wstring text, RECT position) {
        g_currentTranslation = text;
        g_textRect = position;
        if (g_hwndOverlay) {
            InvalidateRect(g_hwndOverlay, NULL, TRUE);
        }
    }

    void UpdateOverlay(const std::vector<TextBlock>& newBlocks) {
        g_currentBlocks = newBlocks;
        InvalidateRect(g_hwndOverlay, NULL, TRUE); // Force Redraw
    }
}