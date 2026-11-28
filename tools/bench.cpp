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
double sdrDb (const wav::Audio& reference, const wav::Audio& y, double fromSeconds = 0.5)
{
    double sy = 0.0, yy = 0.0;
    const size_t from = (size_t) (fromSeconds * reference.sampleRate);
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

wav::Audio softChord (double sr, double seconds);

/** Soft-clips a clean sound at several drives, runs Restore over it, and reports how much nearer
    to the clean original it comes (positive = restored). */
int restoreReport()
{
    const double sr = 48000.0;
    struct Source { const char* name; wav::Audio audio; };
    Source sources[] = { { "soft chord", softChord (sr, 8.0) }, { "saw chord", signals::cleanChord (sr, 8.0) }, { "drums+pad", signals::drumsAndPad (sr, 8.0) } };
    const double settled = 4.0; // seconds: scored after the drive measurement has settled

    std::printf ("Restore: change in signal-to-distortion ratio against the clean original, dB (positive = closer), from %.0f s in\n", settled);
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

            const double before = sdrDb (clean, clipped, settled);
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
                    std::printf (" %+6.1f", sdrDb (clean, measure::process (clipped, p).out, settled) - before);
                }
                std::printf (" |");
            }
            std::printf ("\n");
        }
    }
    return 0;
}

/** How far apart two long-term spectra are, in dB (root mean square over 60 Hz to 10 kHz), after
    lining their overall levels up. Anything more than 70 dB below the loudest bin counts as silence. */
double spectralDistanceDb (const wav::Audio& reference, const wav::Audio& y)
{
    const auto a = measure::spectrum (reference), b = measure::spectrum (y);
    double topA = 0.0, topB = 0.0;
    for (double v : a.power) topA = std::max (topA, v);
    for (double v : b.power) topB = std::max (topB, v);

    const int lo = (int) (60.0 / a.binHz), hi = (int) (10000.0 / a.binHz);
    std::vector<double> da, db;
    double meanDiff = 0.0;
    for (int k = lo; k <= hi; ++k)
    {
        da.push_back (std::max (-70.0, measure::db (a.power[(size_t) k] / topA)));
        db.push_back (std::max (-70.0, measure::db (b.power[(size_t) k] / topB)));
        meanDiff += db.back() - da.back();
    }
    meanDiff /= (double) da.size();

    double sum = 0.0;
    for (size_t i = 0; i < da.size(); ++i)
    {
        const double d = db[i] - da[i] - meanDiff;
        sum += d * d;
    }
    return std::sqrt (sum / (double) da.size());
}

/** A mellow chord: each note has only four partials, falling fast. Saturation adds a lot to this. */
wav::Audio softChord (double sr, double seconds)
{
    auto a = signals::make (sr, seconds);
    const double notes[] = { 110.0, 138.59, 164.81, 220.0 };
    for (size_t n = 0; n < a.channels[0].size(); ++n)
    {
        const double t = (double) n / sr;
        double l = 0.0, r = 0.0;
        for (int i = 0; i < 4; ++i)
            for (int h = 1; h <= 4; ++h)
            {
                l += std::sin (2.0 * signals::pi * h * notes[i] * t) / (h * h);
                r += std::sin (2.0 * signals::pi * h * notes[i] * (1.0 + 0.002 * i) * t) / (h * h);
            }
        a.channels[0][n] = l;
        a.channels[1][n] = r;
    }
    return a;
}

/** The test the plugin is named for: saturate a clean sound, run the plugin over it, and see
    whether the result is nearer the clean original than the saturated one was.
    Two measures, both against the clean original: waveform (SDR, higher is nearer) and
    long-term spectrum (distance in dB, lower is nearer). */
