// Timing of each part of the engine, and a close look at a slice of spectrum before and after.
//
//   desat_bench

#include "measure.h"
#include "signals.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace
{
template <typename Fn>
double timeIt (Fn&& fn)
{
    const auto start = std::chrono::steady_clock::now();
    fn();
    return std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
}

void report (const char* name, double seconds, double audioSeconds)
{
    std::printf ("  %-34s %7.2f%% of one core  (%5.0fx realtime)\n", name, 100.0 * seconds / audioSeconds, audioSeconds / seconds);
}
} // namespace

/** How close y is to the reference, after the best single gain: signal-to-distortion ratio in dB. */
double sdrDb (const wav::Audio& reference, const wav::Audio& y)
{
    double sy = 0.0, yy = 0.0;
    const size_t from = (size_t) (0.5 * reference.sampleRate);
    for (size_t c = 0; c < reference.channels.size(); ++c)
        for (size_t i = from; i < reference.channels[c].size(); ++i)
        {
            sy += reference.channels[c][i] * y.channels[c][i];
            yy += y.channels[c][i] * y.channels[c][i];
        }
    const double g = yy > 0.0 ? sy / yy : 1.0;
    double ss = 0.0, ee = 0.0;
    for (size_t c = 0; c < reference.channels.size(); ++c)
        for (size_t i = from; i < reference.channels[c].size(); ++i)
        {
            const double e = reference.channels[c][i] - g * y.channels[c][i];
            ss += reference.channels[c][i] * reference.channels[c][i];
            ee += e * e;
        }
    return measure::db (ss) - measure::db (ee);
}

/** Soft-clips a clean sound at several drives, runs Restore over it, and reports how much nearer
    to the clean original it comes (positive = restored). */
int restoreReport()
{
    const double sr = 48000.0;
    struct Source { const char* name; wav::Audio audio; };
    Source sources[] = { { "chord", signals::cleanChord (sr, 4.0) }, { "drums+pad", signals::drumsAndPad (sr, 4.0) } };

    std::printf ("Restore: change in signal-to-distortion ratio against the clean original, dB (positive = closer)\n");
    std::printf ("%-10s %-6s %8s | %-28s | %-28s\n", "sound", "drive", "clipped", "Clean 25 / 50 / 75 / 100", "Character 25 / 50 / 75 / 100");

    for (auto& src : sources)
    {
        auto clean = src.audio;
        signals::normalise (clean, 1.0);

        for (double drive : { 1.0, 1.5, 2.3 })
        {
            auto clipped = clean;
            for (auto& c : clipped.channels)
                for (double& v : c)
                    v = std::tanh (drive * v) / std::tanh (drive);

            const double before = sdrDb (clean, clipped);
            std::printf ("%-10s %-6.1f %8.1f |", src.name, drive, before);

            for (bool character : { false, true })
            {
                for (double amount : { 25.0, 50.0, 75.0, 100.0 })
                {
                    desat::EngineParams p;
                    p.hq = false;
                    p.amount = 0.0;
                    p.restore = amount;
                    p.character = character;
                    p.autoGain = false;
                    std::printf (" %+6.1f", sdrDb (clean, measure::process (clipped, p).out) - before);
                }
                std::printf (" |");
            }
            std::printf ("\n");
        }
    }
    return 0;
}

