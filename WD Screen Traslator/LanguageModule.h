#include <windows.h>
#include <vector>
#include <string>
#include <shellapi.h>
#include <functional>

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

struct TextBlock {
    std::wstring text;
    RECT box; // The x, y, width, height on screen
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
        std::vector<TextBlock> blocks;
        auto ReportError = [&](std::wstring msg) {
            TextBlock errBlock;
            errBlock.text = L"OCR Error: " + msg;
            errBlock.box = { 10, 10, 500, 50 };
            blocks.push_back(errBlock);
            };

        try {
            RECT rc;
            if (!GetWindowRect(targetHwnd, &rc)) {
                ReportError(L"Game Window Lost");
                callback(blocks);
                co_return;
            }

            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w <= 0 || h <= 0) {
                ReportError(L"Invalid Window Size");
                callback(blocks);
                co_return;
            }

            HDC hScreen = GetDC(NULL);
            HDC hDC = CreateCompatibleDC(hScreen);
            HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
            SelectObject(hDC, hBitmap);
            BitBlt(hDC, 0, 0, w, h, hScreen, rc.left, rc.top, SRCCOPY);

            BITMAPINFOHEADER bi = { sizeof(bi), w, -h, 1, 32, BI_RGB };
            std::vector<uint8_t> pixels(w * h * 4);
            GetDIBits(hDC, hBitmap, 0, h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

            DataWriter writer;
            writer.WriteBytes(pixels);
            SoftwareBitmap softwareBitmap(BitmapPixelFormat::Bgra8, w, h, BitmapAlphaMode::Premultiplied);
            softwareBitmap.CopyFromBuffer(writer.DetachBuffer());

            DeleteObject(hBitmap);
            DeleteDC(hDC);
            ReleaseDC(NULL, hScreen);

            auto language = winrt::Windows::Globalization::Language(langTag);
            auto engine = OcrEngine::TryCreateFromLanguage(language);

            if (!engine) {
                ReportError(L"Language Pack '" + langTag + L"' missing");
                callback(blocks);
                co_return;
            }

            auto result = co_await engine.RecognizeAsync(softwareBitmap);

            if (result) {
                for (auto const& line : result.Lines()) {
                    TextBlock block;
                    block.text = line.Text().c_str();

                    // Calculate the box for the whole line
                    float minX = 10000, minY = 10000, maxX = 0, maxY = 0;
                    POINT topLeft = { (long)minX, (long)minY };
                    POINT bottomRight = { (long)maxX, (long)maxY };
                    for (auto const& word : line.Words()) {
                        auto r = word.BoundingRect();
                        if (r.X < minX) minX = r.X;
                        if (r.Y < minY) minY = r.Y;
                        if (r.X + r.Width > maxX) maxX = r.X + r.Width;
                        if (r.Y + r.Height > maxY) maxY = r.Y + r.Height;
                    }
                    block.box = { (long)minX, (long)minY, (long)maxX, (long)maxY };
                    blocks.push_back(block);
                }
            }

            // SUCCESS: Send the blocks to the bubble
            callback(blocks);
        }
        catch (winrt::hresult_error const& ex) {
            ReportError(ex.message().c_str());
            callback(blocks);
        }
        co_return;
    }
}