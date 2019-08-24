#include "ErrorHandling.h"
#include <Windows.h>

std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<unsigned int>(hr));
    // TODO: FormatMessage?
    return std::string(s_str);
}

void detail::LogPrint(LogLevel logLevel, const char* message)
{
    constexpr int colorNormal = 15;
    constexpr int colorRed = 12;
    constexpr int colorGreen = 10;

    int colorIndex = colorNormal;
    if (logLevel == LogLevel::Failure)
        colorIndex = colorRed;
    else if (logLevel == LogLevel::Success)
        colorIndex = colorGreen;

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colorIndex);
    printf(message);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colorNormal);

    OutputDebugStringA(message);
}
