#pragma once

#include "plugin/PluginProcessor.h"

/** The whole window, drawn at 960 x 600 from design/spec. PLACEHOLDER: the real view is being built. */
class DeSaturatorView : public juce::Component
{
public:
    static constexpr int designWidth = 960;
    static constexpr int designHeight = 600;

    explicit DeSaturatorView (DeSaturatorProcessor&);

    void paint (juce::Graphics&) override;

private:
    DeSaturatorProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeSaturatorView)
};
