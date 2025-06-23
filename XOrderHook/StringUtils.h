#pragma once
#include <string>
#include <windows.h>

// Converts a multi-byte (ANSI) string to a wide (UTF-16) string.
// Convertit une chaîne multi-octets (ANSI) en une chaîne large (UTF-16).
std::wstring Utf8ToWide(const std::string& utf8Str) {
    if (utf8Str.empty()) {
        return std::wstring();
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), NULL, 0);
    std::wstring wideStr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), &wideStr[0], size_needed);
    return wideStr;
}

// Converts a wide (UTF-16) string to a multi-byte (ANSI) string.
// Convertit une chaîne large (UTF-16) en une chaîne multi-octets (ANSI).
std::string WideToUtf8(const std::wstring& wideStr) {
    if (wideStr.empty()) {
        return std::string();
    }
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], (int)wideStr.size(), NULL, 0, NULL, NULL);
    std::string utf8Str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], (int)wideStr.size(), &utf8Str[0], size_needed, NULL, NULL);
    return utf8Str;
}
