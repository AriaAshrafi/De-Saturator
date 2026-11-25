#pragma once

#include "dsp/Fft.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

/** What the window's analyser draws: the input and the result as 120 levels on a log axis.

    The audio thread pushes samples into a lock-free queue; the window calls update() from its
    timer, which transforms the latest stretch of each signal and applies the display ballistics
    from the design spec (rise at once, fall 28 dB a second, 1-2-1 smoothing across neighbours,
    a 4.5 dB/octave tilt around 1 kHz). Levels are in dB, where a full-scale sine reads 0 before the tilt.
*/
class Analyser
{
public:
    static constexpr int numLines = 120;
    static constexpr float floorDb = -120.0f;

    Analyser();

    /** The frequency the j-th line is drawn at: 20 Hz to 20 kHz, log. */
    static double lineFrequency (int j) noexcept;

    void prepare (double sampleRate);

    /** Audio thread. Both are mono, the same length, and time-aligned. */
    void push (const double* input, const double* result, int numSamples) noexcept;

    /** Message thread. Takes what has arrived and moves the display on by `seconds`. */
    void update (double seconds);

    /** Message thread. Fills both displays from given spectra instead of audio (for pictures and tests). */
    void setForDisplay (const std::array<float, numLines>& inputDb, const std::array<float, numLines>& resultDb);

    const std::array<float, numLines>& input() const noexcept { return shownInput; }
    const std::array<float, numLines>& result() const noexcept { return shownResult; }

    /** False when there's nothing above the floor to draw. */
    bool hasSignal() const noexcept { return signal; }

private:
    static constexpr int fftSize = 4096;
    static constexpr int queueSize = 1 << 15;

    void measure (const std::vector<float>& ring, std::array<float, numLines>& out);
    static void settle (std::array<float, numLines>& shown, const std::array<float, numLines>& now, double seconds);

    double sr = 48000.0;

    juce::AbstractFifo fifo { queueSize };
    std::vector<float> queueInput, queueResult;

    // The last fftSize samples of each, oldest first at ringPos.
    std::vector<float> ringInput, ringResult;
    int ringPos = 0;

    desat::RealFft fft { fftSize };
    std::vector<double> window, time, re, im, power;

    std::array<float, numLines> shownInput {}, shownResult {};
    bool signal = false;
};
