// The plugin around the engines: parameters, state, latency, and both sample formats.

#include "plugin/PluginProcessor.h"
#include "signals.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
struct Gui
{
    juce::ScopedJuceInitialiser_GUI init;
};

void setParam (DeSaturatorProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float getParam (DeSaturatorProcessor& p, const char* id)
{
    return p.apvts.getRawParameterValue (id)->load();
}
} // namespace

TEST_CASE ("The parameters are the ones in the design spec")
{
    Gui gui;
    DeSaturatorProcessor processor;

    const auto specFile = juce::File (DESAT_SOURCE_DIR).getChildFile ("design/spec/spec.json");
    REQUIRE (specFile.existsAsFile());
    const auto spec = juce::JSON::parse (specFile);
    const auto* list = spec["parameters"].getArray();
    REQUIRE (list != nullptr);
    REQUIRE (list->size() == processor.apvts.state.getNumChildren());

    for (const auto& entry : *list)
    {
        const auto id = entry["id"].toString();
        INFO (id);
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        REQUIRE (param->getName (64) == entry["name"].toString());

        const auto kind = entry["kind"].toString();
        const float def = param->convertFrom0to1 (param->getDefaultValue());

        if (kind == "knob" || kind == "slider")
        {
            const auto range = processor.apvts.getParameterRange (id);
            REQUIRE (range.start == Approx ((double) entry["min"]));
            REQUIRE (range.end == Approx ((double) entry["max"]));
            REQUIRE (def == Approx ((double) entry["def"]));
        }
        else if (kind == "toggle")
        {
            REQUIRE ((def >= 0.5f) == (bool) entry["def"]);
        }
        else if (kind == "choice")
        {
            const auto* options = entry["options"].getArray();
            REQUIRE (options != nullptr);
            REQUIRE (options->indexOf (entry["def"]) == juce::roundToInt (def));
            const auto* labels = entry["labels"].getArray();
            REQUIRE (param->getAllValueStrings().size() == labels->size());
            for (int i = 0; i < labels->size(); ++i)
                REQUIRE (param->getAllValueStrings()[i] == (*labels)[i].toString());
        }
    }
}

TEST_CASE ("Values read the way the design writes them")
{
    Gui gui;
    DeSaturatorProcessor processor;

    auto text = [&] (const char* id, float value)
    {
        auto* param = processor.apvts.getParameter (id);
        return param->getText (param->convertTo0to1 (value), 64);
    };

    REQUIRE (text (ParamIDs::amount, 30.0f) == "30%");
    REQUIRE (text (ParamIDs::output, 0.0f) == "0.0 dB");
    REQUIRE (text (ParamIDs::output, 3.0f) == "+3.0 dB");
    REQUIRE (text (ParamIDs::output, -1.5f) == juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "1.5 dB")));
}

TEST_CASE ("State survives a save and a load: parameters, the curve, the theme, the size")
{
    Gui gui;
    juce::MemoryBlock saved;

    desat::Curve curve;
    curve.count = 2;
    curve.points[0] = { 140.0, 0.25, 1.5 };
    curve.points[1] = { 3300.0, 1.8, 0.7 };

    {
        DeSaturatorProcessor a;
        setParam (a, ParamIDs::amount, 72.0f);
        setParam (a, ParamIDs::shape, 12.0f);
        setParam (a, ParamIDs::output, -6.5f);
        setParam (a, ParamIDs::quality, 0.0f);
        setParam (a, ParamIDs::character, 1.0f);
        setParam (a, ParamIDs::autoGain, 0.0f);
        a.setCurve (curve);
        a.setEditorScale (1.5f);
        a.getStateInformation (saved);
    }

    DeSaturatorProcessor b;
    const int versionBefore = b.getCurveVersion();
    b.setStateInformation (saved.getData(), (int) saved.getSize());

    REQUIRE (getParam (b, ParamIDs::amount) == Approx (72.0f));
    REQUIRE (getParam (b, ParamIDs::shape) == Approx (12.0f));
    REQUIRE (getParam (b, ParamIDs::output) == Approx (-6.5f));
    REQUIRE (getParam (b, ParamIDs::quality) == 0.0f);
    REQUIRE (getParam (b, ParamIDs::character) == 1.0f);
    REQUIRE (getParam (b, ParamIDs::autoGain) == 0.0f);
    REQUIRE (b.getCurve() == curve);
    REQUIRE (b.getCurveVersion() != versionBefore);
    REQUIRE (b.getEditorScale() == Approx (1.5f));

    // Rubbish is ignored.
    const char junk[] = "not a state";
    b.setStateInformation (junk, (int) sizeof (junk));
    REQUIRE (getParam (b, ParamIDs::amount) == Approx (72.0f));
}

