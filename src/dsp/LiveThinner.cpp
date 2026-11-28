#include "LiveThinner.h"

#include <algorithm>
#include <cmath>

namespace desat
{

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr int controlInterval = 16; // samples between gain updates

struct LiveMode
{
    double fillPerDb, fillCap, sustainDb, harmonicsDb, bodyDb, maxDb, transientProtect;
};

constexpr LiveMode cleanMode     { 0.7, 14.0,  5.0, 12.0, 12.0, 30.0, 0.85 };
constexpr LiveMode characterMode { 1.6, 32.0, 12.0, 30.0, 24.0, 60.0, 0.60 };

constexpr double bandQ = 1.1;
constexpr double bandOverlap = 0.75; // neighbouring bells add up; this brings a flat cut back to size
constexpr double notchQ = 8.0;
constexpr double bodyFreq = 260.0;

// State-variable filter after Andrew Simper (Cytomic): stable under fast modulation.
inline double tick (double v0, double a1, double a2, double a3, double& ic1, double& ic2, double& v1) noexcept
{
    const double v3 = v0 - ic2;
    v1 = a1 * ic1 + a2 * v3;
    const double v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0 * v1 - ic1;
    ic2 = 2.0 * v2 - ic2;
    return v2;
}
} // namespace

//==============================================================================
void LiveThinner::BellCoef::setFrequency (double freq, double sampleRate) noexcept
{
    g = std::tan (pi * std::min (freq, 0.49 * sampleRate) / sampleRate);
}

void LiveThinner::BellCoef::setGain (double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double k = 1.0 / (q * A);
    a1 = 1.0 / (1.0 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
    m1 = gainDb == 0.0 ? 0.0 : k * (A * A - 1.0);
}

//==============================================================================
void LiveThinner::PitchTracker::prepare (double sampleRate)
{
    decimation = std::max (1, (int) std::lround (sampleRate / 11025.0));
    srDecimated = sampleRate / decimation;
    window = 512;
    hop = 128;
    tauMax = (int) (srDecimated / 45.0);
    tauMin = std::max (2, (int) (srDecimated / 1000.0));

    // A 4th-order Butterworth low-pass before decimating.
    lpA.q = 0.5412;
    lpB.q = 1.3066;
    for (auto* c : { &lpA, &lpB })
    {
        c->setFrequency (0.3 * srDecimated, sampleRate);
        c->setGain (0.0);
    }

    buffer.assign ((size_t) (window + tauMax + 1), 0.0);
    diff.assign ((size_t) tauMax + 2, 0.0);
    reset();
}

void LiveThinner::PitchTracker::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0);
    sA = sB = {};
    phase = fill = 0;
    f0 = 0.0;
    aperiodicity = 1.0;
}

bool LiveThinner::PitchTracker::push (double x) noexcept
{
    double v1;
    x = tick (x, lpA.a1, lpA.a2, lpA.a3, sA.ic1, sA.ic2, v1);
    x = tick (x, lpB.a1, lpB.a2, lpB.a3, sB.ic1, sB.ic2, v1);

    if (++phase < decimation)
        return false;
    phase = 0;

    // Slide the buffer along by a hop: it is small, and this keeps the estimate loop simple.
    const size_t size = buffer.size();
    if (fill == 0)
        std::copy (buffer.begin() + hop, buffer.end(), buffer.begin());
    buffer[size - (size_t) hop + (size_t) fill] = x;

    if (++fill < hop)
        return false;

    fill = 0;
    estimate();
    return true;
}

