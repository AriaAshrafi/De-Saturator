#pragma once

#include "Curve.h"

#include <array>
#include <vector>

namespace desat
{

/** The Live engine: thinning with no latency.

    Everything is a chain of minimum-phase bell filters whose gains move with the signal:
      - ten bands that dip where the band is a valley beside its neighbours, or sustained and dense;
      - one body bell in the low mids;
      - eight notches that follow the overtones of a pitch tracker, and fade out when the
        material isn't one clear pitch.
    A bell at 0 dB passes its input through exactly, so with nothing to remove the output is the input.

    The thinning curve can't be followed bin by bin here, so it is averaged onto the ten bands;
    effectiveCurve() reports what is really applied.
*/
class LiveThinner
{
public:
    static constexpr int numBands = 10;
    static constexpr int numNotches = 8;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setCurve (const Curve& curve);

    /** curveDisplayPoints values, 0 to 2, on the curveDisplayFreq() axis. */
    void effectiveCurve (float* out) const noexcept;

    /** Writes what was removed (input minus output). No delay. */
    void process (const double* const* in, double* const* removed, int numSamples, const ThinSettings& settings);

private:
    struct BellCoef
    {
        double g = 0.0, q = 1.0;
        double a1 = 1.0, a2 = 0.0, a3 = 0.0, m1 = 0.0;
        void setFrequency (double freq, double sampleRate) noexcept;
        void setGain (double gainDb) noexcept;
    };

    struct State
    {
        double ic1 = 0.0, ic2 = 0.0;
    };

    struct PitchTracker
    {
        void prepare (double sampleRate);
        void reset();
        bool push (double x) noexcept; // true when a new estimate is ready

        double f0 = 0.0, aperiodicity = 1.0;

    private:
        void estimate() noexcept;

        double srDecimated = 11025.0;
        int decimation = 4, phase = 0;
        int window = 512, tauMin = 11, tauMax = 245, hop = 128, fill = 0;
        BellCoef lpA, lpB;
        State sA, sB;
        std::vector<double> buffer, diff;
    };

    void updateControl (const ThinSettings& settings);
    double effectiveAt (double freqHz) const noexcept;

    double sr = 48000.0;
    int channels = 2;
    int controlCounter = 0;

    Curve curve;
    std::array<double, numBands> centre {}, bandCurve {};
    std::array<float, curveDisplayPoints> effective {};

    // Detection (on the mono sum).
    std::array<BellCoef, numBands> analysisCoef;
    std::array<State, numBands> analysisState;
    std::array<double, numBands> peak {}, meanSquare {}, peakRelease {}, rmsCoef {};
    std::array<double, numBands> reduction {};
    double bodyReduction = 0.0;

    PitchTracker pitch;
    double f0 = 0.0, confidence = 0.0, confidenceTarget = 0.0, f0Target = 0.0;
    std::array<double, numNotches> notchReduction {};

    // The audio chain: coefficients are shared, state is per channel.
    std::array<BellCoef, numBands> bandCoef;
    BellCoef bodyCoef;
    std::array<BellCoef, numNotches> notchCoef;

    struct ChannelState
    {
        std::array<State, numBands> band;
        State body;
        std::array<State, numNotches> notch;
    };
    std::vector<ChannelState> chan;
};

} // namespace desat