TEST_CASE ("The curve is kept within its limits")
{
    Gui gui;
    DeSaturatorProcessor processor;

    desat::Curve wild;
    wild.count = 9;
    wild.points[0] = { 5.0, 7.0, 0.01 };
    processor.setCurve (wild);

    const auto c = processor.getCurve();
    REQUIRE (c.count == desat::Curve::maxPoints);
    REQUIRE (c.points[0].freq == 20.0);
    REQUIRE (c.points[0].amount == 2.0);
    REQUIRE (c.points[0].width == 0.3);
}

TEST_CASE ("Latency follows Live / HQ and the label agrees")
{
    Gui gui;
    DeSaturatorProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    REQUIRE (processor.getLatencySamples() == 4096);
    REQUIRE (processor.getLatencyMs() == Approx (85.33).margin (0.01));

    setParam (processor, ParamIDs::quality, 0.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE (processor.getLatencySamples() == 0);
    REQUIRE (processor.getLatencyMs() == 0.0);
}

TEST_CASE ("In Live the window is told the smoothed curve; in HQ the drawn one")
{
    Gui gui;
    DeSaturatorProcessor processor;

    desat::Curve curve;
    curve.count = 1;
    curve.points[0] = { 1000.0, 2.0, 0.3 };
    processor.setCurve (curve);

    std::array<float, desat::curveDisplayPoints> hq {}, live {};
    processor.getEffectiveCurve (hq);
    setParam (processor, ParamIDs::quality, 0.0f);
    processor.getEffectiveCurve (live);

    const float hqPeak = *std::max_element (hq.begin(), hq.end());
    const float livePeak = *std::max_element (live.begin(), live.end());
    REQUIRE (hqPeak > 1.9f);
    REQUIRE (livePeak < hqPeak - 0.2f);
    REQUIRE (livePeak > 1.0f);
}

TEST_CASE ("Float and double processing agree, and the analyser is fed")
{
    Gui gui;
    const auto sound = signals::saturatedChord (48000.0, 1.0);
    const int total = sound.numSamples(), block = 480;

    juce::AudioBuffer<float> outF (2, total);
    juce::AudioBuffer<double> outD (2, total);
    juce::MidiBuffer midi;

    {
        DeSaturatorProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, block);
        setParam (p, ParamIDs::amount, 80.0f);
        p.prepareToPlay (48000.0, block);
        for (int pos = 0; pos + block <= total; pos += block)
        {
            juce::AudioBuffer<float> b (2, block);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                    b.setSample (c, i, (float) sound.channels[(size_t) c][(size_t) (pos + i)]);
            p.processBlock (b, midi);
            for (int c = 0; c < 2; ++c)
                outF.copyFrom (c, pos, b, c, 0, block);
        }

        p.getAnalyser().update (1.0 / 30.0);
        REQUIRE (p.getAnalyser().hasSignal());
        // The result sits at or below the input wherever something was removed.
        double in = 0.0, out = 0.0;
        for (int j = 0; j < Analyser::numLines; ++j)
        {
            in += p.getAnalyser().input()[(size_t) j];
            out += p.getAnalyser().result()[(size_t) j];
        }
        REQUIRE (out < in);
    }

    {
        DeSaturatorProcessor p;
        p.setProcessingPrecision (juce::AudioProcessor::doublePrecision);
        p.setPlayConfigDetails (2, 2, 48000.0, block);
        setParam (p, ParamIDs::amount, 80.0f);
        p.prepareToPlay (48000.0, block);
        for (int pos = 0; pos + block <= total; pos += block)
        {
            juce::AudioBuffer<double> b (2, block);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                    b.setSample (c, i, (double) (float) sound.channels[(size_t) c][(size_t) (pos + i)]);
            p.processBlock (b, midi);
            for (int c = 0; c < 2; ++c)
                outD.copyFrom (c, pos, b, c, 0, block);
        }
    }

    float worst = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < total; ++i)
            worst = std::max (worst, std::abs (outF.getSample (c, i) - (float) outD.getSample (c, i)));
    REQUIRE (worst < 1.0e-5f);
}

TEST_CASE ("Mono works, and a host bypass still passes delayed audio")
{
    Gui gui;
    DeSaturatorProcessor p;
    juce::AudioProcessor::BusesLayout mono;
    mono.inputBuses.add (juce::AudioChannelSet::mono());
    mono.outputBuses.add (juce::AudioChannelSet::mono());
    REQUIRE (p.setBusesLayout (mono));
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int n = 0; n < 20; ++n)
    {
        juce::AudioBuffer<float> b (1, 512);
        for (int i = 0; i < 512; ++i)
            b.setSample (0, i, 0.5f * std::sin (0.05f * (float) (n * 512 + i)));
        p.processBlockBypassed (b, midi);
        peak = std::max (peak, b.getMagnitude (0, 512));
    }
    REQUIRE (peak == Approx (0.5f).margin (0.01f));
}
