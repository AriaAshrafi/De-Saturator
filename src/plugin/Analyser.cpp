#include "Analyser.h"

#include <cmath>

Analyser::Analyser()
{
    queueInput.assign ((size_t) queueSize, 0.0f);
    queueResult.assign ((size_t) queueSize, 0.0f);
    ringInput.assign ((size_t) fftSize, 0.0f);
    ringResult.assign ((size_t) fftSize, 0.0f);

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / fftSize);

    time.assign ((size_t) fftSize, 0.0);
    re.assign ((size_t) fftSize / 2 + 1, 0.0);
    im.assign ((size_t) fftSize / 2 + 1, 0.0);
    power.assign ((size_t) fftSize / 2 + 1, 0.0);

    shownInput.fill (floorDb);
    shownResult.fill (floorDb);
}

double Analyser::lineFrequency (int j) noexcept
{
    return 20.0 * std::pow (1000.0, (j + 0.5) / numLines);
}

void Analyser::prepare (double sampleRate)
{
    sr = sampleRate;
}

void Analyser::push (const double* input, const double* result, int numSamples) noexcept
{
    // If the window isn't reading (closed, or behind), the newest audio is simply dropped.
    const auto scope = fifo.write (juce::jmin (numSamples, fifo.getFreeSpace()));

    for (int i = 0; i < scope.blockSize1; ++i)
    {
        queueInput[(size_t) (scope.startIndex1 + i)] = (float) input[i];
        queueResult[(size_t) (scope.startIndex1 + i)] = (float) result[i];
    }
    for (int i = 0; i < scope.blockSize2; ++i)
    {
        queueInput[(size_t) (scope.startIndex2 + i)] = (float) input[scope.blockSize1 + i];
        queueResult[(size_t) (scope.startIndex2 + i)] = (float) result[scope.blockSize1 + i];
    }
}

void Analyser::update (double seconds)
{
    {
        const auto scope = fifo.read (fifo.getNumReady());

        auto take = [this] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                ringInput[(size_t) ringPos] = queueInput[(size_t) (start + i)];
                ringResult[(size_t) ringPos] = queueResult[(size_t) (start + i)];
                if (++ringPos == fftSize)
                    ringPos = 0;
            }
        };

        take (scope.startIndex1, scope.blockSize1);
        take (scope.startIndex2, scope.blockSize2);

        // Nothing new (the host has stopped calling us): let the display fall away.
        if (scope.blockSize1 + scope.blockSize2 == 0)
        {
            std::fill (ringInput.begin(), ringInput.end(), 0.0f);
            std::fill (ringResult.begin(), ringResult.end(), 0.0f);
        }
    }

    std::array<float, numLines> nowInput, nowResult;
    measure (ringInput, nowInput);
    measure (ringResult, nowResult);

    settle (shownInput, nowInput, seconds);
    settle (shownResult, nowResult, seconds);

    signal = false;
    for (float v : shownInput)
        signal = signal || v > -100.0f;
}

void Analyser::setForDisplay (const std::array<float, numLines>& inputDb, const std::array<float, numLines>& resultDb)
{
    shownInput = inputDb;
    shownResult = resultDb;
    signal = false;
    for (float v : shownInput)
        signal = signal || v > -100.0f;
}

void Analyser::measure (const std::vector<float>& ring, std::array<float, numLines>& out)
{
    int p = ringPos;
    for (int i = 0; i < fftSize; ++i)
    {
        time[(size_t) i] = ring[(size_t) p] * window[(size_t) i];
        if (++p == fftSize)
            p = 0;
    }

    fft.forward (time.data(), re.data(), im.data());

    // Scaled so a full-scale sine's peak bin reads 1 (the Hann window halves it).
    const double scale = 4.0 / fftSize;
    for (int k = 0; k <= fftSize / 2; ++k)
        power[(size_t) k] = (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]) * scale * scale;

    const double binHz = sr / fftSize;
    const double halfStep = std::pow (1000.0, 0.5 / numLines); // half a line's width, as a ratio
    const int lastBin = fftSize / 2;

    std::array<float, numLines> raw;
    for (int j = 0; j < numLines; ++j)
    {
        const double f = lineFrequency (j);
        const double lo = f / halfStep / binHz, hi = f * halfStep / binHz;
        double p0 = 0.0;

        if (f >= 0.5 * sr)
        {
            p0 = 0.0;
        }
        else if (hi - lo < 1.0)
        {
            // Lines are closer together than bins down here: read between the two nearest.
            const double x = f / binHz;
            const int k = juce::jlimit (0, lastBin - 1, (int) x);
            const double frac = juce::jlimit (0.0, 1.0, x - k);
            p0 = power[(size_t) k] * (1.0 - frac) + power[(size_t) k + 1] * frac;
        }
        else
        {
            // Several bins to a line up here: the loudest one speaks for it.
            const int a = juce::jlimit (0, lastBin, (int) std::ceil (lo));
            const int b = juce::jlimit (a, lastBin, (int) std::floor (hi));
            for (int k = a; k <= b; ++k)
                p0 = std::max (p0, power[(size_t) k]);
        }

        const double tilt = 4.5 * std::log2 (f / 1000.0);
        raw[(size_t) j] = (float) juce::jmax ((double) floorDb, 10.0 * std::log10 (p0 + 1.0e-30) + tilt);
    }

    for (int j = 0; j < numLines; ++j)
    {
        const float a = raw[(size_t) std::max (0, j - 1)], b = raw[(size_t) j], c = raw[(size_t) std::min (numLines - 1, j + 1)];
        out[(size_t) j] = 0.25f * a + 0.5f * b + 0.25f * c;
    }
}

void Analyser::settle (std::array<float, numLines>& shown, const std::array<float, numLines>& now, double seconds)
{
    const float fall = (float) (28.0 * seconds);
    for (int j = 0; j < numLines; ++j)
        shown[(size_t) j] = std::max (now[(size_t) j], std::max (floorDb, shown[(size_t) j] - fall));
}
