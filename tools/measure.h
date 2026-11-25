#pragma once

// Running the engine offline and measuring what it did. Shared by the render rig and the tests.

#include "dsp/Engine.h"
#include "dsp/Fft.h"
#include "wav.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace measure
{

struct Run
{
    wav::Audio out;      // the plugin's output, shifted back by the latency so it lines up with the input
    int latency = 0;
    double realtime = 0; // seconds of audio per second of CPU
};

/** Runs the whole engine over a file in host-sized blocks. */
inline Run process (const wav::Audio& in, const desat::EngineParams& params, const desat::Curve& curve = {}, int blockSize = 512)
{
    const int numCh = in.numChannels(), total = in.numSamples();

    desat::Engine engine;
    engine.setParams (params);
    engine.setCurve (curve);
    engine.prepare (in.sampleRate, numCh, blockSize);

    Run run;
    run.latency = engine.latency();
    run.out.sampleRate = in.sampleRate;
    run.out.channels.assign ((size_t) numCh, std::vector<double> ((size_t) total, 0.0));

    std::vector<std::vector<double>> block ((size_t) numCh, std::vector<double> ((size_t) blockSize));
    std::vector<double*> ptr ((size_t) numCh);
    for (int c = 0; c < numCh; ++c)
        ptr[(size_t) c] = block[(size_t) c].data();

    const auto start = std::chrono::steady_clock::now();

    for (int pos = 0; pos < total + run.latency; pos += blockSize)
    {
        const int n = std::min (blockSize, total + run.latency - pos);
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
                block[(size_t) c][(size_t) i] = pos + i < total ? in.channels[(size_t) c][(size_t) (pos + i)] : 0.0;

        engine.process (ptr.data(), n);

        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
                if (pos + i >= run.latency)
                    run.out.channels[(size_t) c][(size_t) (pos + i - run.latency)] = block[(size_t) c][(size_t) i];
    }

    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    run.realtime = elapsed > 0.0 ? (total / in.sampleRate) / elapsed : 0.0;
    return run;
}

inline double db (double power) { return 10.0 * std::log10 (power + 1.0e-30); }

/** Mean power over all channels, skipping the first `skip` seconds (settling). */
inline double power (const wav::Audio& a, double skip = 0.5)
{
    const size_t from = std::min ((size_t) (skip * a.sampleRate), (size_t) a.numSamples());
    double sum = 0.0;
    size_t count = 0;
    for (auto& c : a.channels)
        for (size_t i = from; i < c.size(); ++i, ++count)
            sum += c[i] * c[i];
    return count > 0 ? sum / count : 0.0;
}

inline double peak (const wav::Audio& a)
{
    double m = 0.0;
    for (auto& c : a.channels)
        for (double v : c)
            m = std::max (m, std::abs (v));
    return m;
}

inline wav::Audio difference (const wav::Audio& a, const wav::Audio& b)
{
    wav::Audio d = a;
    for (size_t c = 0; c < d.channels.size(); ++c)
        for (size_t i = 0; i < d.channels[c].size(); ++i)
            d.channels[c][i] -= b.channels[c][i];
    return d;
}

inline double crestDb (const wav::Audio& a)
{
    const double p = peak (a);
    return db (p * p) - db (power (a, 0.0));
}

/** Long-term power spectrum of the mono sum (Hann, 8192, half overlap), from `skip` seconds in. */
struct Spectrum
{
    double binHz = 1.0;
    std::vector<double> power;

    double band (double lo, double hi) const
    {
        double sum = 0.0;
        for (size_t k = (size_t) (lo / binHz); k < power.size() && k * binHz < hi; ++k)
            sum += power[k];
        return sum;
    }
};

inline Spectrum spectrum (const wav::Audio& a, double skip = 0.5)
{
    constexpr int N = 8192;
    const double pi = 3.14159265358979323846;
    desat::RealFft fft (N);
    std::vector<double> x ((size_t) N), re ((size_t) N / 2 + 1), im ((size_t) N / 2 + 1);

    Spectrum s;
    s.binHz = a.sampleRate / N;
    s.power.assign ((size_t) N / 2 + 1, 0.0);

    int frames = 0;
    for (int start = (int) (skip * a.sampleRate); start + N <= a.numSamples(); start += N / 2, ++frames)
    {
        for (int i = 0; i < N; ++i)
        {
            double v = 0.0;
            for (auto& c : a.channels)
                v += c[(size_t) (start + i)];
            x[(size_t) i] = v / a.numChannels() * (0.5 - 0.5 * std::cos (2.0 * pi * i / N));
        }
        fft.forward (x.data(), re.data(), im.data());
        for (int k = 0; k <= N / 2; ++k)
            s.power[(size_t) k] += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
    }

    if (frames > 0)
        for (double& p : s.power)
            p /= frames;
    return s;
}

/** How "filled in" a spectrum is between its peaks: the mean depth of the valleys below the local
    peak envelope, in dB, from 60 Hz to 10 kHz. Thinning should raise it. */
inline double valleyDepthDb (const Spectrum& s)
{
    const int lo = (int) (60.0 / s.binHz), hi = std::min ((int) s.power.size() - 1, (int) (10000.0 / s.binHz));
    double sum = 0.0;
    for (int k = lo; k <= hi; ++k)
    {
        const int w = std::max (4, (int) (k * 0.12)); // about a sixth of an octave either side
        double m = 0.0;
        for (int j = std::max (lo, k - w); j <= std::min (hi, k + w); ++j)
            m = std::max (m, s.power[(size_t) j]);
        sum += db (m) - db (s.power[(size_t) k]);
    }
    return sum / (hi - lo + 1);
}

/** Level wobble: how much the short-term level in a band jumps from frame to frame, in dB (standard
    deviation). On steady noise a rise after processing means the thinning is bubbling. */
inline double flutterDb (const wav::Audio& a, double loHz, double hiHz, double skip = 0.5)
{
    constexpr int N = 1024;
    const double pi = 3.14159265358979323846;
    desat::RealFft fft (N);
    std::vector<double> x ((size_t) N), re ((size_t) N / 2 + 1), im ((size_t) N / 2 + 1);
    const double binHz = a.sampleRate / N;

    std::vector<double> levels;
    for (int start = (int) (skip * a.sampleRate); start + N <= a.numSamples(); start += N / 2)
    {
        for (int i = 0; i < N; ++i)
            x[(size_t) i] = a.channels[0][(size_t) (start + i)] * (0.5 - 0.5 * std::cos (2.0 * pi * i / N));
        fft.forward (x.data(), re.data(), im.data());
        double p = 0.0;
        for (int k = (int) (loHz / binHz); k * binHz < hiHz && k <= N / 2; ++k)
            p += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
        levels.push_back (db (p));
    }

    if (levels.size() < 3)
        return 0.0;

    double mean = 0.0, var = 0.0;
    for (size_t i = 1; i < levels.size(); ++i)
        mean += levels[i] - levels[i - 1];
    mean /= (levels.size() - 1);
    for (size_t i = 1; i < levels.size(); ++i)
    {
        const double d = levels[i] - levels[i - 1] - mean;
        var += d * d;
    }
    return std::sqrt (var / (levels.size() - 1));
}

} // namespace measure
