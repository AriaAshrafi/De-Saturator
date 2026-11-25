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
      - an inverse soft-clip curve (x + g x^3, relative to the running peak), which stretches
        the top of the waveform back out. Its added part is anti-aliased (first-order ADAA).
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
        const double shape = amount * (character ? 1.0 : 0.3);

        for (int n = 0; n < numSamples; ++n)
        {
            track (linked (x, n));

            double target = 1.0;
            if (fast > slow && slow > 1.0e-6)
                target = std::min (maxGain, std::pow (fast / slow, exponent));
            gain += (target > gain ? gainAttack : gainRelease) * (target - gain);

            const double scale = peak > 1.0e-6 ? shape / (peak * peak) : 0.0;

            for (size_t c = 0; c < last.size(); ++c)
            {
                const double x1 = x[c][n], x0 = last[c];
                last[c] = x1;
                // The mean of x^3 between the last sample and this one.
                const double cubed = 0.25 * (x1 * x1 * x1 + x1 * x1 * x0 + x1 * x0 * x0 + x0 * x0 * x0);
                x[c][n] = gain * (x1 + scale * cubed);
            }
        }
    }

private:
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
