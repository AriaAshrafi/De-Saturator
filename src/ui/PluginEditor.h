#pragma once

#include "DeSaturatorView.h"
#include "plugin/PluginProcessor.h"

/** The plugin window: the 960 x 600 view under one scale transform, resizable from 75% to 200%
    at a fixed 16:10. It takes no keys and never takes keyboard focus from the host. */
class DeSaturatorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeSaturatorEditor (DeSaturatorProcessor&);

    void resized() override;

private:
    DeSaturatorProcessor& processor;
    DeSaturatorView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeSaturatorEditor)
};