int desaturateReport()
{
    const double sr = 48000.0;
    struct Source { const char* name; wav::Audio audio; };
    Source sources[] = { { "soft chord", softChord (sr, 4.0) }, { "saw chord", signals::cleanChord (sr, 4.0) }, { "drums+pad", signals::drumsAndPad (sr, 4.0) } };

    struct Setting { const char* name; bool character; double amount, shape, restore; };
    const Setting settings[] = {
        { "Clean 30",            false, 30, 50, 0 },
        { "Clean 60",            false, 60, 50, 0 },
        { "Clean 100",           false, 100, 50, 0 },
        { "Clean 60 harmonics",  false, 60, 0, 0 },
        { "Clean 60 body",       false, 60, 100, 0 },
        { "Clean 60 harm+rest",  false, 60, 0, 60 },
        { "Clean 30 harm+rest",  false, 30, 0, 60 },
        { "Char 60 harmonics",   true, 60, 0, 0 },
        { "restore 60 only",     false, 0, 50, 60 },
    };

    std::printf ("De-saturation: change against the clean original. SDR in dB (+ = nearer), spectrum distance in dB (- = nearer)\n");
    for (auto& src : sources)
    {
        auto clean = src.audio;
        signals::normalise (clean, 1.0);

        for (double drive : { 1.5, 3.0 })
        {
            auto saturated = clean;
            for (auto& c : saturated.channels)
                for (double& v : c)
                    v = std::tanh (drive * v) / std::tanh (drive);

            const double sdr0 = sdrDb (clean, saturated), dist0 = spectralDistanceDb (clean, saturated);
            std::printf ("\n%s, drive %.1f: saturated is at SDR %.1f dB, spectrum distance %.1f dB\n", src.name, drive, sdr0, dist0);

            for (const auto& s : settings)
            {
                desat::EngineParams p;
                p.character = s.character;
                p.amount = s.amount;
                p.shape = s.shape;
                p.restore = s.restore;
                p.autoGain = false;
                const auto out = measure::process (saturated, p).out;
                std::printf ("  %-20s SDR %+6.1f   spectrum %+6.1f\n", s.name, sdrDb (clean, out) - sdr0, spectralDistanceDb (clean, out) - dist0);
            }
        }
    }
    return 0;
}

/** Can the drive be found blindly? Saturation fills a spectrum in; the right inverse curve empties it
    again, and too strong a curve fills it in with distortion of its own. So the inverse that leaves
    the sparsest spectrum should be the matching one. This tries every k and reports which one the
    sparsity measure picks, against the true one, and the SDR each gives. */
