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

float ComputeHaltonSequence(int index, int baseIdx)
{
    static int primes[] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293
    };

    assert(baseIdx < sizeof(primes) / sizeof(primes[0]));

    int base = primes[baseIdx];
    float f = 1, r = 0;
    while (index > 0)
    {
        f /= base;
        r += f * (index % base);
        index /= base;
    }
    return r;
}