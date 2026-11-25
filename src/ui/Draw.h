#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Small drawing helpers that follow the conventions of the design's drawing code (SVG): text sits on
    a baseline with an anchor, strokes are centred on their geometry, angles are degrees with 0 up, clockwise. */
namespace desat::ui
{
enum class Align { start, middle, end };

float textWidth (const juce::Font&, const juce::String&);

/** One line of text on a baseline, like SVG text with text-anchor. */
void text (juce::Graphics&, const juce::Font&, float x, float baseline, const juce::String&, juce::Colour, Align = Align::start);

juce::Point<float> polar (float cx, float cy, float r, float degrees);
juce::Path arc (float cx, float cy, float r, float fromDegrees, float toDegrees);

void strokeArc (juce::Graphics&, float cx, float cy, float r, float fromDegrees, float toDegrees, juce::Colour, float width);
void strokeCircle (juce::Graphics&, float cx, float cy, float r, juce::Colour, float width);
void fillCircle (juce::Graphics&, float cx, float cy, float r, juce::Colour);
void line (juce::Graphics&, float x1, float y1, float x2, float y2, juce::Colour, float width);
void dashedLine (juce::Graphics&, float x1, float y1, float x2, float y2, juce::Colour, float width, float dash, float gap);

/** A rectangle with fully round ends (radius half its shorter side). */
void fillPill (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void strokePill (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float width);
} // namespace desat::ui