void LiveThinner::PitchTracker::estimate() noexcept
{
    const double* x = buffer.data();

    double energy = 0.0;
    for (int j = 0; j < window; ++j)
        energy += x[j] * x[j];

    if (energy < window * 1.0e-9)
    {
        f0 = 0.0;
        aperiodicity = 1.0;
        return;
    }

    // YIN: the difference function, then its cumulative mean normalisation.
    double running = 0.0;
    diff[0] = 1.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        // Four running sums, so the additions don't wait on each other.
        double d0 = 0.0, d1 = 0.0, d2 = 0.0, d3 = 0.0;
        const double* y = x + tau;
        for (int j = 0; j < window; j += 4)
        {
            const double e0 = x[j] - y[j], e1 = x[j + 1] - y[j + 1], e2 = x[j + 2] - y[j + 2], e3 = x[j + 3] - y[j + 3];
            d0 += e0 * e0;
            d1 += e1 * e1;
            d2 += e2 * e2;
            d3 += e3 * e3;
        }
        const double d = (d0 + d1) + (d2 + d3);
        running += d;
        diff[(size_t) tau] = running > 0.0 ? d * tau / running : 1.0;
    }

    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (diff[(size_t) tau] < 0.15)
        {
            while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin + 1; tau < tauMax; ++tau)
            if (diff[(size_t) tau] < diff[(size_t) best])
                best = tau;
    }

    double tauFine = best;
    if (best > tauMin && best < tauMax - 1)
    {
        const double a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const double denom = a - 2.0 * b + c;
        if (denom > 1.0e-12)
            tauFine += std::clamp (0.5 * (a - c) / denom, -0.5, 0.5);
    }

    f0 = srDecimated / tauFine;
    aperiodicity = diff[(size_t) best];
}

//==============================================================================
void LiveThinner::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    channels = numChannels;
    chan.assign ((size_t) channels, {});

    for (int b = 0; b < numBands; ++b)
    {
        const double fc = 60.0 * std::pow (12000.0 / 60.0, b / (double) (numBands - 1));
        centre[(size_t) b] = fc;

        auto& a = analysisCoef[(size_t) b];
        a.q = 1.4;
        a.setFrequency (fc, sr);
        a.setGain (0.0);

        auto& c = bandCoef[(size_t) b];
        c.q = bandQ;
        c.setFrequency (fc, sr);
        c.setGain (0.0);

        // Lower bands need longer to tell a sustained sound from an onset.
        const double releaseSeconds = std::max (0.025, 4.0 / fc);
        const double rmsSeconds = std::max (0.120, 12.0 / fc);
        peakRelease[(size_t) b] = std::exp (-1.0 / (releaseSeconds * sr));
        rmsCoef[(size_t) b] = 1.0 - std::exp (-1.0 / (rmsSeconds * sr));
    }

    bodyCoef.q = 0.6;
    bodyCoef.setFrequency (bodyFreq, sr);
    bodyCoef.setGain (0.0);

    for (auto& n : notchCoef)
    {
        n.q = notchQ;
        n.setFrequency (1000.0, sr);
        n.setGain (0.0);
    }

    pitch.prepare (sr);
    setCurve (curve);
    reset();
}

void LiveThinner::reset()
{
    for (auto& c : chan)
        c = {};

    analysisState = {};
    peak = {};
    meanSquare = {};
    reduction = {};
    notchReduction = {};
    bodyReduction = 0.0;
    f0 = f0Target = 0.0;
    confidence = confidenceTarget = 0.0;
    controlCounter = 0;
    pitch.reset();
}

void LiveThinner::setCurve (const Curve& c)
{
    curve = c;

    // Average the curve onto each band, with a Gaussian window half an octave wide.
    constexpr double sigma = 0.5;
    constexpr int steps = 96;
    std::array<double, steps> freq {}, value {};
    for (int i = 0; i < steps; ++i)
    {
        freq[(size_t) i] = 20.0 * std::pow (1000.0, i / (double) (steps - 1));
        value[(size_t) i] = curve.valueAt (freq[(size_t) i]);
    }

    auto weight = [] (double f, double fc)
    {
        const double x = std::log2 (f / fc) / sigma;
        return std::exp (-0.5 * x * x);
    };

    for (int b = 0; b < numBands; ++b)
    {
        double sum = 0.0, norm = 0.0;
        for (int i = 0; i < steps; ++i)
        {
            const double w = weight (freq[(size_t) i], centre[(size_t) b]);
            sum += w * value[(size_t) i];
            norm += w;
        }
        bandCurve[(size_t) b] = norm > 0.0 ? sum / norm : 1.0;
    }

    for (int i = 0; i < curveDisplayPoints; ++i)
    {
        const double f = curveDisplayFreq (i);
        double sum = 0.0, norm = 0.0;
        for (int b = 0; b < numBands; ++b)
        {
            const double w = weight (f, centre[(size_t) b]);
            sum += w * bandCurve[(size_t) b];
            norm += w;
        }
        // Past the outer bands the nearest band's value carries on.
        if (norm < 1.0e-9)
            sum = f < centre[0] ? bandCurve[0] : bandCurve[numBands - 1], norm = 1.0;
        effective[(size_t) i] = (float) std::clamp (sum / norm, 0.0, 2.0);
    }
}

