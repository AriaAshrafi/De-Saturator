// The offline render rig: runs the engine over test sounds (or a file) at a set of settings,
// writes the results as WAV files to listen to, and prints what was measured.
//
//   desat_render [--out folder] [--in file.wav] [--rate 48000] [--no-wav]

#include "measure.h"
#include "signals.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
struct Setting
{
    std::string name;
    desat::EngineParams params;
    desat::Curve curve;
};

desat::EngineParams make (bool hq, bool character, double amount, double shape = 50.0, double restore = 0.0)
{
    desat::EngineParams p;
    p.hq = hq;
    p.character = character;
    p.amount = amount;
    p.shape = shape;
    p.restore = restore;
    p.autoGain = false; // measure the thinning itself, not the make-up gain
    return p;
}

std::vector<Setting> settings()
{
    desat::Curve lowsUntouched;
    lowsUntouched.count = 1;
    lowsUntouched.points[0] = { 60.0, 0.0, 4.0 };

    return {
        { "hq-clean-30", make (true, false, 30), {} },
        { "hq-clean-60", make (true, false, 60), {} },
        { "hq-clean-100", make (true, false, 100), {} },
        { "hq-character-60", make (true, true, 60), {} },
        { "hq-character-100", make (true, true, 100), {} },
        { "hq-clean-60-harmonics", make (true, false, 60, 0), {} },
        { "hq-clean-60-body", make (true, false, 60, 100), {} },
        { "hq-clean-60-lows-untouched", make (true, false, 60), lowsUntouched },
        { "hq-restore-100", make (true, false, 0, 50, 100), {} },
        { "live-clean-60", make (false, false, 60), {} },
        { "live-clean-100", make (false, false, 100), {} },
        { "live-character-100", make (false, true, 100), {} },
    };
}
} // namespace

int main (int argc, char** argv)
{
    std::string outDir = "renders", inFile;
    double rate = 48000.0;
    bool writeWav = true;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--out" && i + 1 < argc) outDir = argv[++i];
        else if (a == "--in" && i + 1 < argc) inFile = argv[++i];
        else if (a == "--rate" && i + 1 < argc) rate = std::stod (argv[++i]);
        else if (a == "--no-wav") writeWav = false;
    }

    std::vector<signals::Named> sounds;
    if (! inFile.empty())
    {
        wav::Audio a;
        if (! wav::read (inFile, a))
        {
            std::fprintf (stderr, "Could not read %s (PCM or float WAV only)\n", inFile.c_str());
            return 1;
        }
        sounds.push_back ({ std::filesystem::path (inFile).stem().string(), a });
    }
    else
    {
        sounds = signals::all (rate);
    }

    if (writeWav)
        std::filesystem::create_directories (outDir);

    std::printf ("%-18s %-28s %8s %8s %8s %8s %8s %8s %8s %7s\n",
                 "sound", "setting", "removed", "level", "valleys", "body", "highs", "crest", "flutter", "x-real");
    std::printf ("%-18s %-28s %8s %8s %8s %8s %8s %8s %8s %7s\n",
                 "", "", "dB", "dB", "+dB", "dB", "dB", "+dB", "+dB", "");

    for (auto& sound : sounds)
    {
        const auto& in = sound.audio;
        const auto inSpec = measure::spectrum (in);
        const double inPower = measure::power (in);

        if (writeWav)
            wav::write (outDir + "/" + sound.name + "--original.wav", in);

        for (auto& s : settings())
        {
            const auto run = measure::process (in, s.params, s.curve);
            const auto removed = measure::difference (in, run.out);
            const auto outSpec = measure::spectrum (run.out);

            auto bandChange = [&] (double lo, double hi) { return measure::db (outSpec.band (lo, hi)) - measure::db (inSpec.band (lo, hi)); };

            std::printf ("%-18s %-28s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.2f %7.0f\n",
                         sound.name.c_str(), s.name.c_str(),
                         measure::db (measure::power (removed)) - measure::db (inPower),
                         measure::db (measure::power (run.out)) - measure::db (inPower),
                         measure::valleyDepthDb (outSpec) - measure::valleyDepthDb (inSpec),
                         bandChange (120.0, 600.0),
                         bandChange (3000.0, 12000.0),
                         measure::crestDb (run.out) - measure::crestDb (in),
                         measure::flutterDb (run.out, 1000.0, 4000.0) - measure::flutterDb (in, 1000.0, 4000.0),
                         run.realtime);

            if (writeWav)
            {
                wav::write (outDir + "/" + sound.name + "--" + s.name + ".wav", run.out);
                wav::write (outDir + "/" + sound.name + "--" + s.name + "--removed.wav", removed);
            }
        }
        std::printf ("\n");
    }

    return 0;
}
