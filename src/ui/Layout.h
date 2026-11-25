#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

/** The window's layout in px at 100%, as in design/spec/SPEC.md (which is made from design/build/ui/core.js).
    Nothing here is tuned by eye: change the design first, then these. */
namespace desat::ui::layout
{
inline constexpr float width = 960.0f, height = 600.0f;

// The analyser's axes.
inline constexpr float freqMin = 20.0f, freqMax = 20000.0f;
inline constexpr float dbMin = -90.0f, dbMax = 6.0f;
inline const float octaves = std::log2 (freqMax / freqMin);

struct Segment
{
    const char* label;
    float width;
};

/** A switch: a pill of segments, one chosen. */
struct Switch
{
    float x, y, h;
    Segment segments[2];

    float totalWidth() const noexcept { return segments[0].width + segments[1].width; }
    float segmentX (int i) const noexcept { return i == 0 ? x : x + segments[0].width; }
    juce::Rectangle<float> segmentBounds (int i) const noexcept { return { segmentX (i), y, segments[i].width, h }; }
    juce::Rectangle<float> bounds() const noexcept { return { x, y, totalWidth(), h }; }
};

inline constexpr Switch character { 472.0f, 12.0f, 32.0f, { { "Clean", 64.0f }, { "Character", 96.0f } } };
inline constexpr Switch quality { 648.0f, 12.0f, 32.0f, { { "Live", 52.0f }, { "HQ", 52.0f } } };
inline constexpr Switch theme { 848.0f, 562.0f, 24.0f, { { "Light", 44.0f }, { "Dark", 44.0f } } };

namespace wordmark { inline constexpr float x = 24.0f, baseline = 38.0f; }
namespace legend { inline constexpr float baseline = 64.0f, result = 24.0f, original = 100.0f, removed = 190.0f, swatch = 18.0f, textGap = 24.0f; }
namespace latency { inline constexpr float x = 760.0f, baseline = 32.0f; }
namespace bypass { inline constexpr float cx = 920.0f, cy = 28.0f, r = 16.0f; }

inline const juce::Rectangle<float> delta { 820.0f, 12.0f, 72.0f, 32.0f };
inline const juce::Rectangle<float> autoGain { 852.0f, 504.0f, 80.0f, 24.0f };
inline const juce::Rectangle<float> analyser { 24.0f, 72.0f, 632.0f, 368.0f };
inline const juce::Rectangle<float> plot { 24.0f, 72.0f, 632.0f, 346.0f };

namespace strip { inline constexpr float baseline = 433.0f; }

namespace comb
{
inline constexpr int lines = 120;
inline constexpr float base = 72.0f + 346.0f + 2.0f; // the body starts just under the plot, so its end is never seen
inline float x (int j) noexcept { return plot.getX() + ((float) j + 0.5f) * plot.getWidth() / (float) lines; }
} // namespace comb

namespace amount
{
inline constexpr float cx = 808.0f, cy = 222.0f, r = 84.0f, track = 96.0f, tickIn = 104.0f, tickOut = 109.0f, tickMajor = 113.0f;
inline constexpr float notch = 6.0f, title = 100.0f, readout = 358.0f, chipW = 80.0f, chipH = 32.0f, chipY = 334.0f, hit = 106.0f;
} // namespace amount

namespace small
{
inline constexpr float cy = 428.0f, r = 22.0f, track = 29.0f, pointerIn = 7.0f, pointerOut = 18.0f, label = 474.0f, value = 493.5f, hit = 32.0f;
inline constexpr float cx[3] = { 724.0f, 808.0f, 892.0f }; // Restore, Mix, Output
} // namespace small

namespace shape
{
inline constexpr float x1 = 120.0f, x2 = 560.0f, y = 514.0f, title = 492.0f, thumbW = 16.0f, thumbH = 30.0f, endGap = 14.0f;
inline const juce::Rectangle<float> hit { x1 - 12.0f, y - 18.0f, x2 - x1 + 24.0f, 36.0f };
} // namespace shape

namespace credit { inline constexpr float x = 24.0f, y = 566.0f, w = 54.0f, h = 15.0f, versionX = 86.0f, versionBaseline = 578.0f; }
namespace point { inline constexpr float r = 6.5f, rHover = 8.5f, rSelected = 7.0f, halo = 12.0f, hit = 13.0f; }
namespace tooltip { inline constexpr float maxWidth = 220.0f, padX = 10.0f, padY = 8.0f, gap = 8.0f, lineHeight = 16.0f, size = 12.0f; inline constexpr int delayMs = 600; }

/** Stroke widths, px at 100%. */
namespace stroke
{
inline constexpr float frame = 1.5f, grid = 1.0f, body = 2.6f, result = 2.0f, original = 1.0f, hair = 1.0f, removedDelta = 2.6f;
inline constexpr float outline = 1.5f, outlineHover = 2.5f, curve = 2.0f, curveApplied = 2.25f, curveDrawn = 1.25f, curveUnder = 5.0f;
inline constexpr float knobTrack = 3.5f, amountArc = 2.0f, tick = 1.25f, pointRing = 2.0f, bracket = 1.25f, ringMax = 44.0f, ringMin = 1.5f;
} // namespace stroke

/** Departure Mono's advance in em. The design sizes its chips from the character count. */
inline constexpr float monoAdvance = 0.6f;
inline float chipWidth (int characters, float size = 11.0f) noexcept { return (float) characters * size * monoAdvance + 12.0f; }

// Frequency <-> x, level -> y, curve value <-> y.
inline float xForFreq (double f) noexcept { return plot.getX() + plot.getWidth() * (float) (std::log2 (f / freqMin) / octaves); }
inline double freqForX (float x) noexcept { return freqMin * std::pow (2.0, (double) octaves * (x - plot.getX()) / plot.getWidth()); }
inline float yForDb (float db) noexcept { return plot.getY() + plot.getHeight() * (dbMax - db) / (dbMax - dbMin); }
inline float yForValue (double v) noexcept { return plot.getY() + plot.getHeight() * (float) (1.0 - v / 2.0); }
inline double valueForY (float y) noexcept { return 2.0 * (1.0 - (y - plot.getY()) / plot.getHeight()); }

/** A knob's angle for a value 0..1: 0 degrees is up, clockwise. */
inline float knobAngle (float normalised) noexcept { return -135.0f + 270.0f * normalised; }
} // namespace desat::ui::layout
