#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include "Common.h" // Include the shared struct

// Access the globals from Main.cpp
extern COLORREF g_SelectedFontColor;
extern COLORREF g_SelectedShadowColor;

namespace OverlayModule {
    inline HWND g_hwndOverlay = NULL;
    inline std::vector<TextBlock> g_currentBlocks;

    LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 1. Clear background
            HBRUSH hBase = CreateSolidBrush(RGB(1, 1, 1)); // Use 1,1,1 as transparent key
            FillRect(hdc, &ps.rcPaint, hBase);
            DeleteObject(hBase);
            HFONT hFont = CreateFontW(-20, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
            SelectObject(hdc, hFont);

            // 2. Draw blocks
            for (const auto& block : g_currentBlocks) {
                // Background for text
               /* HBRUSH hBg = CreateSolidBrush(RGB(20, 20, 20));
                FillRect(hdc, &block.box, hBg);
                DeleteObject(hBg);*/

                // Border
               /* HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
                SelectObject(hdc, hPen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                // FIXED: Rectangle takes 5 arguments
                Rectangle(hdc, block.box.left, block.box.top, block.box.right, block.box.bottom);
                DeleteObject(hPen);*/

                // Text
                RECT r = block.box;
               
                SetBkMode(hdc, TRANSPARENT);
                // FIXED: DrawTextW takes 5 arguments
                // --- 1. DRAW THE SHADOW ---
                SetTextColor(hdc, g_SelectedShadowColor);
                RECT shadowRect = r;
                //OffsetRect(&shadowRect, 2, 2); // 2-pixel offset
                DrawTextW(hdc, block.text.c_str(), -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);

                // --- 2. DRAW THE MAIN TEXT (This was missing!) ---
                SetTextColor(hdc, g_SelectedFontColor); // <--- THIS LINE FIXES IT
                DrawTextW(hdc, block.text.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);
               
               
                shadowRect = r; OffsetRect(&shadowRect, 1, 1);
                DrawTextW(hdc, block.text.c_str(), -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);

                shadowRect = r; OffsetRect(&shadowRect, -1, -1);
                DrawTextW(hdc, block.text.c_str(), -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);

                shadowRect = r; OffsetRect(&shadowRect, 1, -1);
                DrawTextW(hdc, block.text.c_str(), -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);

                shadowRect = r; OffsetRect(&shadowRect, -1, 1);
                DrawTextW(hdc, block.text.c_str(), -1, &shadowRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);
               
              
            }

            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_DESTROY: { g_hwndOverlay = NULL; return 0; }
        default: return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    inline void CreateOverlay(HINSTANCE hInst, HWND gameHwnd) {
        if (g_hwndOverlay && IsWindow(g_hwndOverlay)) return;

        WNDCLASSEXW oc = { sizeof(WNDCLASSEXW) };
        oc.lpfnWndProc = OverlayProc;
        oc.hInstance = hInst;
        oc.lpszClassName = L"WDOverlayClass";
        oc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExW(&oc);

        // Get the SCREEN coordinates of the game's actual image area
        POINT pt = { 0, 0 };
        ClientToScreen(gameHwnd, &pt);

        RECT rc,cc;
        GetWindowRect(gameHwnd, &rc);
        GetClientRect(gameHwnd, &cc);

        // Calculate the thickness of the title bar and borders
        int border_thick = ((rc.right - rc.left) - cc.right) / 2;
        int title_height = ((rc.bottom - rc.top) - cc.bottom) - border_thick;

        g_hwndOverlay = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"WDOverlayClass", NULL, WS_POPUP,
            pt.x, pt.y, rc.right, rc.bottom, 
            NULL, NULL, hInst, NULL
        );

        SetWindowDisplayAffinity(g_hwndOverlay, WDA_EXCLUDEFROMCAPTURE);

        SetLayeredWindowAttributes(g_hwndOverlay, RGB(1, 1, 1), 0, LWA_COLORKEY);
        ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
    }
    inline void UpdateOverlay(const std::vector<TextBlock>& newBlocks) {
        // 1. Create a local copy we can modify
        std::vector<TextBlock> stableBlocks = newBlocks;

        if (g_currentBlocks.empty()) {
            g_currentBlocks = stableBlocks;
        }
        else {
            // 2. STABILITY CHECK: Remove 'const' so we can modify 'newB'
            for (auto& newB : stableBlocks) {
                for (const auto& oldB : g_currentBlocks) {
                    // Calculate center points
                    int oldCX = (oldB.box.left + oldB.box.right) / 2;
                    int oldCY = (oldB.box.top + oldB.box.bottom) / 2;
                    int newCX = (newB.box.left + newB.box.right) / 2;
                    int newCY = (newB.box.top + newB.box.bottom) / 2;

                    int dist = abs(newCX - oldCX) + abs(newCY - oldCY);

                    // If movement is tiny (less than 20 pixels), snap to the old position
                    if (dist < 15) {
                        newB.box = oldB.box;
                        break;
                    }
                }
            }
            g_currentBlocks = stableBlocks;
        }

        if (g_hwndOverlay) {
            InvalidateRect(g_hwndOverlay, NULL, TRUE);
            UpdateWindow(g_hwndOverlay);
        }
    }

    inline void UpdateText(std::wstring manualText, RECT area) {
        TextBlock b;
        b.text = manualText;
        b.box = area;
        g_currentBlocks.clear();
        g_currentBlocks.push_back(b);
        if (g_hwndOverlay) InvalidateRect(g_hwndOverlay, NULL, TRUE);
    }
}