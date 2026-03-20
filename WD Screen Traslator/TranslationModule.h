#pragma once
#include <windows.h>
#include <string>
#include <wininet.h>
#include <vector>

#pragma comment(lib, "wininet.lib")

namespace TranslationModule {
    inline std::wstring GetTranslatedText(std::wstring text, std::wstring srcTag, std::wstring targetTag) {
        if (text.empty()) return L"";

        // 1. Convert Japanese/Source text to UTF-8 URL format
        std::string utf8Text;
        int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
        utf8Text.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8Text[0], len, NULL, NULL);

        std::wstring encodedText;
        for (unsigned char c : utf8Text) {
            if (c == 0) continue;
            if (isalnum(c)) encodedText += (wchar_t)c;
            else {
                wchar_t buf[5];
                swprintf_s(buf, L"%%%.2X", c);
                encodedText += buf;
            }
        }

        // 2. Build URL (sl = ja, tl = gu/en)
        std::wstring sl = srcTag.substr(0, 2);
        std::wstring url = L"https://translate.googleapis.com/translate_a/single?client=gtx&sl="
            + sl + L"&tl=" + targetTag + L"&dt=t&q=" + encodedText;

        HINTERNET hSession = InternetOpenW(L"Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        std::wstring finalResult = L"Translation Failed";

        if (hSession) {
            HINTERNET hUrl = InternetOpenUrlW(hSession, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
            if (hUrl) {
                std::string raw;
                char buffer[4096];
                DWORD bytesRead;
                while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    raw.append(buffer, bytesRead);
                }

                // 3. PARSE JSON: Look for the text between the first set of double quotes
                size_t start = raw.find("\"") + 1;
                size_t end = raw.find("\"", start);

                if (start != std::string::npos && end != std::string::npos) {
                    std::string extracted = raw.substr(start, end - start);

                    // Convert the UTF-8 response back to Unicode for the Red Box
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, extracted.c_str(), -1, NULL, 0);
                    std::wstring wstr(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, extracted.c_str(), -1, &wstr[0], wlen);
                    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
                    finalResult = wstr;
                }
                InternetCloseHandle(hUrl);
            }
            InternetCloseHandle(hSession);
        }
        return finalResult;
    }
}