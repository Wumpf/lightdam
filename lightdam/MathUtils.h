#pragma once

#include <cassert>

template<typename T>
bool IsPowerOfTwo(T x)
{
    return ((x & ~(x - 1)) == x) ? x : 0;
}

template<typename Value, typename Alignment=Value>
Value Align(Value v, Alignment powerOf2Alignment)
{
    assert(IsPowerOfTwo(powerOf2Alignment));
    auto _v = (uintptr_t)v;
    auto _powerOf2Alignment = (uintptr_t)powerOf2Alignment;
    return (Value)(((_v)+(_powerOf2Alignment)-1) & ~((_powerOf2Alignment)-1));
}

constexpr int MaxHaltonBaseIdx = 62;
float ComputeHaltonSequence(int index, int baseIdx);

constexpr float PI = 3.14159265358979323846f;