void LiveThinner::effectiveCurve (float* out) const noexcept
{
    std::copy (effective.begin(), effective.end(), out);
}

double LiveThinner::effectiveAt (double freqHz) const noexcept
{
    const double x = std::clamp (std::log (freqHz / 20.0) / std::log (1000.0), 0.0, 1.0) * (curveDisplayPoints - 1);
    const int i = std::min ((int) x, curveDisplayPoints - 2);
    const double frac = x - i;
    return effective[(size_t) i] * (1.0 - frac) + effective[(size_t) i + 1] * frac;
}

//==============================================================================
void LiveThinner::updateControl (const ThinSettings& s)
{
    const auto& mode = s.character ? characterMode : cleanMode;
    const double step = controlInterval / sr;
    const double attack = 1.0 - std::exp (-step / 0.015);
    const double release = 1.0 - std::exp (-step / 0.060);

    // Band levels, tilted so that an ordinary falling spectrum reads as level.
    std::array<double, numBands> level {}, crest {};
    double total = 0.0, bodyEnergy = 0.0;
    for (int b = 0; b < numBands; ++b)
    {
        const double ms = meanSquare[(size_t) b];
        const double rms = std::sqrt (ms);
        level[(size_t) b] = 10.0 * std::log10 (ms + 1.0e-20) + 3.0 * std::log2 (centre[(size_t) b] / 1000.0);
        crest[(size_t) b] = 20.0 * std::log10 ((peak[(size_t) b] + 1.0e-10) / (rms + 1.0e-10));
        total += ms;
        if (centre[(size_t) b] > 120.0 && centre[(size_t) b] < 600.0)
            bodyEnergy += ms;
    }

    for (int b = 0; b < numBands; ++b)
    {
        double upper = -200.0;
        for (int j = std::max (0, b - 2); j <= std::min (numBands - 1, b + 2); ++j)
            if (j != b)
                upper = std::max (upper, level[(size_t) j] - (std::abs (j - b) == 2 ? 3.0 : 0.0));

        const bool audible = meanSquare[(size_t) b] > 1.0e-9; // above about -90 dB
        const double valley = std::max (0.0, upper - level[(size_t) b] - 3.0);
        const double fill = std::min (mode.fillPerDb * valley, mode.fillCap);
        const double dense = std::clamp ((14.0 - crest[(size_t) b]) / 8.0, 0.0, 1.0);
        const double onset = std::clamp ((crest[(size_t) b] - 12.0) / 8.0, 0.0, 1.0);

        const double depth = s.amount * bandCurve[(size_t) b];
        double target = audible ? depth * (fill + mode.sustainDb * dense) * (1.0 - mode.transientProtect * onset) : 0.0;
        target = std::min (target, mode.maxDb * depth);

        auto& r = reduction[(size_t) b];
        r += (target > r ? attack : release) * (target - r);
        if (r < 1.0e-6)
            r = 0.0;

        bandCoef[(size_t) b].setGain (-bandOverlap * r);
    }

    // Body.
    {
        const double share = bodyEnergy / (total + 1.0e-20);
        const double dynamics = std::clamp (0.7 + 1.2 * share, 0.7, 1.3);
        const double depth = s.amount * effectiveAt (bodyFreq);
        const double target = total > 1.0e-9 ? std::min (depth * s.body * mode.bodyDb * dynamics, mode.maxDb * depth) : 0.0;
        bodyReduction += (target > bodyReduction ? attack : release) * (target - bodyReduction);
        if (bodyReduction < 1.0e-6)
            bodyReduction = 0.0;
        bodyCoef.setGain (-bodyReduction);
    }

    // Overtones of the tracked pitch.
    confidence += (confidenceTarget > confidence ? 0.02 : 0.08) * (confidenceTarget - confidence);
    if (f0Target > 0.0)
        f0 = f0 > 0.0 ? f0 + 0.3 * (f0Target - f0) : f0Target;

    for (int i = 0; i < numNotches; ++i)
    {
        const int h = i + 2;
        const double f = h * f0;
        double target = 0.0;

        if (f0 > 0.0 && f < 0.45 * sr && confidence > 0.01)
        {
            const double depth = s.amount * effectiveAt (f);
            const double weight = std::pow (std::min (1.0, (h - 1) / 5.0), 0.7);
            target = std::min (depth * s.harmonics * mode.harmonicsDb * weight * confidence, mode.maxDb * depth);
            notchCoef[(size_t) i].setFrequency (f, sr);
        }

        auto& r = notchReduction[(size_t) i];
        r += (target > r ? attack : release) * (target - r);
        if (r < 1.0e-6)
            r = 0.0;

        notchCoef[(size_t) i].setGain (-r);
    }
}

