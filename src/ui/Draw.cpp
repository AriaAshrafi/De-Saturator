#include "Draw.h"

namespace desat::ui
{
float textWidth (const juce::Font& font, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (font, s);
}

void text (juce::Graphics& g, const juce::Font& font, float x, float baseline, const juce::String& s, juce::Colour colour, Align align)
{
    if (s.isEmpty())
        return;

    if (align != Align::start)
    {
        const float w = textWidth (font, s);
        x -= align == Align::middle ? 0.5f * w : w;
    }

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, s, x, baseline);
    g.setColour (colour);
    glyphs.draw (g);
}

juce::Point<float> polar (float cx, float cy, float r, float degrees)
{
    const float a = juce::degreesToRadians (degrees);
    return { cx + r * std::sin (a), cy - r * std::cos (a) };
}

juce::Path arc (float cx, float cy, float r, float fromDegrees, float toDegrees)
{
    juce::Path p;
    if (std::abs (toDegrees - fromDegrees) < 0.05f)
        return p;
    if (toDegrees < fromDegrees)
        std::swap (fromDegrees, toDegrees);
    p.addCentredArc (cx, cy, r, r, 0.0f, juce::degreesToRadians (fromDegrees), juce::degreesToRadians (toDegrees), true);
    return p;
}

void strokeArc (juce::Graphics& g, float cx, float cy, float r, float fromDegrees, float toDegrees, juce::Colour colour, float width)
{
    const auto p = arc (cx, cy, r, fromDegrees, toDegrees);
    if (p.isEmpty())
        return;
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
}

void strokeCircle (juce::Graphics& g, float cx, float cy, float r, juce::Colour colour, float width)
{
    g.setColour (colour);
    g.drawEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r, width);
}

void fillCircle (juce::Graphics& g, float cx, float cy, float r, juce::Colour colour)
{
    g.setColour (colour);
    g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
}

void line (juce::Graphics& g, float x1, float y1, float x2, float y2, juce::Colour colour, float width)
{
    g.setColour (colour);

    // Upright and level lines are filled as rectangles: exact, and much cheaper than a stroked path.
    if (juce::approximatelyEqual (x1, x2))
        g.fillRect (juce::Rectangle<float> (x1 - 0.5f * width, std::min (y1, y2), width, std::abs (y2 - y1)));
    else if (juce::approximatelyEqual (y1, y2))
        g.fillRect (juce::Rectangle<float> (std::min (x1, x2), y1 - 0.5f * width, std::abs (x2 - x1), width));
    else
        g.drawLine (x1, y1, x2, y2, width);
}

void dashedLine (juce::Graphics& g, float x1, float y1, float x2, float y2, juce::Colour colour, float width, float dash, float gap)
{
    const float lengths[] = { dash, gap };
    g.setColour (colour);
    g.drawDashedLine ({ x1, y1, x2, y2 }, lengths, 2, width);
}

void fillPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    g.setColour (colour);
    g.fillRoundedRectangle (r, 0.5f * juce::jmin (r.getWidth(), r.getHeight()));
}

void strokePill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float width)
{
    g.setColour (colour);
    g.drawRoundedRectangle (r, 0.5f * juce::jmin (r.getWidth(), r.getHeight()), width);
}
} // namespace desat::ui
