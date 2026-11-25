#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace desat
{

/** One bell of the thinning curve. */
struct CurvePoint
{
    double freq = 1000.0;  // Hz, 20 to 20000
    double amount = 1.0;   // 0 to 2 (1 = follows Amount)
    double width = 1.0;    // octaves at half height, 0.3 to 4

    bool operator== (const CurvePoint& o) const noexcept { return freq == o.freq && amount == o.amount && width == o.width; }
};

/** The thinning curve: bells on a baseline of 1, clamped to 0..2. Matches design/spec/SPEC.md. */
struct Curve
{
    static constexpr int maxPoints = 6;

    std::array<CurvePoint, maxPoints> points {};
    int count = 0;

    double valueAt (double freqHz) const noexcept
    {
        double v = 1.0;
        for (int i = 0; i < count; ++i)
        {
            const auto& p = points[(size_t) i];
            const double sigma = p.width / 2.3548;
            const double x = std::log2 (freqHz / p.freq) / sigma;
            v += (p.amount - 1.0) * std::exp (-0.5 * x * x);
        }
        return std::clamp (v, 0.0, 2.0);
    }

    bool operator== (const Curve& o) const noexcept
    {
        if (count != o.count)
            return false;
        for (int i = 0; i < count; ++i)
            if (! (points[(size_t) i] == o.points[(size_t) i]))
                return false;
        return true;
    }
};

/** The effective curve is reported to the window on this axis: log, 20 Hz to 20 kHz. */
constexpr int curveDisplayPoints = 256;

inline double curveDisplayFreq (int index) noexcept
{
    return 20.0 * std::pow (1000.0, index / (double) (curveDisplayPoints - 1));
}

/** What the thinning engines are asked to do, already smoothed by the caller. */
struct ThinSettings
{
    double amount = 0.0;      // 0 to 1
    double harmonics = 1.0;   // weight of overtone removal, 0 to 1
    double body = 1.0;        // weight of body removal, 0 to 1
    bool character = false;   // false = Clean
};

} // namespace desat
