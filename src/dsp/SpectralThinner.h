#pragma once

#include "Curve.h"
#include "Fft.h"

#include <array>
#include <memory>
#include <vector>

namespace desat
{

/** The HQ engine: a multi-resolution spectral thinner.

    Three STFTs run side by side (long windows for the lows, short for the highs), each owning a
    band of the spectrum. Each one works out, bin by bin, how much to take away, and resynthesises
    only *what is removed*. The caller subtracts that from the delayed input, so wherever nothing
    is removed the output is the input exactly.

    What gets removed, per bin:
      - fill:      whatever sits in the valleys between the spectral peaks (a spectral expander);
      - contrast:  third-octave bands that already dip below their neighbours are pulled down further;
      - harmonics: the overtones (2nd and up) of the pitched sounds it can find;
      - body:      a dynamic bell in the low mids.
    Onsets are detected per band and let through.
*/
class SpectralThinner
{
public:
    SpectralThinner();
    ~SpectralThinner();

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Samples of delay between the input and the matching `removed` output. */
    int latency() const noexcept { return latencySamples; }

    void setCurve (const Curve& curve);

    /** Writes what should be subtracted from the input delayed by latency(). */
    void process (const double* const* in, double* const* removed, int numSamples, const ThinSettings& settings);

private:
    struct Resolution;
    struct HarmonicEntry { double freq; double weight; };

    void frame (int index, const ThinSettings& settings);
    void analyseHarmonics (Resolution& r);
    void analyseContrast (Resolution& r);
    void rebuildCurve();

    double sr = 48000.0;
    int channels = 2;
    int latencySamples = 0;

    Curve curve;
    std::vector<std::unique_ptr<Resolution>> res;

    // Shared detection state, written by the longest resolution.
    std::vector<HarmonicEntry> harmonics;
    double harmonicConfidence = 0.0;
    double bodyDynamics = 1.0;

    // Third-octave bands from 50 Hz up, for the contrast term.
    static constexpr int numContrastBands = 27;
    std::array<int, numContrastBands> bandLo {}, bandHi {};
    std::array<double, numContrastBands> bandTilt {}, contrast {};
    std::array<bool, numContrastBands> bandAbove {};

    // Scratch for the harmonic analysis.
    std::vector<double> compressed, compressedWork;
    std::vector<char> claimed;
    std::vector<double> candidateFreqs;
};

} // namespace desat