int main (int argc, char** argv)
{
    if (argc > 1 && std::string (argv[1]) == "restore")
        return restoreReport();

    const double sr = 48000.0, seconds = 10.0;
    const auto chord = signals::saturatedChord (sr, seconds);
    const int total = chord.numSamples();
    const double* in[2] = { chord.channels[0].data(), chord.channels[1].data() };

    std::vector<double> ra ((size_t) total), rb ((size_t) total);
    double* removed[2] = { ra.data(), rb.data() };

    desat::ThinSettings s;
    s.amount = 0.6;

    std::printf ("CPU at %.0f Hz, stereo, saturated chord:\n", sr);

    {
        // The FFT work the HQ engine does per second of audio.
        std::vector<double> x (4096), re (2049), im (2049);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = std::sin (0.1 * (double) i);
        const int sizes[] = { 4096, 1024, 256 };
        double t = 0.0;
        for (int n : sizes)
        {
            desat::RealFft fft (n);
            const int frames = (int) (seconds * sr / (n / 4)) * 4; // forward + inverse, two channels
            t += timeIt ([&] { for (int i = 0; i < frames / 2; ++i) { fft.forward (x.data(), re.data(), im.data()); fft.inverse (re.data(), im.data(), x.data()); } });
        }
        report ("FFTs alone (HQ's share)", t, seconds);
    }

    for (bool character : { false, true })
    {
        desat::SpectralThinner hq;
        hq.prepare (sr, 2);
        s.character = character;
        const double t = timeIt ([&] { for (int p = 0; p + 64 <= total; p += 64) { const double* i2[2] = { in[0] + p, in[1] + p }; double* r2[2] = { removed[0] + p, removed[1] + p }; hq.process (i2, r2, 64, s); } });
        report (character ? "HQ thinner, Character" : "HQ thinner, Clean", t, seconds);
    }

    {
        desat::SpectralThinner hq;
        hq.prepare (sr, 2);
        s.amount = 0.0;
        const double t = timeIt ([&] { for (int p = 0; p + 64 <= total; p += 64) { const double* i2[2] = { in[0] + p, in[1] + p }; double* r2[2] = { removed[0] + p, removed[1] + p }; hq.process (i2, r2, 64, s); } });
        report ("HQ thinner, Amount 0", t, seconds);
        s.amount = 0.6;
    }

    {
        desat::LiveThinner live;
        live.prepare (sr, 2);
        s.character = false;
        const double t = timeIt ([&] { for (int p = 0; p + 64 <= total; p += 64) { const double* i2[2] = { in[0] + p, in[1] + p }; double* r2[2] = { removed[0] + p, removed[1] + p }; live.process (i2, r2, 64, s); } });
        report ("Live thinner", t, seconds);
    }

    {
        desat::Restore restore;
        restore.prepare (sr, 2);
        auto copy = chord;
        double* io[2] = { copy.channels[0].data(), copy.channels[1].data() };
        const double t = timeIt ([&] { restore.process (io, total, 1.0, false); });
        report ("Restore", t, seconds);
    }

    for (bool hq : { true, false })
    {
        desat::EngineParams p;
        p.hq = hq;
        p.amount = 60.0;
        p.restore = 50.0;
        const double t = timeIt ([&] { measure::process (chord, p); });
        report (hq ? "Whole engine, HQ" : "Whole engine, Live", t, seconds);
    }

    // A slice of the spectrum, before and after HQ Clean 60 with only the fill term (no harmonics, no body).
    {
        desat::EngineParams p;
        p.amount = 100.0;
        p.autoGain = false;
        const auto run = measure::process (chord, p);
        const auto a = measure::spectrum (chord), b = measure::spectrum (run.out);
        double ref = 0.0;
        for (double v : a.power)
            ref = std::max (ref, v);

        std::printf ("\nSpectrum 800 to 1000 Hz, dB below the loudest bin (HQ Clean 100):\n   Hz     before   after   change\n");
        for (int k = (int) (800.0 / a.binHz); k * a.binHz < 1000.0; ++k)
            std::printf ("%7.1f  %7.1f %7.1f  %7.1f\n", k * a.binHz, measure::db (a.power[(size_t) k] / ref), measure::db (b.power[(size_t) k] / ref),
                         measure::db (b.power[(size_t) k]) - measure::db (a.power[(size_t) k]));

        std::printf ("\nValley depth: before %.1f dB, after %.1f dB\n", measure::valleyDepthDb (a), measure::valleyDepthDb (b));
    }

    // A pure tone: what else appears? Levels relative to the tone.
    {
        const auto tone = signals::sine (sr);
        desat::EngineParams p;
        p.amount = 100.0;
        p.character = true;
        p.autoGain = false;
        const auto run = measure::process (tone, p);
        const auto b = measure::spectrum (run.out, 1.0);
        const double toneP = b.band (950.0, 1050.0);
        std::printf ("\nPure 1 kHz tone through HQ Character 100: everything that isn't the tone is at %.1f dB below it\n",
                     -(measure::db (b.band (20.0, 900.0) + b.band (1100.0, 20000.0)) - measure::db (toneP)));
    }

    return 0;
}
