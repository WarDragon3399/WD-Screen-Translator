#pragma once
#include <windows.h>
#include <string>
#include <wininet.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "wininet.lib")

namespace TranslationModule {
    // Helper to URL encode UTF-8 strings
    inline std::wstring UrlEncode(const std::string& value) {
        std::wostringstream escaped;
        escaped.fill('0');
        for (auto i = value.begin(); i != value.end(); ++i) {
            unsigned char c = (unsigned char)*i;
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << (wchar_t)c;
                continue;
            }
            escaped << std::uppercase << L"%" << std::hex << std::setw(2) << static_cast<int>(c);
        }
        return escaped.str();
    }

    inline std::wstring GetTranslatedText(std::wstring text, std::wstring srcTag, std::wstring targetTag) {
        if (text.empty()) return L"";

        // 1. Language Tag Cleanup
        std::wstring sl = (srcTag.length() >= 2) ? srcTag.substr(0, 2) : L"auto";
        std::wstring tl = (targetTag.length() >= 2) ? targetTag.substr(0, 2) : targetTag;

        // 2. Wide to UTF-8
        int cbNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
        std::string utf8Str(cbNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8Str[0], cbNeeded, NULL, NULL);
        if (!utf8Str.empty() && utf8Str.back() == '\0') utf8Str.pop_back();

        // 3. Request
        std::wstring url = L"https://translate.googleapis.com/translate_a/single?client=gtx&sl="
            + sl + L"&tl=" + tl + L"&dt=t&q=" + UrlEncode(utf8Str);

        std::string rawResponse;
        HINTERNET hSession = InternetOpenW(L"Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (hSession) {
            HINTERNET hUrl = InternetOpenUrlW(hSession, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
            if (hUrl) {
                char buffer[16384]; // Larger buffer for long RPG dialogues
                DWORD bytesRead;
                while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    rawResponse.append(buffer, bytesRead);
                }
                InternetCloseHandle(hUrl);
            }
            InternetCloseHandle(hSession);
        }

        if (rawResponse.empty()) return L"[Error: No Response]";

        // 4. ROBUST PARSING for Google's nested JSON structure
        // We need to find all instances of ["translated text","original text",...]
        std::wstring finalWResult = L"";
        size_t pos = 0;
        while ((pos = rawResponse.find("[\"", pos)) != std::string::npos) {
            size_t startQuote = pos + 2;
            size_t endQuote = rawResponse.find("\",\"", startQuote);
            if (endQuote != std::string::npos) {
                std::string part = rawResponse.substr(startQuote, endQuote - startQuote);

                // Convert segment UTF-8 to Wide
                int wLen = MultiByteToWideChar(CP_UTF8, 0, part.c_str(), (int)part.length(), NULL, 0);
                std::wstring wPart(wLen, 0);
                MultiByteToWideChar(CP_UTF8, 0, part.c_str(), (int)part.length(), &wPart[0], wLen);

                finalWResult += wPart;
                pos = endQuote + 3;
            }
            else break;

            // Google puts the full translation in the first few nested arrays. 
            // If we hit the original text or metadata, we stop.
            if (rawResponse.find(",[[", pos) != std::string::npos) break;
        }

        return finalWResult.empty() ? L"[Parsing Error]" : finalWResult;
    }
}