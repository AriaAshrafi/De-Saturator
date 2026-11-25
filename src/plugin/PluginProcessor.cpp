#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace
{
/** Remembers the theme last chosen, across instances and sessions. */
juce::PropertiesFile::Options preferenceOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "De-Saturator";
    o.folderName = "Aria Ashrafi";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    return o;
}

juce::String percent (float value, int)
{
    return juce::String (juce::roundToInt (value)) + "%";
}

juce::String decibels (float value, int)
{
    if (std::abs (value) < 0.05f)
        return "0.0 dB";
    // A real minus sign, as in the design spec.
    return (value > 0.0f ? juce::String ("+") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"))) + juce::String (std::abs (value), 1) + " dB";
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DeSaturatorProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percentParam = [] (const char* id, const char* name, float defaultValue)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.01f), defaultValue,
                                                      AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percent));
    };

    layout.add (percentParam (ParamIDs::amount, "Amount", 30.0f));
    layout.add (percentParam (ParamIDs::shape, "Shape", 50.0f));
    layout.add (percentParam (ParamIDs::restore, "Restore", 0.0f));
    layout.add (percentParam (ParamIDs::mix, "Mix", 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (decibels)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoGain, 1 }, "Auto gain", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::delta, 1 }, "Delta", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::character, 1 }, "Character", StringArray { "Clean", "Character" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::quality, 1 }, "Quality", StringArray { "Live", "HQ" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

DeSaturatorProcessor::DeSaturatorProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DeSaturator", createLayout())
{
    amount = apvts.getRawParameterValue (ParamIDs::amount);
    shape = apvts.getRawParameterValue (ParamIDs::shape);
    restore = apvts.getRawParameterValue (ParamIDs::restore);
    mix = apvts.getRawParameterValue (ParamIDs::mix);
    output = apvts.getRawParameterValue (ParamIDs::output);
    autoGain = apvts.getRawParameterValue (ParamIDs::autoGain);
    delta = apvts.getRawParameterValue (ParamIDs::delta);
    character = apvts.getRawParameterValue (ParamIDs::character);
    quality = apvts.getRawParameterValue (ParamIDs::quality);
    bypass = apvts.getRawParameterValue (ParamIDs::bypass);
    bypassParam = apvts.getParameter (ParamIDs::bypass);

    apvts.addParameterListener (ParamIDs::quality, this);

    liveCurveModel.prepare (48000.0, 1);

    juce::PropertiesFile preferences (preferenceOptions());
    darkTheme = preferences.getBoolValue ("darkTheme", false);
}

DeSaturatorProcessor::~DeSaturatorProcessor()
{
    apvts.removeParameterListener (ParamIDs::quality, this);
    cancelPendingUpdate();
}

//==============================================================================
desat::Curve DeSaturatorProcessor::getCurve() const
{
    const juce::SpinLock::ScopedLockType lock (curveLock);
    return curve;
}

void DeSaturatorProcessor::setCurve (const desat::Curve& newCurve)
{
    desat::Curve c = newCurve;
    c.count = juce::jlimit (0, desat::Curve::maxPoints, c.count);
    for (int i = 0; i < c.count; ++i)
    {
        auto& p = c.points[(size_t) i];
        p.freq = juce::jlimit (20.0, 20000.0, p.freq);
        p.amount = juce::jlimit (0.0, 2.0, p.amount);
        p.width = juce::jlimit (0.3, 4.0, p.width);
    }

    {
        const juce::SpinLock::ScopedLockType lock (curveLock);
        if (c == curve)
            return;
        curve = c;
    }

    ++curveVersion;
}

void DeSaturatorProcessor::getEffectiveCurve (std::array<float, desat::curveDisplayPoints>& out)
{
    const auto c = getCurve();

    if (quality->load() >= 0.5f)
    {
        for (int i = 0; i < desat::curveDisplayPoints; ++i)
            out[(size_t) i] = (float) c.valueAt (desat::curveDisplayFreq (i));
    }
    else
    {
        liveCurveModel.setCurve (c);
        liveCurveModel.effectiveCurve (out.data());
    }
}

double DeSaturatorProcessor::getLatencyMs() const
{
    return 1000.0 * desat::Engine::latencyFor (currentSampleRate, quality->load() >= 0.5f) / currentSampleRate;
}

void DeSaturatorProcessor::setDarkTheme (bool dark)
{
    if (darkTheme.exchange (dark) == dark)
        return;

    juce::PropertiesFile preferences (preferenceOptions());
    preferences.setValue ("darkTheme", dark);
    preferences.saveIfNeeded();
}

//==============================================================================
bool DeSaturatorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void DeSaturatorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engineChannels = juce::jmax (1, getTotalNumInputChannels());

    const int blockSize = juce::jmax (64, samplesPerBlock);
    scratch.setSize (engineChannels, blockSize);

    engine.setParams (readParams());
    engine.setCurve (getCurve());
    engine.prepare (sampleRate, engineChannels, blockSize);
    curveSeenByAudio = curveVersion.load();

    analyser.prepare (sampleRate);
    updateLatency();
}

