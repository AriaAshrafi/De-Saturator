#pragma once

// Test material for the render rig and the tests: sounds with known properties, made in code.

#include "wav.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace signals
{

constexpr double pi = 3.14159265358979323846;

struct Random
{
    uint32_t state = 0x1234567u;
    double next() // -1 to 1
    {
        state = state * 1664525u + 1013904223u;
        return (double) (int32_t) state / 2147483648.0;
    }
};

inline wav::Audio make (double sr, double seconds)
{
    wav::Audio a;
    a.sampleRate = sr;
    a.channels.assign (2, std::vector<double> ((size_t) (sr * seconds), 0.0));
    return a;
}

inline void normalise (wav::Audio& a, double peak = 0.7)
{
    double m = 0.0;
    for (auto& c : a.channels)
        for (double v : c)
            m = std::max (m, std::abs (v));
    if (m > 0.0)
        for (auto& c : a.channels)
            for (double& v : c)
                v *= peak / m;
}

/** A band-limited saw, summed from sines. */
inline double saw (double phase, double freq, double sr)
{
    double v = 0.0;
    for (int h = 1; h * freq < 0.45 * sr && h <= 80; ++h)
        v += std::sin (2.0 * pi * h * phase) / h;
    return v * 0.6;
}

/** A sustained chord of saws pushed hard through a soft clipper: dense, full of intermodulation. */
inline wav::Audio saturatedChord (double sr, double seconds = 6.0)
{
    auto a = make (sr, seconds);
    const double notes[] = { 110.0, 138.59, 164.81, 220.0 };
    for (size_t n = 0; n < a.channels[0].size(); ++n)
    {
        const double t = n / sr;
        double l = 0.0, r = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            const double detune = 1.0 + 0.002 * i;
            l += saw (t * notes[i], notes[i], sr);
            r += saw (t * notes[i] * detune, notes[i], sr);
        }
        a.channels[0][n] = std::tanh (1.6 * l);
        a.channels[1][n] = std::tanh (1.6 * r);
    }
    normalise (a);
    return a;
}

/** The same chord, clean, for comparing against. */
inline wav::Audio cleanChord (double sr, double seconds = 6.0)
{
    auto a = make (sr, seconds);
    const double notes[] = { 110.0, 138.59, 164.81, 220.0 };
    for (size_t n = 0; n < a.channels[0].size(); ++n)
    {
        const double t = n / sr;
        double l = 0.0, r = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            l += saw (t * notes[i], notes[i], sr);
            r += saw (t * notes[i] * (1.0 + 0.002 * i), notes[i], sr);
        }
        a.channels[0][n] = l;
        a.channels[1][n] = r;
    }
    normalise (a);
    return a;
}

/** A bass line: one pitch at a time, soft-clipped, with note changes. */
inline wav::Audio saturatedBass (double sr, double seconds = 6.0)
{
    auto a = make (sr, seconds);
    const double notes[] = { 55.0, 65.41, 49.0, 73.42 };
    double phase = 0.0;
    for (size_t n = 0; n < a.channels[0].size(); ++n)
    {
        const double t = n / sr;
        const int step = (int) (t / 0.75);
        const double f = notes[step % 4];
        const double env = std::exp (-2.0 * std::fmod (t, 0.75));
        phase += f / sr;
        const double v = std::tanh (3.0 * env * saw (phase, f, sr));
        a.channels[0][n] = a.channels[1][n] = v;
    }
    normalise (a);
    return a;
}

/** Kick and noise-burst hits over a quiet sustained pad: for checking that onsets survive. */
inline wav::Audio drumsAndPad (double sr, double seconds = 6.0)
{
    auto a = make (sr, seconds);
    Random rnd;
    for (size_t n = 0; n < a.channels[0].size(); ++n)
    {
        const double t = n / sr;
        const double beat = std::fmod (t, 0.5);
        const double kick = std::sin (2.0 * pi * (50.0 * beat + 60.0 * (1.0 - std::exp (-30.0 * beat)) / 30.0)) * std::exp (-9.0 * beat);
        const double hatPhase = std::fmod (t + 0.25, 0.5);
        const double hat = rnd.next() * std::exp (-60.0 * hatPhase) * 0.5;
        const double pad = 0.25 * std::tanh (1.5 * (saw (t * 196.0, 196.0, sr) + saw (t * 246.94, 246.94, sr) + saw (t * 293.66, 293.66, sr)));
        a.channels[0][n] = kick + hat + pad;
        a.channels[1][n] = kick + hat * 0.8 + pad;
    }
    normalise (a);
    return a;
}

/** Pink-ish noise: nothing here is tonal, so thinning should stay smooth and not bubble. */
inline wav::Audio noise (double sr, double seconds = 6.0)
{
    auto a = make (sr, seconds);
    Random rnd;
    for (auto& c : a.channels)
    {
        double b0 = 0, b1 = 0, b2 = 0;
        for (double& v : c)
        {
            const double w = rnd.next();
            b0 = 0.99765 * b0 + w * 0.0990460;
            b1 = 0.96300 * b1 + w * 0.2965164;
            b2 = 0.57000 * b2 + w * 1.0526913;
            v = b0 + b1 + b2 + w * 0.1848;
        }
    }
    normalise (a);
    return a;
}

/** One pure tone. */
inline wav::Audio sine (double sr, double freq = 1000.0, double seconds = 4.0)
{
    auto a = make (sr, seconds);
    for (size_t n = 0; n < a.channels[0].size(); ++n)
        a.channels[0][n] = a.channels[1][n] = 0.5 * std::sin (2.0 * pi * freq * n / sr);
    return a;
}

struct Named
{
    std::string name;
    wav::Audio audio;
};

inline std::vector<Named> all (double sr)
{
    return {
        { "chord-saturated", saturatedChord (sr) },
        { "chord-clean", cleanChord (sr) },
        { "bass-saturated", saturatedBass (sr) },
        { "drums-pad", drumsAndPad (sr) },
        { "noise", noise (sr) },
        { "sine-1k", sine (sr) },
    };
}

} // namespace signals
