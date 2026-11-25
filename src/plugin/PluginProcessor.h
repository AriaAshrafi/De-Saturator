#pragma once

#include "Analyser.h"
#include "dsp/Engine.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

/** Parameter IDs, as in design/spec/SPEC.md. Never rename one after a release: hosts store them. */
namespace ParamIDs
{
inline constexpr const char* amount = "amount";
inline constexpr const char* shape = "shape";
inline constexpr const char* restore = "restore";
inline constexpr const char* mix = "mix";
inline constexpr const char* output = "output";
inline constexpr const char* autoGain = "autoGain";
inline constexpr const char* delta = "delta";
inline constexpr const char* character = "character"; // choice: 0 Clean, 1 Character
inline constexpr const char* quality = "quality";     // choice: 0 Live, 1 HQ
inline constexpr const char* bypass = "bypass";
} // namespace ParamIDs

class DeSaturatorProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    DeSaturatorProcessor();
    ~DeSaturatorProcessor() override;

    //==============================================================================
    // What the window uses. All of it is for the message thread.

    juce::AudioProcessorValueTreeState apvts;

    /** The thinning curve. It is state, not a set of parameters: hosts save it but don't automate it. */
    desat::Curve getCurve() const;
    void setCurve (const desat::Curve& curve);

    /** Goes up whenever the curve changes from anywhere (the window, or a host loading state). */
    int getCurveVersion() const noexcept { return curveVersion.load(); }

    /** The curve as the running mode applies it: 0 to 2 on the desat::curveDisplayFreq() axis.
        In HQ it is the curve as drawn; in Live it is the smoothed curve. */
    void getEffectiveCurve (std::array<float, desat::curveDisplayPoints>& out);

    /** Latency of the running mode, for the label beside Live / HQ. */
    double getLatencyMs() const;

    Analyser& getAnalyser() noexcept { return analyser; }

    /** Light or dark window. A window preference, kept with the plugin's state; a new instance
        starts with whichever was chosen last. */
    bool isDarkTheme() const noexcept { return darkTheme.load(); }
    void setDarkTheme (bool dark);

    /** The window's size as a factor of 960 x 600, 0.75 to 2. */
    float getEditorScale() const noexcept { return editorScale.load(); }
    void setEditorScale (float scale) noexcept { editorScale = juce::jlimit (0.75f, 2.0f, scale); }

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "De-Saturator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    template <typename Float>
    void process (juce::AudioBuffer<Float>& buffer, bool hostBypass);

    desat::EngineParams readParams() const noexcept;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void updateLatency();

    desat::Engine engine;
    Analyser analyser;
    juce::AudioBuffer<double> scratch;
    int engineChannels = 2;
    double currentSampleRate = 48000.0;

    std::atomic<float>* amount = nullptr;
    std::atomic<float>* shape = nullptr;
    std::atomic<float>* restore = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* autoGain = nullptr;
    std::atomic<float>* delta = nullptr;
    std::atomic<float>* character = nullptr;
    std::atomic<float>* quality = nullptr;
    std::atomic<float>* bypass = nullptr;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    // The curve crosses from the message thread to the audio thread under a spin lock the audio
    // thread only ever tries, never waits on.
    mutable juce::SpinLock curveLock;
    desat::Curve curve;
    std::atomic<int> curveVersion { 0 };
    int curveSeenByAudio = -1;

    desat::LiveThinner liveCurveModel; // message thread only: works out the smoothed curve for the window

    std::atomic<bool> darkTheme { false };
    std::atomic<float> editorScale { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeSaturatorProcessor)
};