desat::EngineParams DeSaturatorProcessor::readParams() const noexcept
{
    desat::EngineParams p;
    p.amount = amount->load();
    p.shape = shape->load();
    p.restore = restore->load();
    p.mix = mix->load();
    p.outputDb = output->load();
    p.autoGain = autoGain->load() >= 0.5f;
    p.delta = delta->load() >= 0.5f;
    p.character = character->load() >= 0.5f;
    p.hq = quality->load() >= 0.5f;
    p.bypass = bypass->load() >= 0.5f;
    return p;
}

template <typename Float>
void DeSaturatorProcessor::process (juce::AudioBuffer<Float>& buffer, bool hostBypass)
{
    const juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), engineChannels);

    for (int c = numChannels; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    if (numChannels < engineChannels || scratch.getNumSamples() == 0)
        return; // not the layout we were prepared for

    if (curveSeenByAudio != curveVersion.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (curveLock);
        if (lock.isLocked())
        {
            engine.setCurve (curve);
            curveSeenByAudio = curveVersion.load();
        }
    }

    auto params = readParams();
    params.bypass = params.bypass || hostBypass;
    engine.setParams (params);

    for (int offset = 0; offset < numSamples; offset += scratch.getNumSamples())
    {
        const int n = juce::jmin (scratch.getNumSamples(), numSamples - offset);

        for (int c = 0; c < numChannels; ++c)
        {
            const Float* in = buffer.getReadPointer (c, offset);
            double* s = scratch.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                s[i] = (double) in[i];
        }

        engine.process (scratch.getArrayOfWritePointers(), n);
        analyser.push (engine.inputTap(), engine.resultTap(), n);

        for (int c = 0; c < numChannels; ++c)
        {
            Float* out = buffer.getWritePointer (c, offset);
            const double* s = scratch.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                out[i] = (Float) s[i];
        }
    }
}

void DeSaturatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process (buffer, false); }
void DeSaturatorProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { process (buffer, false); }

// Bypassed by the host: still run, so the dry signal comes out delayed by the latency we reported.
void DeSaturatorProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process (buffer, true); }
void DeSaturatorProcessor::processBlockBypassed (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) { process (buffer, true); }

//==============================================================================
void DeSaturatorProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == ParamIDs::quality)
        triggerAsyncUpdate();
}

void DeSaturatorProcessor::handleAsyncUpdate()
{
    updateLatency();
}

void DeSaturatorProcessor::updateLatency()
{
    setLatencySamples (desat::Engine::latencyFor (currentSampleRate, quality->load() >= 0.5f));
}

//==============================================================================
void DeSaturatorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    state.removeChild (state.getChildWithName ("CURVE"), nullptr);
    juce::ValueTree curveTree ("CURVE");
    const auto c = getCurve();
    for (int i = 0; i < c.count; ++i)
    {
        juce::ValueTree point ("POINT");
        point.setProperty ("freq", c.points[(size_t) i].freq, nullptr);
        point.setProperty ("amount", c.points[(size_t) i].amount, nullptr);
        point.setProperty ("width", c.points[(size_t) i].width, nullptr);
        curveTree.appendChild (point, nullptr);
    }
    state.appendChild (curveTree, nullptr);

    state.setProperty ("theme", darkTheme.load() ? "dark" : "light", nullptr);
    state.setProperty ("editorScale", editorScale.load(), nullptr);
    state.setProperty ("version", DESAT_VERSION_STRING, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DeSaturatorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    desat::Curve c;
    const auto curveTree = state.getChildWithName ("CURVE");
    for (int i = 0; i < curveTree.getNumChildren() && c.count < desat::Curve::maxPoints; ++i)
    {
        const auto point = curveTree.getChild (i);
        c.points[(size_t) c.count++] = { (double) point.getProperty ("freq", 1000.0),
                                         (double) point.getProperty ("amount", 1.0),
                                         (double) point.getProperty ("width", 1.0) };
    }
    setCurve (c);

    if (state.hasProperty ("theme"))
        darkTheme = state.getProperty ("theme").toString() == "dark";
    if (state.hasProperty ("editorScale"))
        setEditorScale ((float) state.getProperty ("editorScale"));

    apvts.replaceState (state);
    updateLatency();
}

//==============================================================================
juce::AudioProcessorEditor* DeSaturatorProcessor::createEditor()
{
    return new DeSaturatorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeSaturatorProcessor();
}
