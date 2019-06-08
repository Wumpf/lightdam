#pragma once

#include <cassert>

template<typename T>
bool IsPowerOfTwo(T x)
{
    return ((x & ~(x - 1)) == x) ? x : 0;
}

template<typename T>
T Align(T v, T powerOf2Alignment)
{
    assert(IsPowerOfTwo(powerOf2Alignment));
    return ((v)+(powerOf2Alignment)-1) & ~((powerOf2Alignment)-1);
}
