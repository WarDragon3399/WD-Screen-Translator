#include <windows.h>
#include <vector>
#include <string>
#include <shellapi.h>
#include <functional>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include "Common.h"

// WinRT Headers
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment(lib, "windowsapp")

using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

struct LanguageOption {
    std::wstring displayName;
    std::wstring ocrTag;
};

struct TargetLanguage {
    std::wstring name;
    std::wstring code;
};



namespace LanguageModule {
    inline const std::vector<LanguageOption>& GetSteamLanguages() {
        static const std::vector<LanguageOption> sources = {
           { L"Japanese", L"ja-JP" }, { L"English", L"en-US" },
           { L"Korean", L"ko-KR" }, { L"Chinese (Simp)", L"zh-Hans" },
           { L"Chinese (Trad)", L"zh-Hant" }, { L"Russian", L"ru-RU" }
        };
        return sources;
    }

    // Target Languages (Google)
    inline const std::vector<TargetLanguage>& GetGoogleLanguages() {
        static const std::vector<TargetLanguage> targets = {
            // --- Popular & Regional ---
            { L"Hindi", L"hi" }, { L"Gujarati", L"gu" }, { L"Marathi", L"mr" },
            { L"Bengali", L"bn" }, { L"Tamil", L"ta" }, { L"Telugu", L"te" },
            { L"Kannada", L"kn" }, { L"Malayalam", L"ml" }, { L"Punjabi", L"pa" },
            // --- Global ---
            { L"English", L"en" }, { L"Japanese", L"ja" }, { L"Korean", L"ko" },
            { L"Chinese (Simp)", L"zh-CN" }, { L"Chinese (Trad)", L"zh-TW" },
            { L"Russian", L"ru" }, { L"Spanish", L"es" }, { L"French", L"fr" },
            { L"German", L"de" }, { L"Italian", L"it" }, { L"Portuguese", L"pt" },
            { L"Arabic", L"ar" }, { L"Turkish", L"tr" }, { L"Vietnamese", L"vi" },
            { L"Thai", L"th" }, { L"Indonesian", L"id" }
        };
        return targets;
    }

    // --- POPULATE UTILS ---
    inline void PopulateSourceLanguages(HWND hCombo) {
        SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
        for (const auto& lang : GetSteamLanguages()) SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)lang.displayName.c_str());
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }

    inline void PopulateTargetLanguages(HWND hCombo) {
        SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
        for (const auto& lang : GetGoogleLanguages()) SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)lang.name.c_str());
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }

    inline bool IsLanguagePackInstalled(std::wstring tag) {
        try {
            auto languages = OcrEngine::AvailableRecognizerLanguages();

            // If Windows has NO OCR languages installed at all
            if (languages.Size() == 0) return false;

            for (auto const& lang : languages) {
                std::wstring installedTag(lang.LanguageTag().c_str());

                // 1. Check exact match (ja-JP == ja-JP)
                if (_wcsicmp(installedTag.c_str(), tag.c_str()) == 0) return true;

                // 2. Check partial match (ja matches ja-JP)
                if (installedTag.length() >= 2 && tag.length() >= 2) {
                    if (_wcsnicmp(installedTag.c_str(), tag.c_str(), 2) == 0) return true;
                }
            }
        }
        catch (...) {
            return false;
        }
        return false;
    }

    // --- THE CORE OCR LOGIC ---
    inline winrt::fire_and_forget RecognizeText(HWND targetHwnd, std::wstring langTag, std::function<void(std::vector<TextBlock>)> callback) {
        // Capture the dispatcher context if needed, but for now, we use a simpler safety:
        auto lifetime = callback;

        std::vector<TextBlock> blocks;
        try {
            if (!IsWindow(targetHwnd)) { callback(blocks); co_return; }

            // --- CAPTURE LOGIC (Same as before, ensure it's within the try) ---
            
            // 1. Get Title Bar Height
            RECT rc, cc; 
			GetWindowRect(targetHwnd, &rc);
            GetClientRect(targetHwnd, &cc);
            int titleBarHeight = (rc.bottom - rc.top) - cc.bottom - GetSystemMetrics(SM_CYSIZEFRAME);
            

            int w = rc.right;
            int h = rc.bottom;
            if (w <= 0 || h <= 0) { callback(blocks); co_return; }

            HDC hScreenDC = GetDC(NULL);
            HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
            HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, w, h);
            SelectObject(hMemoryDC, hBitmap);
            // 2. Adjust the BitBlt capture to ONLY get the game area
            POINT pt = { 0, 0 };
            ClientToScreen(targetHwnd, &pt);
            BitBlt(hMemoryDC, 0, 0, w, h, hScreenDC, pt.x, pt.y, SRCCOPY);

            BITMAPINFOHEADER bi = { sizeof(bi), w, -h, 1, 32, BI_RGB };
            std::vector<uint8_t> pixels(w * h * 4);
            GetDIBits(hMemoryDC, hBitmap, 0, h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

            DataWriter writer;
            writer.WriteBytes(pixels);
            auto buffer = writer.DetachBuffer();
            SoftwareBitmap swBmp = SoftwareBitmap::CreateCopyFromBuffer(buffer, BitmapPixelFormat::Bgra8, w, h);

            DeleteObject(hBitmap);
            DeleteDC(hMemoryDC);
            ReleaseDC(NULL, hScreenDC);

            // --- OCR LOGIC ---
            auto engine = OcrEngine::TryCreateFromLanguage(winrt::Windows::Globalization::Language(langTag));
            if (!engine) {
                OutputDebugStringW(L"[WD] OCR Engine creation failed!\n");
                callback(blocks);
                co_return;
            }

            // The co_await happens here
            auto result = co_await engine.RecognizeAsync(swBmp);

            if (result) {
                for (auto const& line : result.Lines()) {
                    TextBlock b;
                    b.text = line.Text().c_str();

                    // Simplified bounding box for stability
                    auto r = line.Words().GetAt(0).BoundingRect(); // Start with first word
                    float minX = r.X, minY = r.Y, maxX = r.X + r.Width, maxY = r.Y + r.Height;

                    for (uint32_t i = 1; i < line.Words().Size(); i++) {
                        auto wr = line.Words().GetAt(i).BoundingRect();
                        minX = (std::min)(minX, wr.X);
                        minY = (std::min)(minY, wr.Y);
                        maxX = (std::max)(maxX, wr.X + wr.Width);
                        maxY = (std::max)(maxY, wr.Y + wr.Height);
                    }

                    // 2. STABILIZATION: Snap to 20-pixel grid
                    // This stops the box from vibrating/shaking
                    const int snap = 8;
                    b.box.left = (long)(floor(minX / snap) * snap);
                    b.box.top = (long)(floor(minY / snap) * snap);
                    b.box.right = (long)(ceil(maxX / snap) * snap);
                    b.box.bottom = (long)(ceil(maxY / snap) * snap);

                    // 3. PADDING: Give the text some breathing room
                    const int padding = 2;
                    b.box.left -= padding;
                    b.box.top -= padding;
                    b.box.right += padding;
                    b.box.bottom += padding;
                    blocks.push_back(b);
                }
            }
        }
        catch (...) {
            OutputDebugStringW(L"[WD] Critical OCR Error in RecognizeText\n");
        }

        // Return results to main thread via the callback
        callback(blocks);
        co_return;
    }
}