#include "ErrorHandling.h"

std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<unsigned int>(hr));
    // TODO: FormatMessage?
    return std::string(s_str);
}
