#pragma once

#include "Curve.h"
#include "LiveThinner.h"
#include "Restore.h"
#include "SpectralThinner.h"

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
    double outputDb = 0.0;   // -24 to +12
    bool autoGain = true;
    bool delta = false;
    bool character = false;  // false = Clean
    bool hq = true;          // false = Live
    bool bypass = false;
};

/** Everything between the plugin's input and output.

    input -> thinning (HQ or Live) -> Restore -> Mix -> [analyser tap] -> Delta / Auto gain -> Output

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
    double smoothing = 1.0;
    bool primed = false;

    // Auto gain.
    double dryPower = 0.0, wetPower = 0.0, autoGainValue = 1.0;
    double powerCoef = 0.0, autoGainCoef = 0.0;

    // Dry delay (HQ only).
    std::vector<std::vector<double>> delayLine;
    int delayPos = 0;

    std::vector<std::vector<double>> inBuf, removedBuf, dryBuf, wetBuf;
    std::vector<double*> inPtr, removedPtr, wetPtr;
    std::vector<const double*> inConstPtr;
    std::vector<double> tapIn, tapOut;
};

} // namespace desat
