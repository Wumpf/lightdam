#pragma once

#include <stdexcept>
#include <string>
#include <winerror.h>

std::string HrToString(HRESULT hr);

class HResultException : public std::runtime_error
{
public:
    HResultException(HRESULT hr)
        : std::runtime_error(HrToString(hr))
        , m_hr(hr)
    {}

    HRESULT Error() const { return m_hr; }

private:
    const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
        throw HResultException(hr);
}