void LiveThinner::process (const double* const* in, double* const* removed, int numSamples, const ThinSettings& settings)
{
    for (int n = 0; n < numSamples; ++n)
    {
        if (controlCounter == 0)
            updateControl (settings);
        if (++controlCounter == controlInterval)
            controlCounter = 0;

        // Detection on the mono sum.
        double mono = 0.0;
        for (int c = 0; c < channels; ++c)
            mono += in[c][n];
        mono /= channels;

        for (int b = 0; b < numBands; ++b)
        {
            const auto& a = analysisCoef[(size_t) b];
            auto& st = analysisState[(size_t) b];
            double v1;
            tick (mono, a.a1, a.a2, a.a3, st.ic1, st.ic2, v1);
            const double band = v1 / a.q; // band-pass, unity at the centre

            const double mag = std::abs (band);
            auto& pk = peak[(size_t) b];
            pk = std::max (mag, pk * peakRelease[(size_t) b]);
            auto& ms = meanSquare[(size_t) b];
            ms += rmsCoef[(size_t) b] * (band * band - ms);
            if (ms < 1.0e-30)
                ms = 0.0;
        }

        if (pitch.push (mono))
        {
            f0Target = pitch.f0;
            confidenceTarget = pitch.f0 > 0.0 ? std::clamp ((0.30 - pitch.aperiodicity) / 0.20, 0.0, 1.0) : 0.0;
        }

        // The chain.
        for (int c = 0; c < channels; ++c)
        {
            auto& cs = chan[(size_t) c];
            const double x = in[c][n];
            double y = x, v1;

            for (int b = 0; b < numBands; ++b)
            {
                const auto& k = bandCoef[(size_t) b];
                tick (y, k.a1, k.a2, k.a3, cs.band[(size_t) b].ic1, cs.band[(size_t) b].ic2, v1);
                y += k.m1 * v1;
            }

            tick (y, bodyCoef.a1, bodyCoef.a2, bodyCoef.a3, cs.body.ic1, cs.body.ic2, v1);
            y += bodyCoef.m1 * v1;

            for (int i = 0; i < numNotches; ++i)
            {
                const auto& k = notchCoef[(size_t) i];
                tick (y, k.a1, k.a2, k.a3, cs.notch[(size_t) i].ic1, cs.notch[(size_t) i].ic2, v1);
                y += k.m1 * v1;
            }

            removed[c][n] = x - y;
        }
    }
}

} // namespace desat
