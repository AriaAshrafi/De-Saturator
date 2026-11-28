#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace desat
{

int Engine::latencyFor (double sampleRate, bool hq) noexcept
{
    if (! hq)
        return 0;
    return 4096 * (sampleRate > 100000.0 ? 4 : (sampleRate > 50000.0 ? 2 : 1));
}

void Engine::prepare (double sampleRate, int numChannels, int maxBlockSize)
{
    sr = sampleRate;
    channels = numChannels;

    spectral.prepare (sr, channels);
    live.prepare (sr, channels);
    restore.prepare (sr, channels);
    dryWeighting.prepare (sr, channels);
    wetWeighting.prepare (sr, channels);
    spectral.setCurve (curve);
    live.setCurve (curve);

    delayLine.assign ((size_t) channels, std::vector<double> ((size_t) spectral.latency(), 0.0));

    for (auto* b : { &inBuf, &removedBuf, &dryBuf, &wetBuf })
        b->assign ((size_t) channels, std::vector<double> ((size_t) chunkSize, 0.0));

    inPtr.resize ((size_t) channels);
    removedPtr.resize ((size_t) channels);
    wetPtr.resize ((size_t) channels);
    inConstPtr.resize ((size_t) channels);
    for (int c = 0; c < channels; ++c)
    {
        inPtr[(size_t) c] = inBuf[(size_t) c].data();
        inConstPtr[(size_t) c] = inBuf[(size_t) c].data();
        removedPtr[(size_t) c] = removedBuf[(size_t) c].data();
        wetPtr[(size_t) c] = wetBuf[(size_t) c].data();
    }

    tapIn.assign ((size_t) std::max (1, maxBlockSize), 0.0);
    tapOut.assign ((size_t) std::max (1, maxBlockSize), 0.0);

    fadeLength = std::max (1, (int) (0.005 * sr));
    hqActive = target.hq;
    reset();
}

void Engine::reset()
{
    spectral.reset();
    live.reset();
    restore.reset();

    for (auto& d : delayLine)
        std::fill (d.begin(), d.end(), 0.0);
    delayPos = 0;

    dryPower = wetPower = 0.0;
    autoGainValue = 1.0;
    dryWeighting.reset();
    wetWeighting.reset();
    fadeRemaining = 0;
    primed = false;
}

void Engine::setCurve (const Curve& c)
{
    if (c == curve)
        return;
    curve = c;
    spectral.setCurve (curve);
    live.setCurve (curve);
}

void Engine::effectiveCurve (float* out) const noexcept
{
    if (hqActive)
    {
        for (int i = 0; i < curveDisplayPoints; ++i)
            out[i] = (float) curve.valueAt (curveDisplayFreq (i));
    }
    else
    {
        live.effectiveCurve (out);
    }
}

void Engine::process (double* const* io, int numSamples)
{
    for (int offset = 0; offset < numSamples; offset += chunkSize)
        processChunk (io, offset, std::min (chunkSize, numSamples - offset));
}

void Engine::processChunk (double* const* io, int offset, int n)
{
    if (target.hq != hqActive)
    {
        // The two engines have different latencies, so there is no seamless way across: start clean and fade in.
        hqActive = target.hq;
        spectral.reset();
        live.reset();
        restore.reset();
        for (auto& d : delayLine)
            std::fill (d.begin(), d.end(), 0.0);
        delayPos = 0;
        fadeRemaining = fadeLength;
    }

    // Controls ease towards their targets once per chunk; the gains are ramped across it.
    const bool firstChunk = ! primed;
    const double a = primed ? 1.0 - std::exp (-n / (0.020 * sr)) : 1.0;
    primed = true;

    auto ease = [a] (double& value, double to)
    {
        value += a * (to - value);
        if (std::abs (to - value) < 1.0e-9)
            value = to;
    };

    double mix0 = mix, out0 = outputGain, bypass0 = bypassMix, delta0 = deltaMix;
    double auto0 = 1.0 + autoGainMix * (autoGainValue - 1.0);

    ease (amount, target.amount / 100.0);
    ease (shape, target.shape / 100.0);
    ease (restoreAmount, target.restore / 100.0);
    ease (mix, target.mix / 100.0);
    ease (outputGain, std::pow (10.0, target.outputDb / 20.0));
    ease (bypassMix, target.bypass ? 1.0 : 0.0);
    ease (deltaMix, target.delta ? 1.0 : 0.0);
    ease (autoGainMix, target.autoGain ? 1.0 : 0.0);

    // Straight after a reset there is nothing to ramp from: start at the settings as they are.
    if (firstChunk)
    {
        mix0 = mix;
        out0 = outputGain;
        bypass0 = bypassMix;
        delta0 = deltaMix;
        auto0 = 1.0 + autoGainMix * (autoGainValue - 1.0);
    }

    ThinSettings settings;
    // The knob is eased at the bottom, so the first third of its travel already does something clear.
    settings.amount = amount > 0.0 ? std::pow (amount, 0.7) : 0.0;
    settings.harmonics = std::min (1.0, 2.0 * (1.0 - shape));
    settings.body = std::min (1.0, 2.0 * shape);
    settings.character = target.character;

    for (int c = 0; c < channels; ++c)
        std::copy (io[c] + offset, io[c] + offset + n, inBuf[(size_t) c].begin());

    if (hqActive)
        spectral.process (inConstPtr.data(), removedPtr.data(), n, settings);
    else
        live.process (inConstPtr.data(), removedPtr.data(), n, settings);

    // The dry signal, lined up with what the engine removed.
    const int delay = latency();
    for (int c = 0; c < channels; ++c)
    {
        auto& dry = dryBuf[(size_t) c];
        const auto& in = inBuf[(size_t) c];

        if (delay > 0)
        {
            auto& line = delayLine[(size_t) c];
            int p = delayPos;
            for (int i = 0; i < n; ++i)
            {
                dry[(size_t) i] = line[(size_t) p];
                line[(size_t) p] = in[(size_t) i];
                if (++p == delay)
                    p = 0;
            }
        }
        else
        {
            std::copy (in.begin(), in.begin() + n, dry.begin());
        }

        auto& wet = wetBuf[(size_t) c];
        const auto& removed = removedBuf[(size_t) c];
        for (int i = 0; i < n; ++i)
            wet[(size_t) i] = dry[(size_t) i] - removed[(size_t) i];
    }

    if (delay > 0)
        delayPos = (delayPos + n) % delay;

    restore.process (wetPtr.data(), n, restoreAmount, target.character);

    // Loudness of the dry signal against the mixed result, for Auto gain.
    double drySum = 0.0, wetSum = 0.0;
    for (int c = 0; c < channels; ++c)
    {
        for (int i = 0; i < n; ++i)
        {
            const double d = dryBuf[(size_t) c][(size_t) i];
            const double w = wetBuf[(size_t) c][(size_t) i];
            const double r = mix >= 1.0 ? w : d + mix * (w - d);
            // Loudness, not raw energy: both go through the same K-weighting.
            const double dk = dryWeighting.process (c, d), rk = wetWeighting.process (c, r);
            drySum += dk * dk;
            wetSum += rk * rk;
        }
    }

    const double powerA = 1.0 - std::exp (-n / (0.8 * sr));
    dryPower += powerA * (drySum / (n * channels) - dryPower);
    wetPower += powerA * (wetSum / (n * channels) - wetPower);

    if (dryPower > 1.0e-9)
    {
        constexpr double floor = 1.0e-12;
        // Up to 24 dB of make-up: enough for Character at full, short of turning a near-silent result into a blast.
        const double wanted = std::clamp (std::sqrt ((dryPower + floor) / (wetPower + floor)), 0.5, 16.0);
        autoGainValue += (1.0 - std::exp (-n / (0.15 * sr))) * (wanted - autoGainValue);
        if (std::abs (wanted - autoGainValue) < 1.0e-9)
            autoGainValue = wanted;
    }

    const double auto1 = 1.0 + autoGainMix * (autoGainValue - 1.0);
    const bool steady = mix0 == mix && out0 == outputGain && bypass0 == bypassMix && delta0 == deltaMix && auto0 == auto1;

    for (int i = 0; i < n; ++i)
    {
        const double t = steady ? 1.0 : (i + 1.0) / n;
        const double m = mix0 + t * (mix - mix0);
        const double og = out0 + t * (outputGain - out0);
        const double bp = bypass0 + t * (bypassMix - bypass0);
        const double dm = delta0 + t * (deltaMix - delta0);
        const double ag = auto0 + t * (auto1 - auto0);

        double fade = 1.0;
        if (fadeRemaining > 0)
        {
            fade = 1.0 - fadeRemaining / (double) fadeLength;
            --fadeRemaining;
        }

        double tapDry = 0.0, tapResult = 0.0;

        for (int c = 0; c < channels; ++c)
        {
            const double d = dryBuf[(size_t) c][(size_t) i];
            const double w = wetBuf[(size_t) c][(size_t) i];
            const double result = m >= 1.0 ? w : d + m * (w - d);

            double out = result * ag;
            if (dm > 0.0)
                out += dm * (m * (d - w) - out);

            out *= og * fade;
            if (bp > 0.0)
                out += bp * (d - out);

            io[c][offset + i] = out;

            tapDry += d;
            tapResult += bp > 0.0 ? result + bp * (d - result) : result;
        }

        const size_t index = (size_t) (offset + i);
        if (index < tapIn.size())
        {
            tapIn[index] = tapDry / channels;
            tapOut[index] = tapResult / channels;
        }
    }
}

} // namespace desat
