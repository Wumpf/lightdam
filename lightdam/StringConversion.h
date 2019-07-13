#pragma once

#include <string>

std::wstring Utf8toUtf16(const std::string& str);
std::string Utf16toUtf8(const std::wstring& str);

