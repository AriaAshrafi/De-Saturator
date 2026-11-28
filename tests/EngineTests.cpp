// The engines on their own: what must always hold, whatever the tuning.

#include "measure.h"
#include "signals.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
constexpr double sr = 48000.0;

desat::EngineParams params (bool hq, double amount)
{
    desat::EngineParams p;
    p.hq = hq;
    p.amount = amount;
    return p;
}

double maxDifference (const wav::Audio& a, const wav::Audio& b, int from = 0)
{
    double m = 0.0;
    for (size_t c = 0; c < a.channels.size(); ++c)
        for (size_t i = (size_t) from; i < a.channels[c].size(); ++i)
            m = std::max (m, std::abs (a.channels[c][i] - b.channels[c][i]));
    return m;
}

bool allFinite (const wav::Audio& a)
{
    for (auto& c : a.channels)
        for (double v : c)
            if (! std::isfinite (v))
                return false;
    return true;
}

double bandPower (const wav::Audio& a, double lo, double hi)
{
    return measure::spectrum (a).band (lo, hi);
}
} // namespace

TEST_CASE ("The FFT gives back what it was given")
{
    for (int n : { 8, 256, 4096 })
    {
        desat::RealFft fft (n);
        std::vector<double> x ((size_t) n), y ((size_t) n), re ((size_t) n / 2 + 1), im ((size_t) n / 2 + 1);
        signals::Random rnd;
        for (double& v : x)
            v = rnd.next();

        fft.forward (x.data(), re.data(), im.data());
        fft.inverse (re.data(), im.data(), y.data());

        for (int i = 0; i < n; ++i)
            REQUIRE (y[(size_t) i] == Approx (x[(size_t) i]).margin (1.0e-12));
    }

    // A sine on a bin lands in that bin.
    desat::RealFft fft (256);
    std::vector<double> x (256), re (129), im (129);
    for (int i = 0; i < 256; ++i)
        x[(size_t) i] = std::cos (2.0 * signals::pi * 10.0 * i / 256.0);
    fft.forward (x.data(), re.data(), im.data());
    REQUIRE (re[10] == Approx (128.0).margin (1.0e-9));
    REQUIRE (std::abs (re[11]) < 1.0e-9);
    REQUIRE (std::abs (im[10]) < 1.0e-9);
}

TEST_CASE ("The curve follows the design's formula")
{
    desat::Curve c;
    REQUIRE (c.valueAt (1000.0) == 1.0);

    c.count = 1;
    c.points[0] = { 1000.0, 2.0, 1.0 };
    REQUIRE (c.valueAt (1000.0) == Approx (2.0));
    // Half height at half the width either side.
    REQUIRE (c.valueAt (1000.0 * std::pow (2.0, 0.5)) == Approx (1.5).margin (1.0e-3));

    c.points[0].amount = 0.0;
    REQUIRE (c.valueAt (1000.0) == 0.0);
    REQUIRE (c.valueAt (20.0) == Approx (1.0).margin (1.0e-6));
}

TEST_CASE ("With Amount and Restore at 0 the audio passes through untouched")
{
    const auto in = signals::saturatedChord (sr, 2.0);

    for (bool hq : { true, false })
    {
        auto p = params (hq, 0.0);
        const auto run = measure::process (in, p);
        REQUIRE (run.latency == (hq ? 4096 : 0));
        REQUIRE (maxDifference (run.out, in) == 0.0);
    }
}

TEST_CASE ("The reported latency is where the audio comes out")
{
    wav::Audio impulse = signals::make (sr, 0.5);
    impulse.channels[0][100] = impulse.channels[1][100] = 1.0;

    for (bool hq : { true, false })
    {
        desat::Engine engine;
        auto p = params (hq, 50.0);
        p.mix = 0.0; // dry only
        engine.setParams (p);
        engine.prepare (sr, 2, 512);

        auto buf = impulse;
        for (int pos = 0; pos + 512 <= buf.numSamples(); pos += 512)
        {
            double* io[2] = { buf.channels[0].data() + pos, buf.channels[1].data() + pos };
            engine.process (io, 512);
        }

        int at = -1;
        for (int i = 0; i < buf.numSamples(); ++i)
            if (std::abs (buf.channels[0][(size_t) i]) > 0.5)
                at = i;
        REQUIRE (at == 100 + engine.latency());
    }

    REQUIRE (desat::Engine::latencyFor (44100.0, true) == 4096);
    REQUIRE (desat::Engine::latencyFor (96000.0, true) == 8192);
    REQUIRE (desat::Engine::latencyFor (192000.0, true) == 16384);
    REQUIRE (desat::Engine::latencyFor (96000.0, false) == 0);
}

