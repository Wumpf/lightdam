#include "StringConversion.h"
#include <Windows.h>
#include <exception>

std::wstring Utf8toUtf16(const std::string& str)
{
    // https://stackoverflow.com/a/26914562

    if (str.empty())
        return L"";

    const int charsNeeded = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    if (charsNeeded == 0)
        throw std::exception("Failed converting UTF-8 string to UTF-16");

    std::wstring buffer;
    buffer.resize(charsNeeded);
    int charsConverted = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &buffer[0], (int)buffer.size());
    if (charsConverted == 0)
        throw std::exception("Failed converting UTF-8 string to UTF-16");

    return buffer;
}

std::string Utf16toUtf8(const std::wstring& str)
{
    if (str.empty())
        return "";

    const int charsNeeded = ::WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0, 0, nullptr);
    if (charsNeeded == 0)
        throw std::exception("Failed converting UTF-16 string to UTF-8");

    std::string buffer;
    buffer.resize(charsNeeded);
    int charsConverted = ::WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), &buffer[0], (int)buffer.size(), 0, nullptr);
    if (charsConverted == 0)
        throw std::exception("Failed converting UTF-16 string to UTF-8");

    return buffer;
}
