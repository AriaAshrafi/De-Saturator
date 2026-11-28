#include "SpectralThinner.h"

#include "FastMath.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace desat
{

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double tiny = 1.0e-30;

/** How far each mode is allowed to go. All dB figures are per unit of depth (Amount x curve). */
struct ModeConstants
{
    double fillPerDb;         // dB removed per dB of valley depth
    double fillCap;           // most the fill term can remove
    double harmonicsDb;       // most an overtone is reduced
    double bodyDb;            // depth of the body bell
    double contrastPerDb;     // dB removed per dB a third-octave band sits under its neighbours
    double contrastCap;       // most the contrast term can remove
    double maxDb;             // ceiling for everything together
    double transientProtect;  // how much of the reduction an onset cancels
};

constexpr ModeConstants cleanMode     { 1.5, 24.0, 12.0, 12.0, 0.5, 12.0, 30.0, 0.85 };
constexpr ModeConstants characterMode { 3.0, 48.0, 30.0, 24.0, 1.2, 30.0, 60.0, 0.60 };

// The contrast term works on third-octave bands, 50 Hz upwards.
constexpr double firstBandHz = 50.0;
constexpr double contrastKnee = 3.0;   // dB under the neighbours before a band counts as a dip
constexpr double contrastTilt = 3.0;   // dB/octave: an ordinary falling spectrum reads as level
constexpr int contrastReach = 4;       // bands either side that a band is measured against

// The long windows reach well up, because telling partials from the fill between them takes
// frequency resolution. Above that the ear hears density, not single partials, and time matters more.
constexpr double lowCrossover = 2500.0;   // Hz, long windows below
constexpr double highCrossover = 8000.0;  // Hz, short windows above
constexpr double peakKnee = 4.0;          // dB under the envelope that still counts as the peak itself
constexpr double harmonicTop = 8000.0;    // Hz, pitch analysis looks this far up
constexpr int maxSeries = 3;
constexpr int maxSnapped = 60;

double smoothstep (double x) noexcept
{
    x = std::clamp (x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

double onePole (double seconds, double stepSeconds) noexcept
{
    return 1.0 - std::exp (-stepSeconds / seconds);
}

// The windows used below are [k - bw[k], k + bw[k]], clipped to [lo, hi]. bw grows slower than k,
// so both ends only ever move right as k rises, and each can be done in one pass.

/** Maximum of x over each bin's window: a monotonic queue. */
void slidingMax (const double* x, const int* bw, int lo, int hi, double* out, int* queue) noexcept
{
    int head = 0, tail = 0, next = lo;
    for (int k = lo; k <= hi; ++k)
    {
        const int a = std::max (lo, k - bw[k]), b = std::min (hi, k + bw[k]);
        while (next <= b)
        {
            while (tail > head && x[queue[tail - 1]] <= x[next])
                --tail;
            queue[tail++] = next++;
        }
        while (queue[head] < a)
            ++head;
        out[k] = x[queue[head]];
    }
}

/** Fills prefix so that the sum of x[a..b] is prefix[b + 1] - prefix[a]. */
void prefixSums (const double* x, int lo, int hi, double* prefix) noexcept
{
    double sum = 0.0;
    prefix[lo] = 0.0;
    for (int k = lo; k <= hi; ++k)
    {
        sum += x[k];
        prefix[k + 1] = sum;
    }
}
} // namespace

//==============================================================================
struct SpectralThinner::Resolution
{
    explicit Resolution (int size) : N (size), hop (size / 4), K (size / 2 + 1), fft (size) {}

    struct Channel
    {
        std::vector<double> in, ola, inDelayLine, outDelayLine;
        std::vector<double> Xr, Xi;
    };

    int N, hop, K;
    RealFft fft;
    std::vector<double> window, time;

    // With Amount at 0 and nothing left to release, a resolution stops transforming altogether.
    bool idle = true;

    double binHz = 1.0;
    int inDelay = 0, outDelay = 0;
    int kLo = 0, kHi = 0;  // bins this resolution owns
    int aLo = 0, aHi = 0;  // bins it analyses (wider)

    double aPower = 0, aAttack = 0, aRelease = 0, holdDecay = 0;

    std::vector<double> W, curveVal, bodyBell, bandPos; // bandPos: where each bin falls among the contrast bands
    std::vector<double> P, Ps, L, mx, U, trans, hold, harm, R, coef;
    std::vector<int> bwClean, bwCharacter, queue;
    std::vector<double> prefix;

    std::vector<Channel> ch;
    int pos = 0, count = 0, inDelayPos = 0, outDelayPos = 0;
};

//==============================================================================
SpectralThinner::SpectralThinner() = default;
SpectralThinner::~SpectralThinner() = default;

void SpectralThinner::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    channels = numChannels;

    // Keep the windows the same length in time at higher sample rates.
    const int m = sr > 100000.0 ? 4 : (sr > 50000.0 ? 2 : 1);
    const std::array<int, 3> sizes { 4096 * m, 1024 * m, 256 * m };
    latencySamples = sizes[0];

    res.clear();
    for (size_t i = 0; i < sizes.size(); ++i)
    {
        auto r = std::make_unique<Resolution> (sizes[i]);
        const int N = r->N, K = r->K;
        r->binHz = sr / N;

        r->window.resize ((size_t) N);
        r->time.assign ((size_t) N, 0.0);
        for (int n = 0; n < N; ++n)
            r->window[(size_t) n] = 0.5 - 0.5 * std::cos (2.0 * pi * n / N);

        // Centre every resolution's frames on the same instant, so they can share detection.
        r->inDelay = (sizes[0] - N) / 2;
        r->outDelay = sizes[0] - N - r->inDelay;

        const double step = r->hop / sr;
        r->aPower = onePole (0.050, step);
        r->aAttack = onePole (0.025, step);
        r->aRelease = onePole (0.080, step);
        r->holdDecay = std::exp (-step / 0.040);

        for (auto* v : { &r->W, &r->curveVal, &r->bodyBell, &r->bandPos, &r->P, &r->Ps, &r->L, &r->mx, &r->U,
                         &r->trans, &r->hold, &r->harm, &r->R, &r->coef })
            v->assign ((size_t) K, 0.0);

        r->queue.assign ((size_t) K, 0);
        r->prefix.assign ((size_t) K + 1, 0.0);
        r->bwClean.assign ((size_t) K, 3);
        r->bwCharacter.assign ((size_t) K, 4);

        r->kLo = K;
        r->kHi = 0;
        for (int k = 0; k < K; ++k)
        {
            const double f = k * r->binHz;
            const double s1 = f > 0.0 ? smoothstep (std::log2 (f / lowCrossover) + 0.5) : 0.0;
            const double s2 = f > 0.0 ? smoothstep (std::log2 (f / highCrossover) + 0.5) : 0.0;
            const double w = i == 0 ? 1.0 - s1 : (i == 1 ? s1 * (1.0 - s2) : s1 * s2);
            r->W[(size_t) k] = w;
            if (w > 0.0)
            {
                r->kLo = std::min (r->kLo, k);
                r->kHi = std::max (r->kHi, k);
            }

            r->bwClean[(size_t) k] = std::max (3, (int) std::lround (k * (std::pow (2.0, 1.0 / 6.0) - 1.0)));
            r->bwCharacter[(size_t) k] = std::max (4, (int) std::lround (k * (std::pow (2.0, 1.0 / 3.0) - 1.0)));

            const double x = f > 0.0 ? std::log2 (f / 260.0) / (2.4 / 2.3548) : -100.0;
            r->bodyBell[(size_t) k] = std::exp (-0.5 * x * x);
            r->bandPos[(size_t) k] = f > 0.0 ? std::clamp (3.0 * std::log2 (f / firstBandHz), 0.0, (double) (numContrastBands - 1)) : 0.0;
        }

        r->kLo = std::max (r->kLo, 1); // never touch DC
        const int margin = 2 * r->bwCharacter[(size_t) r->kHi] + 2;
        r->aLo = std::max (1, r->kLo - margin);
        r->aHi = std::min (K - 2, r->kHi + margin);
        if (i == 0)
        {
            r->aLo = 1;
            r->aHi = K - 2; // the whole spectrum: the contrast bands are measured here
        }

        r->ch.resize ((size_t) channels);
        for (auto& c : r->ch)
        {
            c.in.assign ((size_t) N, 0.0);
            c.ola.assign ((size_t) N, 0.0);
            c.inDelayLine.assign ((size_t) std::max (1, r->inDelay), 0.0);
            c.outDelayLine.assign ((size_t) std::max (1, r->outDelay), 0.0);
            c.Xr.assign ((size_t) K, 0.0);
            c.Xi.assign ((size_t) K, 0.0);
        }

        res.push_back (std::move (r));
    }

    const int K0 = res[0]->K;
    compressed.assign ((size_t) K0, 0.0);
    compressedWork.assign ((size_t) K0, 0.0);
    claimed.assign ((size_t) K0, 0);

    candidateFreqs.clear();
    for (double f = 40.0; f <= 1200.0; f *= std::pow (2.0, 1.0 / 48.0))
        candidateFreqs.push_back (f);

    harmonics.clear();
    harmonics.reserve (maxSeries * 256);

    // Third-octave band edges on the long resolution's bins.
    for (int b = 0; b < numContrastBands; ++b)
    {
        const double fc = firstBandHz * std::pow (2.0, b / 3.0);
        const double binHz = res[0]->binHz;
        int lo = (int) std::ceil (fc * std::pow (2.0, -1.0 / 6.0) / binHz), hi = (int) std::floor (fc * std::pow (2.0, 1.0 / 6.0) / binHz);
        lo = std::clamp (lo, 1, K0 - 2);
        hi = std::clamp (hi, lo, K0 - 2);
        bandLo[(size_t) b] = lo;
        bandHi[(size_t) b] = hi;
        bandTilt[(size_t) b] = contrastTilt * std::log2 (fc / 1000.0);
        bandAbove[(size_t) b] = fc < 0.5 * sr;
    }
    contrast.fill (0.0);

    rebuildCurve();
    reset();
}

void SpectralThinner::reset()
{
    for (auto& r : res)
    {
        for (auto* v : { &r->P, &r->Ps, &r->hold, &r->R, &r->coef })
            std::fill (v->begin(), v->end(), 0.0);

        for (auto& c : r->ch)
        {
            std::fill (c.in.begin(), c.in.end(), 0.0);
            std::fill (c.ola.begin(), c.ola.end(), 0.0);
            std::fill (c.inDelayLine.begin(), c.inDelayLine.end(), 0.0);
            std::fill (c.outDelayLine.begin(), c.outDelayLine.end(), 0.0);
        }

        r->pos = r->count = r->inDelayPos = r->outDelayPos = 0;
        r->idle = true;
    }

    harmonics.clear();
    harmonicConfidence = 0.0;
    bodyDynamics = 1.0;
    contrast.fill (0.0);
}

void SpectralThinner::setCurve (const Curve& c)
{
    curve = c;
    rebuildCurve();
}

void SpectralThinner::rebuildCurve()
{
    for (auto& r : res)
        for (int k = 0; k < r->K; ++k)
            r->curveVal[(size_t) k] = curve.valueAt (std::max (10.0, k * r->binHz));
}

//==============================================================================
void SpectralThinner::process (const double* const* in, double* const* removed, int numSamples, const ThinSettings& settings)
{
    for (int n = 0; n < numSamples; ++n)
    {
        for (int c = 0; c < channels; ++c)
            removed[c][n] = 0.0;

        for (size_t i = 0; i < res.size(); ++i)
        {
            auto& r = *res[i];

            for (int c = 0; c < channels; ++c)
            {
                auto& ch = r.ch[(size_t) c];
                double x = in[c][n];

                if (r.inDelay > 0)
                    std::swap (x, ch.inDelayLine[(size_t) r.inDelayPos]);

                ch.in[(size_t) r.pos] = x;
                double y = ch.ola[(size_t) r.pos];
                ch.ola[(size_t) r.pos] = 0.0;

                if (r.outDelay > 0)
                    std::swap (y, ch.outDelayLine[(size_t) r.outDelayPos]);

                removed[c][n] += y;
            }

            if (r.inDelay > 0 && ++r.inDelayPos == r.inDelay)
                r.inDelayPos = 0;
            if (r.outDelay > 0 && ++r.outDelayPos == r.outDelay)
                r.outDelayPos = 0;
            if (++r.pos == r.N)
                r.pos = 0;

            if (++r.count == r.hop)
            {
                r.count = 0;
                frame ((int) i, settings);
            }
        }
    }
}

//==============================================================================
void SpectralThinner::frame (int index, const ThinSettings& s)
{
    auto& r = *res[(size_t) index];
    const auto& mode = s.character ? characterMode : cleanMode;
    const auto& bw = s.character ? r.bwCharacter : r.bwClean;
    const int N = r.N;

    if (s.amount <= 0.0 && r.idle)
    {
        if (index == 0)
            harmonics.clear();
        return;
    }

    const bool waking = r.idle;
    r.idle = false;

    for (auto& ch : r.ch)
    {
        // The ring starts at pos: unwrap it in two straight runs.
        const int first = N - r.pos;
        const double* in = ch.in.data();
        const double* w = r.window.data();
        double* t = r.time.data();
        for (int i = 0; i < first; ++i)
            t[i] = in[r.pos + i] * w[i];
        for (int i = first; i < N; ++i)
            t[i] = in[i - first] * w[i];
        r.fft.forward (r.time.data(), ch.Xr.data(), ch.Xi.data());
    }

    // Power, linked across channels so both get the same gains and the image holds still.
    for (int k = r.aLo; k <= r.aHi; ++k)
    {
        double p = 0.0;
        for (auto& ch : r.ch)
            p += ch.Xr[(size_t) k] * ch.Xr[(size_t) k] + ch.Xi[(size_t) k] * ch.Xi[(size_t) k];
        r.P[(size_t) k] = p;
    }

    // Coming back from idle there is no history: start from what's here, so it doesn't read as an onset.
    if (waking)
        for (int k = r.aLo; k <= r.aHi; ++k)
            r.Ps[(size_t) k] = r.P[(size_t) k];

    // Onsets: bins that jump well above their recent average.
    const double floorPower = (N * 1.0e-6) * (N * 1.0e-6);
    for (int k = r.aLo; k <= r.aHi; ++k)
    {
        const double p = r.P[(size_t) k];
        double t = 0.0;
        if (p > floorPower)
            t = std::clamp ((fastPowerToDb (p / (r.Ps[(size_t) k] + floorPower)) - 6.0) / 12.0, 0.0, 1.0);
        r.trans[(size_t) k] = t;
    }

    prefixSums (r.trans.data(), r.aLo, r.aHi, r.prefix.data());
    for (int k = r.kLo; k <= r.kHi; ++k)
    {
        const int a = std::max (r.aLo, k - bw[(size_t) k]);
        const int b = std::min (r.aHi, k + bw[(size_t) k]);
        const double avg = (r.prefix[(size_t) b + 1] - r.prefix[(size_t) a]) / (b - a + 1);
        r.hold[(size_t) k] = std::max (avg, r.hold[(size_t) k] * r.holdDecay);
    }

    for (int k = r.aLo; k <= r.aHi; ++k)
    {
        auto& ps = r.Ps[(size_t) k];
        ps += r.aPower * (r.P[(size_t) k] - ps);
        if (ps < tiny)
            ps = 0.0;
        r.L[(size_t) k] = fastPowerToDb (ps + tiny);
    }

    if (index == 0)
    {
        analyseHarmonics (r);
        analyseContrast (r);
    }

    // The upper envelope the valleys are measured against: a sliding maximum, then a mean.
    slidingMax (r.L.data(), bw.data(), r.aLo, r.aHi, r.mx.data(), r.queue.data());
    prefixSums (r.mx.data(), r.aLo, r.aHi, r.prefix.data());

    for (int k = r.kLo; k <= r.kHi; ++k)
    {
        const int a = std::max (r.aLo, k - bw[(size_t) k]);
        const int b = std::min (r.aHi, k + bw[(size_t) k]);
        r.U[(size_t) k] = (r.prefix[(size_t) b + 1] - r.prefix[(size_t) a]) / (b - a + 1);
        r.harm[(size_t) k] = 0.0;
    }

    // Where the overtones are, on this resolution's bins.
    for (const auto& e : harmonics)
    {
        const double kc = e.freq / r.binHz;
        if (kc < r.kLo - 3 || kc > r.kHi + 3)
            continue;

        // Just the partial's own main lobe, so the fill beside it is left to the fill term.
        const double hw = 1.75;
        const int a = std::max (r.kLo, (int) std::ceil (kc - hw));
        const int b = std::min (r.kHi, (int) std::floor (kc + hw));
        for (int k = a; k <= b; ++k)
        {
            const double u = (k - kc) / hw;
            const double w = e.weight * (1.0 - u * u) * (1.0 - u * u); // a smooth bump, 1 at the partial, 0 at the edges
            r.harm[(size_t) k] = std::max (r.harm[(size_t) k], w);
        }
    }

    // Reduction per bin, in dB, smoothed in time.
    for (int k = r.kLo; k <= r.kHi; ++k)
    {
        const double depth = s.amount * r.curveVal[(size_t) k];
        const double valley = std::max (0.0, r.U[(size_t) k] - r.L[(size_t) k] - peakKnee);
        const double fill = std::min (mode.fillPerDb * valley, mode.fillCap);

        // The contrast term, read between the two bands this bin lies between.
        const double bp = r.bandPos[(size_t) k];
        const int b0 = std::min ((int) bp, numContrastBands - 2);
        const double dip = contrast[(size_t) b0] + (bp - b0) * (contrast[(size_t) b0 + 1] - contrast[(size_t) b0]);
        const double carve = std::min (mode.contrastPerDb * dip, mode.contrastCap);

        double target = depth * (fill + carve
                                 + s.harmonics * mode.harmonicsDb * r.harm[(size_t) k]
                                 + s.body * mode.bodyDb * r.bodyBell[(size_t) k] * bodyDynamics);
        const double held = r.hold[(size_t) k];
        target = std::min (target, mode.maxDb * depth) * (1.0 - mode.transientProtect * held);

        auto& red = r.R[(size_t) k];
        // Let go at once on an onset; otherwise ease in and out.
        const double a = target > red ? r.aAttack : r.aRelease + (1.0 - r.aRelease) * held;
        red += a * (target - red);
        if (red < 1.0e-7)
            red = 0.0;
    }

    bool any = false;
    for (int k = r.kLo; k <= r.kHi; ++k)
    {
        const double lo = r.R[(size_t) std::max (r.kLo, k - 1)];
        const double hi = r.R[(size_t) std::min (r.kHi, k + 1)];
        double red = 0.25 * lo + 0.5 * r.R[(size_t) k] + 0.25 * hi;

        // The curve's floor is exact: where it is 0, nothing is removed.
        red = std::min (red, mode.maxDb * s.amount * r.curveVal[(size_t) k]);

        const double c = red > 1.0e-6 ? (1.0 - std::exp (-0.11512925464970229 * red)) * r.W[(size_t) k] : 0.0; // 10^(-dB/20)
        r.coef[(size_t) k] = c;
        any = any || c > 0.0;
    }

    if (! any)
    {
        // Nothing to remove and nothing asked for: stop transforming until Amount comes back.
        if (s.amount <= 0.0)
        {
            bool settled = true;
            for (int k = r.kLo; k <= r.kHi && settled; ++k)
                settled = r.R[(size_t) k] == 0.0;
            r.idle = settled;
        }
        return;
    }

    // Resynthesise only what is removed.
    constexpr double overlapGain = 1.0 / 1.5; // Hann squared at 75% overlap sums to 1.5
    for (auto& ch : r.ch)
    {
        for (int k = 0; k < r.K; ++k)
        {
            const double c = (k >= r.kLo && k <= r.kHi) ? r.coef[(size_t) k] : 0.0;
            ch.Xr[(size_t) k] *= c;
            ch.Xi[(size_t) k] *= c;
        }

        r.fft.inverse (ch.Xr.data(), ch.Xi.data(), r.time.data());

        const int first = N - r.pos;
        const double* t = r.time.data();
        const double* w = r.window.data();
        double* ola = ch.ola.data();
        for (int i = 0; i < first; ++i)
            ola[r.pos + i] += t[i] * w[i] * overlapGain;
        for (int i = first; i < N; ++i)
            ola[i - first] += t[i] * w[i] * overlapGain;
    }
}

//==============================================================================
/** How far each third-octave band dips below its neighbours, in dB.

    Thick sound is evenly filled in: every band is about as loud as the ones beside it. Pulling
    down the bands that are already the lower ones leaves the sound standing on its strongest
    regions, which is what "thin" sounds like. Measured on the long resolution and shared.
*/
void SpectralThinner::analyseContrast (Resolution& r)
{
    std::array<double, numContrastBands> level {};
    double loudest = -400.0;

    for (int b = 0; b < numContrastBands; ++b)
    {
        double sum = 0.0;
        for (int k = bandLo[(size_t) b]; k <= bandHi[(size_t) b]; ++k)
            sum += r.Ps[(size_t) k];
        level[(size_t) b] = fastPowerToDb (sum + tiny) + bandTilt[(size_t) b];
        if (bandAbove[(size_t) b])
            loudest = std::max (loudest, level[(size_t) b]);
    }

    for (int b = 0; b < numContrastBands; ++b)
    {
        double upper = -400.0;
        for (int j = std::max (0, b - contrastReach); j <= std::min (numContrastBands - 1, b + contrastReach); ++j)
            if (j != b && bandAbove[(size_t) j])
                upper = std::max (upper, level[(size_t) j] - 1.5 * (std::abs (j - b) - 1)); // nearer bands count for more

        // A band far below everything (silence up there, or a filtered sound) is not a dip to deepen.
        double dip = level[(size_t) b] > loudest - 60.0 ? std::max (0.0, upper - level[(size_t) b] - contrastKnee) : 0.0;
        dip = std::min (dip, 24.0);
        contrast[(size_t) b] += 0.35 * (dip - contrast[(size_t) b]);
    }
}

//==============================================================================
void SpectralThinner::analyseHarmonics (Resolution& r)
{
    harmonics.clear();

    const double binHz = r.binHz;
    const int kMin = std::max (2, (int) (35.0 / binHz));
    const int kMax = std::min (r.aHi - 2, (int) (harmonicTop / binHz));
    const auto& Ps = r.Ps;

    double total = 0.0, bodyEnergy = 0.0;
    const int bodyLo = (int) (120.0 / binHz), bodyHi = (int) (600.0 / binHz);
    for (int k = kMin; k <= kMax; ++k)
    {
        const double p = Ps[(size_t) k];
        compressed[(size_t) k] = std::sqrt (std::sqrt (p));
        total += p;
        if (k >= bodyLo && k <= bodyHi)
            bodyEnergy += p;
    }
    compressed[(size_t) kMax + 1] = 0.0;

    // The body bell digs deeper when the low mids dominate.
    const double share = bodyEnergy / (total + tiny);
    bodyDynamics += 0.3 * (std::clamp (0.7 + 1.2 * share, 0.7, 1.3) - bodyDynamics);

    const double floorPower = (r.N * 1.0e-5) * (r.N * 1.0e-5);
    if (total < floorPower)
    {
        harmonicConfidence *= 0.5;
        return;
    }

    std::copy (compressed.begin() + kMin, compressed.begin() + kMax + 2, compressedWork.begin() + kMin);
    std::fill (claimed.begin(), claimed.end(), (char) 0);
    const auto& work = compressedWork;

    auto salience = [&] (double f0)
    {
        double sum = 0.0, w = 1.0;
        for (int h = 1; h <= 12; ++h)
        {
            const double x = h * f0 / binHz;
            if (x >= kMax)
                break;
            const int i = (int) x;
            const double frac = x - i;
            if (i >= kMin)
                sum += w * (work[(size_t) i] * (1.0 - frac) + work[(size_t) i + 1] * frac);
            w *= 0.85;
        }
        return sum;
    };

    struct Series
    {
        double f0 = 0.0;
        int count = 0;
        std::array<double, maxSnapped + 1> peak {};
    };
    std::array<Series, maxSeries> found;
    int numFound = 0;
    double explained = 0.0, firstSalience = 0.0;

    for (int iter = 0; iter < maxSeries; ++iter)
    {
        double best = 0.0, bestS = 0.0;
        for (double f : candidateFreqs)
        {
            const double sal = salience (f);
            if (sal > bestS)
            {
                bestS = sal;
                best = f;
            }
        }

        if (bestS <= 0.0)
            break;

        const double coarse = best;
        for (int j = -6; j <= 6; ++j)
        {
            const double f = coarse * std::pow (2.0, j / (48.0 * 7.0));
            const double sal = salience (f);
            if (sal > bestS)
            {
                bestS = sal;
                best = f;
            }
        }

        if (iter == 0)
            firstSalience = bestS;
        else if (bestS < 0.45 * firstSalience)
            break;

        // Snap each harmonic to the spectral peak beside it.
        auto& series = found[(size_t) numFound++];
        const int half = std::max (2, (int) std::lround (0.5 * best / binHz));
        double sumF = 0.0, sumW = 0.0;

        for (int h = 1; h <= maxSnapped; ++h)
        {
            const double kt = h * best / binHz;
            if (kt > kMax - 2)
                break;

            const double dev = std::max (1.5, 0.03 * kt);
            const int a = std::max (kMin, (int) std::ceil (kt - dev));
            const int b = std::min (kMax - 1, (int) std::floor (kt + dev));
            if (a > b)
            {
                series.peak[(size_t) h] = -1.0;
                series.count = h;
                continue;
            }

            int kp = a;
            for (int k = a + 1; k <= b; ++k)
                if (Ps[(size_t) k] > Ps[(size_t) kp])
                    kp = k;

            const double p = Ps[(size_t) kp];
            const bool isPeak = p >= Ps[(size_t) kp - 1] && p >= Ps[(size_t) kp + 1];

            // A real partial stands clear of what's around it.
            const int ma = std::max (kMin, kp - half), mb = std::min (kMax, kp + half);
            double mean = 0.0;
            for (int k = ma; k <= mb; ++k)
                mean += Ps[(size_t) k];
            mean /= (mb - ma + 1);
            const bool prominent = isPeak && p > 2.0 * mean;

            double fh = -1.0;
            if (prominent)
            {
                const double la = r.L[(size_t) kp - 1], lb = r.L[(size_t) kp], lc = r.L[(size_t) kp + 1];
                const double denom = la - 2.0 * lb + lc;
                const double delta = denom < -1.0e-9 ? std::clamp (0.5 * (la - lc) / denom, -0.5, 0.5) : 0.0;
                fh = (kp + delta) * binHz;

                const double w = std::sqrt (p);
                sumF += w * fh / h;
                sumW += w;

                for (int k = kp - 1; k <= kp + 1; ++k)
                {
                    if (! claimed[(size_t) k])
                    {
                        claimed[(size_t) k] = 1;
                        explained += Ps[(size_t) k];
                    }
                }
            }

            for (int k = std::max (kMin, kp - 2); k <= std::min (kMax, kp + 2); ++k)
                compressedWork[(size_t) k] = 0.0;

            series.peak[(size_t) h] = fh;
            series.count = h;
        }

        series.f0 = sumW > 0.0 ? sumF / sumW : best;
    }

    const double confidence = smoothstep ((explained / total - 0.30) / 0.40);
    harmonicConfidence += 0.5 * (confidence - harmonicConfidence);
    if (harmonicConfidence < 0.02)
        return;

    const double top = std::min (12000.0, 0.45 * sr);
    for (int i = 0; i < numFound; ++i)
    {
        const auto& series = found[(size_t) i];
        for (int h = 2; h <= 200; ++h)
        {
            double f = h * series.f0;
            if (h <= series.count && series.peak[(size_t) h] > 0.0)
                f = series.peak[(size_t) h];
            if (f > top)
                break;

            // Higher overtones go further: the 2nd is eased, from the 6th up it's the full amount.
            const double weight = harmonicConfidence * std::pow (std::min (1.0, (h - 1) / 5.0), 0.7);
            harmonics.push_back ({ f, weight });
        }
    }
}

} // namespace desat