TEST_CASE ("Thinning takes something out of a saturated sound, in both modes and both ranges")
{
    const auto in = signals::saturatedChord (sr, 4.0);
    const double inPower = measure::power (in);

    for (bool hq : { true, false })
    {
        double last = -200.0;
        for (bool character : { false, true })
        {
            auto p = params (hq, 60.0);
            p.character = character;
            p.autoGain = false;
            const auto run = measure::process (in, p);
            REQUIRE (allFinite (run.out));

            const double removedDb = measure::db (measure::power (measure::difference (in, run.out))) - measure::db (inPower);
            REQUIRE (removedDb > -20.0);
            REQUIRE (removedDb > last); // Character goes further than Clean
            last = removedDb;
        }
    }
}

TEST_CASE ("HQ removes the fill between partials more than the partials")
{
    const auto in = signals::saturatedChord (sr, 4.0);
    auto p = params (true, 100.0);
    p.autoGain = false;
    const auto run = measure::process (in, p);

    REQUIRE (measure::valleyDepthDb (measure::spectrum (run.out)) > measure::valleyDepthDb (measure::spectrum (in)) + 0.5);
}

TEST_CASE ("Where the curve is at the bottom, that range is left alone")
{
    const auto in = signals::noise (sr, 4.0);

    desat::Curve open, shut;
    shut.count = 1;
    shut.points[0] = { 300.0, 0.0, 2.0 };

    for (bool hq : { true, false })
    {
        auto p = params (hq, 100.0);
        p.shape = 100.0; // body: plenty to remove around 300 Hz
        p.autoGain = false;

        const auto a = measure::difference (in, measure::process (in, p, open).out);
        const auto b = measure::difference (in, measure::process (in, p, shut).out);

        const double removedOpen = measure::db (bandPower (a, 270.0, 330.0));
        const double removedShut = measure::db (bandPower (b, 270.0, 330.0));
        // The curve only reaches 0 at the point itself; the band around it sees a sliver.
        REQUIRE (removedShut < removedOpen - (hq ? 25.0 : 4.0)); // Live applies the curve smoothed, so less of the dip survives
    }
}

TEST_CASE ("Steady noise is not made to bubble")
{
    const auto in = signals::noise (sr, 4.0);
    const double before = measure::flutterDb (in, 1000.0, 4000.0);

    for (bool hq : { true, false })
    {
        for (bool character : { false, true })
        {
            auto p = params (hq, 100.0);
            p.character = character;
            const auto run = measure::process (in, p);
            REQUIRE (measure::flutterDb (run.out, 1000.0, 4000.0) < before + 0.3);
        }
    }
}

TEST_CASE ("A drum hit keeps its peak")
{
    const auto in = signals::drumsAndPad (sr, 4.0);

    for (bool hq : { true, false })
    {
        auto p = params (hq, 60.0);
        p.autoGain = false;
        const auto run = measure::process (in, p);
        // The sustained pad comes down more than the hits do, so the crest factor rises.
        REQUIRE (measure::crestDb (run.out) > measure::crestDb (in));
        REQUIRE (measure::peak (run.out) > 0.5 * measure::peak (in));
    }
}

TEST_CASE ("Delta is exactly what was taken away")
{
    const auto in = signals::saturatedBass (sr, 3.0);

    for (bool hq : { true, false })
    {
        auto p = params (hq, 70.0);
        p.restore = 40.0;
        p.autoGain = false;
        const auto result = measure::process (in, p);
        p.delta = true;
        const auto delta = measure::process (in, p);

        wav::Audio sum = result.out;
        for (size_t c = 0; c < sum.channels.size(); ++c)
            for (size_t i = 0; i < sum.channels[c].size(); ++i)
                sum.channels[c][i] += delta.out.channels[c][i];

        REQUIRE (maxDifference (sum, in) < 1.0e-9);
    }
}

TEST_CASE ("Bypass gives the input back, in time")
{
    const auto in = signals::saturatedChord (sr, 2.0);

    for (bool hq : { true, false })
    {
        auto p = params (hq, 100.0);
        p.restore = 100.0;
        p.bypass = true;
        const auto run = measure::process (in, p);
        REQUIRE (maxDifference (run.out, in) < 1.0e-12);
    }
}

TEST_CASE ("Restore in Character lifts peaks; in Clean it leaves unsaturated sound alone")
{
    const auto in = signals::drumsAndPad (sr, 3.0);

    auto p = params (false, 0.0);
    p.autoGain = false;
    p.restore = 100.0;

    p.character = true;
    const auto forced = measure::process (in, p);
    REQUIRE (allFinite (forced.out));
    REQUIRE (measure::crestDb (forced.out) > measure::crestDb (in) + 1.0);
    REQUIRE (measure::peak (forced.out) < 5.0 * measure::peak (in)); // the curve and the expander together stay bounded

    // Nothing here was saturated, so Clean measures no drive and changes nothing.
    p.character = false;
    const auto clean = measure::process (in, p);
    REQUIRE (maxDifference (clean.out, in) < 1.0e-9);
}

