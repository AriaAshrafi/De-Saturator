#include "PluginEditor.h"

DeSaturatorEditor::DeSaturatorEditor (DeSaturatorProcessor& p)
    : AudioProcessorEditor (p), processor (p), view (p)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    setOpaque (true);

    addAndMakeVisible (view);

    const int w = DeSaturatorView::designWidth, h = DeSaturatorView::designHeight;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) w / h);
    setResizeLimits (w * 3 / 4, h * 3 / 4, w * 2, h * 2);

    const float scale = processor.getEditorScale();
    setSize (juce::roundToInt (w * scale), juce::roundToInt (h * scale));
}

void DeSaturatorEditor::resized()
{
    const float scale = (float) getWidth() / (float) DeSaturatorView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, DeSaturatorView::designWidth, DeSaturatorView::designHeight);
    processor.setEditorScale (scale);
}
