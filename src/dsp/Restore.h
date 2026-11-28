#pragma once

#include "FastMath.h"
#include "Fft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace desat
{

/** Restore: gives flattened peaks back. No latency.

    Two parts, both driven by the same amount:

    1. An upward expander on onsets (fast envelope against slow), which undoes squashing without
       adding harmonics. In Clean it only acts as far as saturation was measured (see below).

    2. The inverse of the soft clipper. A soft clipper is, near enough, y = tanh (x); its inverse is
       atanh. Relative to the signal's ceiling the curve is atanh (k u) / k, where k says how hard the
       sound was driven (k = tanh of the drive). With the right k the clipper's harmonics and
       intermodulation cancel; with too much, the curve adds distortion of its own.

       So k is *measured*. Saturation fills a spectrum in, the matching inverse empties it again, and
       an inverse that is too strong fills it in once more. A small bank of candidate curves is tried
       on the incoming audio all the time and the one that leaves the sparsest spectrum is the match.
       How much sparser it gets is the confidence: on sound that was never saturated, or on noise,
       nothing gets clearly sparser and the curve stays flat.

       In Clean the amount is how much of the measured match to apply (100% undoes it). In Character
       the curve is forced, at least as strong as a heavy drive, whatever was measured.

    Only the part the curve adds is computed, and it is anti-aliased (first-order ADAA).
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
        gainAttack = coef (0.0005);
        gainRelease = coef (0.005);
        ceilingRelease = std::exp (-1.0 / (8.0 * sr)); // a saturator's ceiling doesn't move: hold it for seconds

        // The analysis frame is about 85 ms at any sample rate.
        frameSize = 4096 * (sr > 100000.0 ? 4 : (sr > 50000.0 ? 2 : 1));
        fft = std::make_unique<RealFft> (frameSize);
        window.resize ((size_t) frameSize);
        for (int i = 0; i < frameSize; ++i)
            window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / frameSize);

        ring.assign ((size_t) numChannels, std::vector<double> ((size_t) frameSize, 0.0));
        frame.assign ((size_t) frameSize, 0.0);
        shaped.assign ((size_t) frameSize, 0.0);
        re.assign ((size_t) frameSize / 2 + 1, 0.0);
        im.assign ((size_t) frameSize / 2 + 1, 0.0);

        const double binHz = sr / frameSize;
        firstBin = std::max (2, (int) (60.0 / binHz));
        lastBin = std::min (frameSize / 2 - 1, (int) (16000.0 / binHz));
        const double updateSeconds = (frameSize / 2) / sr;
        matchSmoothing = 1.0 - std::exp (-updateSeconds / 0.5);
        // Each candidate comes round every second update.
        differenceSmoothing = 1.0 - std::exp (-2.0 * updateSeconds / 1.2);

        reset();
    }

    void reset()
    {
        std::fill (last.begin(), last.end(), 0.0);
        for (auto& r : ring)
            std::fill (r.begin(), r.end(), 0.0);
        fast = slow = ceiling = 0.0;
        gain = 1.0;
        ringPos = sinceUpdate = nextCandidate = 0;
        fullness.fill (0.0);
        measured.fill (false);
        match = 0.0;
    }

    /** The drive it has measured, as k (0 = not saturated, towards 1 = driven hard). */
    double matchedDrive() const noexcept { return match; }

    /** amount is 0 to 1. Works in place. */
    void process (double* const* x, int numSamples, double amount, bool character) noexcept
    {
        const size_t numChannels = last.size();

        if (amount <= 0.0)
        {
            // Keep the level detectors following the signal so switching it on is smooth.
            // The drive measurement is left to rest: it costs CPU, and catches up within a second.
            for (int n = 0; n < numSamples; ++n)
            {
                track (linked (x, n));
                for (size_t c = 0; c < numChannels; ++c)
                    last[c] = x[c][n];
            }
            gain = 1.0;
            return;
        }

        const double maxGain = std::pow (10.0, amount * 6.0 / 20.0); // an attack is lifted by 6 dB at most

        for (int n = 0; n < numSamples; ++n)
        {
            // Feed the drive measurement.
            for (size_t c = 0; c < numChannels; ++c)
                ring[c][(size_t) ringPos] = x[c][n];
            if (++ringPos == frameSize)
                ringPos = 0;
            if (++sinceUpdate >= frameSize / 2)
            {
                sinceUpdate = 0;
                measure();
            }

            track (linked (x, n));

            // In Clean the expander, like the curve, only acts as far as saturation was measured:
            // on sound that was never squashed, Clean leaves the dynamics alone. Character always acts.
            const double exponent = amount * (character ? 0.8 : 0.5 * std::min (1.0, match / 0.5));

            double target = 1.0;
            if (exponent > 0.0 && fast > slow && slow > 1.0e-6)
                target = std::min (maxGain, std::pow (fast / slow, exponent));
            gain += (target > gain ? gainAttack : gainRelease) * (target - gain);

            // Clean applies the measured match; Character forces a curve at least as strong as a heavy drive.
            const double k = std::min (0.98, amount * (character ? std::max (match, 0.90) : match));
            const bool shaping = k > 1.0e-3 && ceiling > 1.0e-6;
            const double invCeiling = shaping ? 1.0 / ceiling : 0.0;

            for (size_t c = 0; c < numChannels; ++c)
            {
                const double x1 = x[c][n], x0 = last[c];
                last[c] = x1;

                double added = 0.0;
                if (shaping)
                {
                    // Both samples relative to the ceiling, kept inside the curve's range.
                    const double u1 = std::clamp (x1 * invCeiling, -1.0, 1.0), u0 = std::clamp (x0 * invCeiling, -1.0, 1.0);
                    const double du = u1 - u0;
                    // The curve averaged between the last sample and this one, less the straight line
                    // averaged the same way: what the curve adds, with the aliasing averaged out.
                    const double mean = std::abs (du) > 1.0e-5 ? (antiderivative (u1, k) - antiderivative (u0, k)) / du
                                                               : std::atanh (k * 0.5 * (u0 + u1)) / k;
                    added = ceiling * (mean - 0.5 * (u0 + u1));
                }

                x[c][n] = gain * (x1 + added);
            }
        }
    }

private:
    // Candidate curves: closer together where a small change in k is a big change in drive.
    static constexpr int numCandidates = 9;
    static constexpr std::array<double, numCandidates> candidates { 0.0, 0.30, 0.50, 0.66, 0.78, 0.86, 0.92, 0.96, 0.99 };

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
        ceiling = std::max (a, ceiling * ceilingRelease);
        if (fast < 1.0e-20) fast = 0.0;
        if (slow < 1.0e-20) slow = 0.0;
        if (ceiling < 1.0e-20) ceiling = 0.0;
    }

    /** How filled-in the frame's spectrum is after a candidate curve, in dB: lower is sparser.
        (sum |X|)^2 / (bins * sum |X|^2) is 0 dB for a flat spectrum and falls as it empties. */
    double fullnessAfter (double k) noexcept
    {
        if (k > 0.0)
        {
            // atanh (z) = (ln (1 + z) - ln (1 - z)) / 2, with the quick logarithm: this only steers a measurement.
            const double scale = 0.5 * 0.6931471805599453 / k;
            for (int i = 0; i < frameSize; ++i)
            {
                const double z = k * frame[(size_t) i];
                shaped[(size_t) i] = scale * (fastLog2 (1.0 + z) - fastLog2 (1.0 - z)) * window[(size_t) i];
            }
        }
        else
        {
            for (int i = 0; i < frameSize; ++i)
                shaped[(size_t) i] = frame[(size_t) i] * window[(size_t) i];
        }

        fft->forward (shaped.data(), re.data(), im.data());

        double l1 = 0.0, l2 = 0.0;
        for (int b = firstBin; b <= lastBin; ++b)
        {
            const double p = re[(size_t) b] * re[(size_t) b] + im[(size_t) b] * im[(size_t) b];
            l1 += std::sqrt (p);
            l2 += p;
        }
        return fastPowerToDb (l1 * l1 / ((lastBin - firstBin + 1) * l2 + 1.0e-300) + 1.0e-300);
    }

    /** Tries a few candidate curves on the latest frame and moves the match towards the best. */
    void measure() noexcept
    {
        if (ceiling < 1.0e-4)
            return;

        // The louder channel, relative to the ceiling.
        size_t loudest = 0;
        double best = -1.0;
        for (size_t c = 0; c < ring.size(); ++c)
        {
            double e = 0.0;
            for (int i = 0; i < frameSize; i += 8)
                e += ring[c][(size_t) i] * ring[c][(size_t) i];
            if (e > best)
            {
                best = e;
                loudest = c;
            }
        }

        const double inv = 1.0 / ceiling;
        double peak = 0.0;
        int p = ringPos;
        for (int i = 0; i < frameSize; ++i)
        {
            const double u = std::clamp (ring[loudest][(size_t) p] * inv, -1.0, 1.0);
            frame[(size_t) i] = u;
            peak = std::max (peak, std::abs (u));
            if (++p == frameSize)
                p = 0;
        }

        // A frame that stays well under the ceiling says nothing about how the top was bent.
        if (peak < 0.5)
            return;

        // Each candidate is judged against the unshaped version of the *same* frame: frames differ far
        // more from one to the next than the curves differ on one frame, so only the difference means
        // anything. Four candidates a time, in turn, each difference averaged over about a second.
        const double unshaped = fullnessAfter (0.0);
        for (int i = 0; i < 4; ++i)
        {
            nextCandidate = nextCandidate % (numCandidates - 1) + 1;
            const double d = fullnessAfter (candidates[(size_t) nextCandidate]) - unshaped;
            auto& smoothed = fullness[(size_t) nextCandidate];
            smoothed = measured[(size_t) nextCandidate] ? smoothed + differenceSmoothing * (d - smoothed) : d;
            measured[(size_t) nextCandidate] = true;
        }
        measured[0] = true;
        fullness[0] = 0.0;

        for (bool m : measured)
            if (! m)
                return;

        int pick = 0;
        for (int i = 1; i < numCandidates; ++i)
            if (fullness[(size_t) i] < fullness[(size_t) pick])
                pick = i;

        // Between candidates: the bottom of the parabola through the pick and its neighbours.
        double k = candidates[(size_t) pick];
        if (pick > 0 && pick < numCandidates - 1)
        {
            const double a = fullness[(size_t) pick - 1], b = fullness[(size_t) pick], c = fullness[(size_t) pick + 1];
            const double denom = a - 2.0 * b + c;
            if (denom > 1.0e-9)
            {
                const double shift = std::clamp (0.5 * (a - c) / denom, -0.5, 0.5);
                const double towards = candidates[(size_t) (shift > 0.0 ? pick + 1 : pick - 1)];
                k += std::abs (shift) * (towards - k);
            }
        }

        // How much sparser the match makes it is how sure we are. Noise and clean sound barely move.
        const double sparser = -fullness[(size_t) pick];
        double confidence = std::clamp ((sparser - 0.05) / 0.10, 0.0, 1.0);
        confidence = confidence * confidence * (3.0 - 2.0 * confidence);

        match += matchSmoothing * (k * confidence - match);
        if (match < 1.0e-4)
            match = 0.0;
    }

    double sr = 48000.0;
    std::vector<double> last;
    double fast = 0.0, slow = 0.0, ceiling = 0.0, gain = 1.0;
    double fastAttack = 0, fastRelease = 0, slowAttack = 0, slowRelease = 0, ceilingRelease = 0, gainAttack = 0, gainRelease = 0;

    // The drive measurement.
    int frameSize = 2048, firstBin = 2, lastBin = 1000;
    std::unique_ptr<RealFft> fft;
    std::vector<double> window, frame, shaped, re, im;
    std::vector<std::vector<double>> ring;
    int ringPos = 0, sinceUpdate = 0, nextCandidate = 0;
    std::array<double, numCandidates> fullness {}; // each candidate's fullness less the unshaped frame's, averaged
    std::array<bool, numCandidates> measured {};
    double match = 0.0, matchSmoothing = 0.1, differenceSmoothing = 0.1;
};

} // namespace desat