TEST_CASE ("Restore measures how hard a sound was driven, and undoing it brings the clean sound back")
{
    auto clean = signals::cleanChord (sr, 8.0);
    signals::normalise (clean, 1.0);

    auto sdr = [&] (const wav::Audio& y)
    {
        // Against the clean original, after the best single gain, from 4 s in (the measurement has settled).
        const size_t from = (size_t) (4.0 * sr);
        double sy = 0.0, yy = 0.0, ss = 0.0, ee = 0.0;
        for (size_t c = 0; c < 2; ++c)
            for (size_t i = from; i < clean.channels[c].size(); ++i)
            {
                sy += clean.channels[c][i] * y.channels[c][i];
                yy += y.channels[c][i] * y.channels[c][i];
            }
        const double g = sy / yy;
        for (size_t c = 0; c < 2; ++c)
            for (size_t i = from; i < clean.channels[c].size(); ++i)
            {
                const double e = clean.channels[c][i] - g * y.channels[c][i];
                ss += clean.channels[c][i] * clean.channels[c][i];
                ee += e * e;
            }
        return measure::db (ss) - measure::db (ee);
    };

    // Unsaturated: it reads no drive.
    {
        desat::Restore restore;
        restore.prepare (sr, 2);
        auto copy = clean;
        double* io[2] = { copy.channels[0].data(), copy.channels[1].data() };
        restore.process (io, copy.numSamples(), 1.0, false);
        REQUIRE (restore.matchedDrive() < 0.05);
    }

    // Driven hard: it reads close to the true drive (k = tanh of the drive), and Clean at 100% undoes most of it.
    const double drive = 2.3;
    auto saturated = clean;
    for (auto& c : saturated.channels)
        for (double& v : c)
            v = std::tanh (drive * v) / std::tanh (drive);

    {
        desat::Restore restore;
        restore.prepare (sr, 2);
        auto copy = saturated;
        double* io[2] = { copy.channels[0].data(), copy.channels[1].data() };
        restore.process (io, copy.numSamples(), 1.0, false);
        REQUIRE (restore.matchedDrive() > 0.85);
        REQUIRE (restore.matchedDrive() < 0.995);
    }

    auto p = params (false, 0.0);
    p.autoGain = false;
    p.restore = 100.0;
    const auto restored = measure::process (saturated, p);
    REQUIRE (sdr (restored.out) > sdr (saturated) + 6.0);
}

TEST_CASE ("Auto gain brings the level back")
{
    const auto in = signals::saturatedChord (sr, 6.0);
    auto p = params (true, 100.0);
    p.character = true;

    p.autoGain = false;
    const double without = measure::db (measure::power (measure::process (in, p).out, 3.0)) - measure::db (measure::power (in, 3.0));
    p.autoGain = true;
    const double with = measure::db (measure::power (measure::process (in, p).out, 3.0)) - measure::db (measure::power (in, 3.0));

    // Auto gain matches loudness (K-weighted), which is close to, but not the same as, plain level.
    REQUIRE (without < -6.0);
    REQUIRE (std::abs (with) < 3.0);
    REQUIRE (std::abs (with) < std::abs (without) - 6.0);
}

TEST_CASE ("Every combination stays finite and sane at every sample rate, on silence and on full-scale noise")
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto loud = signals::noise (rate, 1.0);
        signals::normalise (loud, 1.0);
        const auto silence = signals::make (rate, 1.0);

        for (bool hq : { true, false })
        {
            for (bool character : { false, true })
            {
                desat::EngineParams p;
                p.hq = hq;
                p.character = character;
                p.amount = 100.0;
                p.restore = 100.0;
                p.outputDb = 12.0;

                desat::Curve c;
                c.count = 2;
                c.points[0] = { 80.0, 2.0, 0.3 };
                c.points[1] = { 9000.0, 0.0, 4.0 };

                const auto a = measure::process (loud, p, c, 137); // an awkward block size
                REQUIRE (allFinite (a.out));
                REQUIRE (measure::peak (a.out) < 64.0);

                const auto b = measure::process (silence, p, c, 137);
                REQUIRE (measure::peak (b.out) == 0.0);
            }
        }
    }
}

TEST_CASE ("The host's block size barely matters")
{
    const auto in = signals::saturatedChord (sr, 2.0);
    auto p = params (true, 60.0);
    const auto a = measure::process (in, p, {}, 512);
    const auto b = measure::process (in, p, {}, 100);
    const double diffDb = measure::db (measure::power (measure::difference (a.out, b.out))) - measure::db (measure::power (in));
    REQUIRE (diffDb < -50.0);
}

TEST_CASE ("Live reports the curve it really applies")
{
    desat::LiveThinner live;
    live.prepare (sr, 2);

    std::array<float, desat::curveDisplayPoints> eff {};
    live.effectiveCurve (eff.data());
    for (float v : eff)
        REQUIRE (v == Approx (1.0f).margin (1.0e-4f));

    desat::Curve c;
    c.count = 1;
    c.points[0] = { 1000.0, 2.0, 0.3 }; // narrower than a band
    live.setCurve (c);
    live.effectiveCurve (eff.data());

    float peak = 0.0f;
    for (float v : eff)
        peak = std::max (peak, v);
    REQUIRE (peak > 1.05f); // it shows up...
    REQUIRE (peak < 1.8f);  // ...but smoothed, not at the drawn height
}