int matchReport()
{
    const double sr = 48000.0;
    struct Source { const char* name; wav::Audio audio; };
    Source sources[] = { { "soft chord", softChord (sr, 3.0) }, { "saw chord", signals::cleanChord (sr, 3.0) },
                         { "drums+pad", signals::drumsAndPad (sr, 3.0) }, { "sine", signals::sine (sr, 220.0, 3.0) }, { "noise", signals::noise (sr, 3.0) } };

    constexpr int N = 4096;
    desat::RealFft fft (N);
    std::vector<double> x ((size_t) N), re ((size_t) N / 2 + 1), im ((size_t) N / 2 + 1);

    // Lower is sparser: (sum |X|)^2 / (bins * sum |X|^2), averaged over frames in dB.
    auto fullness = [&] (const std::vector<double>& mono)
    {
        double total = 0.0;
        int frames = 0;
        for (size_t start = (size_t) (0.5 * sr); start + N <= mono.size(); start += N / 2, ++frames)
        {
            for (int i = 0; i < N; ++i)
                x[(size_t) i] = mono[start + (size_t) i] * (0.5 - 0.5 * std::cos (2.0 * signals::pi * i / N));
            fft.forward (x.data(), re.data(), im.data());
            double l1 = 0.0, l2 = 0.0;
            for (int k = 4; k < N / 2; ++k)
            {
                const double p = re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
                l1 += std::sqrt (p);
                l2 += p;
            }
            total += measure::db (l1 * l1 / ((N / 2 - 4) * l2 + 1.0e-30));
        }
        return frames > 0 ? total / frames : 0.0;
    };

    std::printf ("Blind match: which inverse curve (k) leaves the sparsest spectrum, against the true one\n");
    std::printf ("%-11s %-6s %-7s %-8s %-10s %-10s %-8s\n", "sound", "drive", "true k", "picked k", "SDR gain", "best gain", "sparser by");

    for (auto& src : sources)
    {
        auto clean = src.audio;
        signals::normalise (clean, 1.0);

        for (double drive : { 0.0, 1.0, 1.5, 2.3, 3.0 })
        {
            auto saturated = clean;
            if (drive > 0.0)
                for (auto& c : saturated.channels)
                    for (double& v : c)
                        v = std::tanh (drive * v) / std::tanh (drive);

            const double sdr0 = drive > 0.0 ? sdrDb (clean, saturated) : 200.0;
            double pickedK = 0.0, pickedFullness = 1.0e9, pickedSdr = 0.0, bestSdr = -1.0e9, fullnessAtZero = 0.0;

            for (int step = 0; step <= 33; ++step)
            {
                const double k = step * 0.03;
                auto out = saturated;
                if (k > 0.0)
                    for (auto& c : out.channels)
                        for (double& v : c)
                            v = std::atanh (k * std::clamp (v, -1.0, 1.0)) / k; // the peak is 1 here

                const double f = fullness (out.channels[0]);
                if (step == 0)
                    fullnessAtZero = f;
                const double sdr = drive > 0.0 ? sdrDb (clean, out) : (k > 0.0 ? sdrDb (clean, out) : 200.0);
                if (f < pickedFullness)
                {
                    pickedFullness = f;
                    pickedK = k;
                    pickedSdr = sdr;
                }
                bestSdr = std::max (bestSdr, sdr);
            }

            std::printf ("%-11s %-6.1f %-7.2f %-8.2f %+-10.1f %+-10.1f %.2f dB\n", src.name, drive, drive > 0.0 ? std::tanh (drive) : 0.0, pickedK,
                         pickedSdr - sdr0, bestSdr - sdr0, fullnessAtZero - pickedFullness);
        }
    }
    return 0;
}

/** What the live drive measurement reads over time, against the true k. */
int trackReport()
{
    const double sr = 48000.0;
    struct Source { const char* name; wav::Audio audio; };
    Source sources[] = { { "soft chord", softChord (sr, 4.0) }, { "saw chord", signals::cleanChord (sr, 4.0) },
                         { "drums+pad", signals::drumsAndPad (sr, 4.0) }, { "noise", signals::noise (sr, 4.0) } };

    std::printf ("Live drive measurement: k read at 0.5, 1, 2, 3 and 4 seconds, against the true k\n");
    for (auto& src : sources)
    {
        auto clean = src.audio;
        signals::normalise (clean, 1.0);

        for (double drive : { 0.0, 1.0, 1.5, 2.3, 3.0 })
        {
            auto saturated = clean;
            if (drive > 0.0)
                for (auto& c : saturated.channels)
                    for (double& v : c)
                        v = std::tanh (drive * v) / std::tanh (drive);

            desat::Restore restore;
            restore.prepare (sr, 2);
            std::printf ("%-11s drive %.1f  true %.2f  read", src.name, drive, drive > 0.0 ? std::tanh (drive) : 0.0);

            const int block = 480;
            for (int pos = 0; pos + block <= saturated.numSamples(); pos += block)
            {
                double* io[2] = { saturated.channels[0].data() + pos, saturated.channels[1].data() + pos };
                restore.process (io, block, 1.0, false);
                const double t = (pos + block) / sr;
                for (double mark : { 0.5, 1.0, 2.0, 3.0, 4.0 })
                    if (std::abs (t - mark) < 0.5 * block / sr)
                        std::printf ("  %.2f", restore.matchedDrive());
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
    if (argc > 1 && std::string (argv[1]) == "desaturate")
        return desaturateReport();
    if (argc > 1 && std::string (argv[1]) == "match")
        return matchReport();
    if (argc > 1 && std::string (argv[1]) == "track")
        return trackReport();

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
