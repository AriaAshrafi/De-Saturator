#include "DeSaturatorView.h"

DeSaturatorView::DeSaturatorView (DeSaturatorProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    setOpaque (true);
    setSize (designWidth, designHeight);
}

void DeSaturatorView::paint (juce::Graphics& g)
{
    juce::ignoreUnused (processor);
    g.fillAll (juce::Colour (0xffecebe6));
    g.setColour (juce::Colour (0xff0c0c0b));
    g.drawText ("De-Saturator", getLocalBounds(), juce::Justification::centred);
}
