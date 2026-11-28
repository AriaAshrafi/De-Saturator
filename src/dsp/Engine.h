#pragma once

#include "Curve.h"
#include "LiveThinner.h"
#include "Restore.h"
#include "SpectralThinner.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace desat
{

/** The plugin's parameters, in the units the window shows. */
struct EngineParams
{
    double amount = 30.0;    // %
    double shape = 50.0;     // %, 0 = Harmonics, 100 = Body
    double restore = 0.0;    // %
    double mix = 100.0;      // %
    double outputDb = 0.0;   // -24 to +36
    bool autoGain = true;
    bool delta = false;
    bool character = false;  // false = Clean
    bool hq = true;          // false = Live
    bool bypass = false;
};

/** The K-weighting of ITU-R BS.1770: how loud a signal is to the ear, not just how much energy it has.
    A high shelf (+4 dB above about 1.5 kHz) and a high-pass at 38 Hz. Auto gain measures through it. */
class KWeighting
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        const double pi = 3.14159265358979323846;
        {
            const double K = std::tan (pi * 1681.974450955533 / sampleRate), Q = 0.7071752369554196;
            const double Vh = std::pow (10.0, 3.999843853973347 / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                      2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double K = std::tan (pi * 38.13547087602444 / sampleRate), Q = 0.5003270373238773;
            const double a0 = 1.0 + K / Q + K * K;
            highPass = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        state.assign ((size_t) numChannels, {});
    }

    void reset() { std::fill (state.begin(), state.end(), State {}); }

    double process (int channel, double x) noexcept
    {
        auto& s = state[(size_t) channel];
        const double y = tick (shelf, s.a, x);
        return tick (highPass, s.b, y);
    }

private:
    struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct Pair { double z1 = 0, z2 = 0; };
    struct State { Pair a, b; };

    static double tick (const Coefs& c, Pair& z, double x) noexcept
    {
        const double y = c.b0 * x + z.z1;
        z.z1 = c.b1 * x - c.a1 * y + z.z2;
        z.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    Coefs shelf, highPass;
    std::vector<State> state;
};

/** Everything between the plugin's input and output.

    input -> Restore -> thinning (HQ or Live) -> Mix -> [analyser tap] -> Delta / Auto gain -> Output

    The thinning engines report what they remove and it is subtracted from the (delayed) input,
    so nothing-to-remove means bit-identical audio, and Delta is exactly that removed signal.
*/
class Engine
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlockSize);
    void reset();

    void setParams (const EngineParams& p) noexcept { target = p; }
    void setCurve (const Curve& c);

    /** Latency of the mode currently running, in samples. Changes when Live / HQ is switched. */
    int latency() const noexcept { return hqActive ? spectral.latency() : 0; }

    /** Latency a mode would have at a sample rate, without preparing anything. */
    static int latencyFor (double sampleRate, bool hq) noexcept;

    void process (double* const* io, int numSamples);

    /** The curve as it is applied: curveDisplayPoints values, 0 to 2. */
    void effectiveCurve (float* out) const noexcept;

    /** Mono taps of the last process() call: the delayed input, and the result after Mix. */
    const double* inputTap() const noexcept { return tapIn.data(); }
    const double* resultTap() const noexcept { return tapOut.data(); }

    /** The drive Restore has measured in the incoming audio: 0 = not saturated, towards 1 = driven hard. */
    double matchedDrive() const noexcept { return restore.matchedDrive(); }

    /** The auto-gain correction being applied right now, as a linear factor. */
    double autoGainNow() const noexcept { return autoGainValue; }

private:
    void processChunk (double* const* io, int offset, int n);

    static constexpr int chunkSize = 32;

    double sr = 48000.0;
    int channels = 2;

    EngineParams target;
    Curve curve;

    SpectralThinner spectral;
    LiveThinner live;
    Restore restore;

    bool hqActive = true;
    int fadeRemaining = 0, fadeLength = 1;

    // Smoothed controls.
    double amount = 0.0, shape = 0.5, restoreAmount = 0.0, mix = 1.0, outputGain = 1.0;
    double bypassMix = 0.0, autoGainMix = 1.0, deltaMix = 0.0;
    bool primed = false;

    // Auto gain.
    double dryPower = 0.0, wetPower = 0.0, autoGainValue = 1.0;
    KWeighting dryWeighting, wetWeighting;

    // Dry delay (HQ only).
    std::vector<std::vector<double>> delayLine, restoredDelayLine;
    int delayPos = 0;

    std::vector<std::vector<double>> inBuf, removedBuf, dryBuf, wetBuf;
    std::vector<double*> inPtr, removedPtr, wetPtr;
    std::vector<const double*> inConstPtr, wetConstPtr;
    std::vector<double> tapIn, tapOut;
};

} // namespace desat
