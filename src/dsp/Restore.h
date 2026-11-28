#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace desat
{

/** Restore: gives flattened peaks back. No latency.

    Two parts, both driven by the same amount:
      - an upward expander on onsets (fast envelope against slow), which undoes squashing
        without adding harmonics;
      - an inverse soft-clip curve, which stretches the top of the waveform back out. A soft clipper
        is, near enough, y = tanh (x); its inverse is atanh. Relative to the running peak the curve
        is atanh (k u) / k, where k says how hard the sound was driven (k = tanh of the drive), and
        that is what the amount sets. When k matches, the clipper's harmonics cancel. Only the part
        the curve adds is computed, and it is anti-aliased (first-order ADAA).
    At amount 0 the signal passes through exactly.
*/
class Restore
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        last.assign ((size_t) numChannels, 0.0);

        fastAttack = coef (0.0003);
        fastRelease = coef (0.008);
        slowAttack = coef (0.030);
        slowRelease = coef (0.150);
        peakRelease = std::exp (-1.0 / (0.250 * sr));
        gainAttack = coef (0.0005);
        gainRelease = coef (0.005);
        reset();
    }

    void reset()
    {
        std::fill (last.begin(), last.end(), 0.0);
        fast = slow = peak = 0.0;
        gain = 1.0;
    }

    /** amount is 0 to 1. Works in place. */
    void process (double* const* x, int numSamples, double amount, bool character) noexcept
    {
        if (amount <= 0.0)
        {
            // Keep the detectors following the signal so switching it on is smooth.
            for (int n = 0; n < numSamples; ++n)
            {
                track (linked (x, n));
                for (size_t c = 0; c < last.size(); ++c)
                    last[c] = x[c][n];
            }
            gain = 1.0;
            return;
        }

        const double exponent = amount * (character ? 0.8 : 0.5);
        const double maxGain = std::pow (10.0, amount * (character ? 10.0 : 6.0) / 20.0);
        // How hard a drive to undo: up to tanh(1.47) in Clean, tanh(2.3) in Character.
        const double k = amount * (character ? 0.98 : 0.90);

        for (int n = 0; n < numSamples; ++n)
        {
            track (linked (x, n));

            double target = 1.0;
            if (fast > slow && slow > 1.0e-6)
                target = std::min (maxGain, std::pow (fast / slow, exponent));
            gain += (target > gain ? gainAttack : gainRelease) * (target - gain);

            const bool shaping = k > 1.0e-3 && peak > 1.0e-6;
            const double invPeak = shaping ? 1.0 / peak : 0.0;

            for (size_t c = 0; c < last.size(); ++c)
            {
                const double x1 = x[c][n], x0 = last[c];
                last[c] = x1;

                double added = 0.0;
                if (shaping)
                {
                    // Both samples relative to the peak, kept inside the curve's range.
                    const double u1 = std::clamp (x1 * invPeak, -1.0, 1.0), u0 = std::clamp (x0 * invPeak, -1.0, 1.0);
                    const double du = u1 - u0;
                    // The curve averaged between the last sample and this one, less the straight line
                    // averaged the same way: what the curve adds, with the aliasing averaged out.
                    const double mean = std::abs (du) > 1.0e-5 ? (antiderivative (u1, k) - antiderivative (u0, k)) / du
                                                               : std::atanh (k * 0.5 * (u0 + u1)) / k;
                    added = peak * (mean - 0.5 * (u0 + u1));
                }

                x[c][n] = gain * (x1 + added);
            }
        }
    }

private:
    /** The integral of atanh (k u) / k. */
    static double antiderivative (double u, double k) noexcept
    {
        const double ku = k * u;
        return (ku * std::atanh (ku) + 0.5 * std::log (1.0 - ku * ku)) / (k * k);
    }

    double coef (double seconds) const noexcept { return 1.0 - std::exp (-1.0 / (seconds * sr)); }

    double linked (double* const* x, int n) const noexcept
    {
        double a = 0.0;
        for (size_t c = 0; c < last.size(); ++c)
            a = std::max (a, std::abs (x[c][n]));
        return a;
    }

    void track (double a) noexcept
    {
        fast += (a > fast ? fastAttack : fastRelease) * (a - fast);
        slow += (a > slow ? slowAttack : slowRelease) * (a - slow);
        peak = std::max (a, peak * peakRelease);
        if (fast < 1.0e-20) fast = 0.0;
        if (slow < 1.0e-20) slow = 0.0;
        if (peak < 1.0e-20) peak = 0.0;
    }

    double sr = 48000.0;
    std::vector<double> last;
    double fast = 0.0, slow = 0.0, peak = 0.0, gain = 1.0;
    double fastAttack = 0, fastRelease = 0, slowAttack = 0, slowRelease = 0, peakRelease = 0, gainAttack = 0, gainRelease = 0;
};

} // namespace desat
