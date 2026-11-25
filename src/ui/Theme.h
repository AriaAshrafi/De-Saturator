#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace desat::ui
{
/** The colour roles of design/spec/SPEC.md. Each has a light and a dark value; nothing is mixed on the spot. */
struct Palette
{
    juce::Colour ground, text, textMuted, grid;
    juce::Colour body, result, original, removed; // the analyser
    juce::Colour hand;                            // what you set
    juce::Colour selected, onSelected, onHand, readout, onReadout;
    float bodyAlpha = 0.16f;                      // the only colour drawn at an opacity
};

/** The type roles: Anybody for words, Departure Mono (a pixel face, drawn for 11 and 22 px) for values. */
struct Fonts
{
    juce::Font title, button, label, tooltip, value, valueLarge;
};

/** A drawing from design/ui/assets: one outlined path, in the file's own coordinates. */
struct Mark
{
    juce::Path path;
    juce::Rectangle<float> viewBox;
    juce::Colour fill;
};

/** The palettes, fonts and drawings, loaded once from the embedded design files.
    Held through a juce::SharedResourcePointer, so the typefaces go when the last window does
    rather than at static destruction, after JUCE has shut down. */
class Theme
{
public:
    Theme();

    const Palette& palette (bool dark) const noexcept { return dark ? darkPalette : lightPalette; }
    const Fonts& fonts() const noexcept { return type; }
    const Mark& wordmark (bool dark) const noexcept { return dark ? wordmarkDark : wordmarkLight; }
    const Mark& credit (bool dark) const noexcept { return dark ? creditDark : creditLight; }

private:
    Palette lightPalette, darkPalette;
    Fonts type;
    Mark wordmarkLight, wordmarkDark, creditLight, creditDark;
};
} // namespace desat::ui
