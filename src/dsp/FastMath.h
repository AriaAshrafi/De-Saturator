#pragma once

#include <cstdint>
#include <cstring>

namespace desat
{

/** log2 for positive, normal doubles, good to about 1e-5: far finer than any level decision here needs,
    and several times quicker than the library call. The exponent comes straight from the bits; the
    mantissa, in [1, 2), goes through a short series in (m - 1) / (m + 1). */
inline double fastLog2 (double x) noexcept
{
    uint64_t bits;
    std::memcpy (&bits, &x, sizeof (bits));
    const int exponent = (int) ((bits >> 52) & 0x7ff) - 1023;
    bits = (bits & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL;
    double m;
    std::memcpy (&m, &bits, sizeof (m));

    const double t = (m - 1.0) / (m + 1.0);
    const double t2 = t * t;
    // ln(m) = 2 (t + t^3/3 + t^5/5 + t^7/7 + t^9/9), over ln 2
    return exponent + 2.8853900817779268 * t * (1.0 + t2 * (1.0 / 3.0 + t2 * (0.2 + t2 * (1.0 / 7.0 + t2 * (1.0 / 9.0)))));
}

/** 10 log10 (x): power to decibels. */
inline double fastPowerToDb (double x) noexcept
{
    return 3.0102999566398120 * fastLog2 (x);
}

} // namespace desat
