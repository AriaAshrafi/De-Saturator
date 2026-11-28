#include "DeSaturatorView.h"

#include "Draw.h"
#include "Layout.h"

#include <cstring>

using namespace desat::ui;
namespace L = desat::ui::layout;
namespace S = desat::ui::layout::stroke;

using Control = DeSaturatorView::Control;
using Target = DeSaturatorView::Target;

namespace
{
constexpr int timerHz = 60;
constexpr int noteMs = 2000;
constexpr float dragFullRangePx = 240.0f, dragFineRangePx = 1200.0f;
constexpr double widthMin = 0.3, widthMax = 4.0, widthDefault = 1.0, widthStep = 0.1;
constexpr float bypassDim = 0.45f;

// What one detent of a mouse wheel reads as in JUCE (120 wheel units on Windows).
constexpr float wheelNotch = 0.234375f;

const char* const noteBypass = "Bypassed";
const char* const noteDelta = "Delta: you hear only what's removed";
const char* const noteLive = "Live: the applied curve is smoothed";
const char* const noteFull = "Six points is the most";

const juce::String minusSign = juce::String::fromUTF8 ("\xe2\x88\x92");

bool isKnob (Control c) noexcept
{
    return c == Control::amount || c == Control::restore || c == Control::mix || c == Control::output;
}

bool isToggle (Control c) noexcept
{
    return c == Control::autoGain || c == Control::delta || c == Control::bypass;
}

const char* parameterId (Control c) noexcept
{
    switch (c)
    {
        case Control::amount:    return ParamIDs::amount;
        case Control::restore:   return ParamIDs::restore;
        case Control::mix:       return ParamIDs::mix;
        case Control::output:    return ParamIDs::output;
        case Control::shape:     return ParamIDs::shape;
        case Control::autoGain:  return ParamIDs::autoGain;
        case Control::delta:     return ParamIDs::delta;
        case Control::bypass:    return ParamIDs::bypass;
        case Control::character: return ParamIDs::character;
        case Control::quality:   return ParamIDs::quality;
        case Control::none:
        case Control::theme:
        case Control::point:
        case Control::display:   break;
    }
    return nullptr;
}

juce::String percentText (float value)
{
    return juce::String (juce::roundToInt (value)) + "%";
}

/** One decimal, a sign (a real minus), "dB": "-1.5 dB", "0.0 dB", "+3.0 dB". */
juce::String decibelText (float value)
{
    const float r = std::round (value * 10.0f) / 10.0f;
    return (r > 0.0f ? juce::String ("+") : r < 0.0f ? minusSign : juce::String()) + juce::String (std::abs (r), 1) + " dB";
}

/** "140 Hz", "3.3 kHz", "12 kHz". */
juce::String frequencyText (double f)
{
    if (f < 1000.0)
        return juce::String (juce::roundToInt (f)) + " Hz";
    return (f < 9950.0 ? juce::String (f / 1000.0, 1) : juce::String (juce::roundToInt (f / 1000.0))) + " kHz";
}

/** The design breaks tooltip lines by character count (0.52 em a character), so the plugin's
    tooltips break where the design's pictures do. */
juce::StringArray wrapTooltip (const juce::String& message)
{
    const int perLine = (int) std::floor ((L::tooltip::maxWidth - 2.0f * L::tooltip::padX) / (L::tooltip::size * 0.52f));
    juce::StringArray words, lines;
    words.addTokens (message, " ", "");

    juce::String current;
    for (const auto& word : words)
    {
        const auto longer = (current + " " + word).trim();
        if (longer.length() > perLine && current.isNotEmpty())
        {
            lines.add (current);
            current = word;
        }
        else
        {
            current = longer;
        }
    }
    if (current.isNotEmpty())
        lines.add (current);
    return lines;
}

juce::Rectangle<float> circleBox (float cx, float cy, float r)
{
    return { cx - r, cy - r, 2.0f * r, 2.0f * r };
}

/** Where a pointer event is on the screen. Knob drags are measured there, so a drag is the same
    length of hand movement whatever size the window is. */
juce::Point<float> screenPosition (const juce::MouseEvent& e)
{
    return e.eventComponent != nullptr ? e.eventComponent->localPointToGlobal (e.position) : e.position;
}
} // namespace

//==============================================================================
DeSaturatorView::DeSaturatorView (DeSaturatorProcessor& p) : processor (p)
{
    // No keyboard: the host keeps every key.
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    setOpaque (true);
    setSize (designWidth, designHeight);
    setTitle ("De-Saturator");

    st = readState();
    refreshCurve();
    buildAccessibility();

    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (timerHz);
}

DeSaturatorView::~DeSaturatorView()
{
    stopTimer();
    endGesture(); // a host must never be left inside a gesture
}

//==============================================================================
void DeSaturatorView::setPreview (const Preview& p)
{
    endGesture();
    preview = p;
    ui = { p.hover, p.drag, p.tip, p.selectedPoint, p.note, p.tipAt };
    tipDue = noteUntil = 0;
    st = readState();
    refreshCurve();
    syncAccessibility();
    repaint();
}

void DeSaturatorView::clearPreview()
{
    preview.reset();
    ui = {};
    st = readState();
    refreshCurve();
    lastTick = juce::Time::getMillisecondCounterHiRes();
    repaint();
}

juce::String DeSaturatorView::getStateLine() const { return stateLine(); }
juce::String DeSaturatorView::getTooltipText() const { return tipText (ui.tip); }

//==============================================================================
DeSaturatorView::State DeSaturatorView::readState() const
{
    State s;
    s.amount = plainValue (Control::amount);
    s.shape = plainValue (Control::shape);
    s.restore = plainValue (Control::restore);
    s.mix = plainValue (Control::mix);
    s.output = plainValue (Control::output);
    s.autoGain = plainValue (Control::autoGain) >= 0.5f;
    s.delta = plainValue (Control::delta) >= 0.5f;
    s.bypass = plainValue (Control::bypass) >= 0.5f;
    s.character = juce::roundToInt (plainValue (Control::character));
    s.quality = juce::roundToInt (plainValue (Control::quality));
    s.dark = preview.has_value() && preview->dark.has_value() ? *preview->dark : processor.isDarkTheme();
    s.curveVersion = processor.getCurveVersion();

    // Live has no latency; the label says so rather than going blank.
    const double ms = preview.has_value() && preview->latencyMs.has_value() ? *preview->latencyMs : processor.getLatencyMs();
    s.latencyMs = s.quality == 1 ? juce::roundToInt (ms) : 0;
    return s;
}

void DeSaturatorView::refreshCurve()
{
    curve = processor.getCurve();

    // The curve can change under the pointer (a host loading state): drop what no longer exists.
    if (ui.selected >= curve.count)
        ui.selected = -1;
    if (ui.hover.control == Control::point && ui.hover.index >= curve.count)
        ui.hover = {};
    if (ui.drag.control == Control::point && ui.drag.index >= curve.count)
        ui.drag = {};
    if (ui.tip.control == Control::point && ui.tip.index >= curve.count)
        ui.tip = {};

    // In HQ the applied curve is the drawn one; only Live needs the processor's smoothed curve.
    if (st.quality == 0 && (appliedVersion != st.curveVersion || appliedQuality != st.quality))
        processor.getEffectiveCurve (applied);
    appliedVersion = st.curveVersion;
    appliedQuality = st.quality;
}

bool DeSaturatorView::isOn (Control c) const noexcept
{
    return ui.hover.control == c || ui.drag.control == c;
}

float DeSaturatorView::hairline (float width) const noexcept
{
    // At 75% a 1 px line would be three quarters of a physical pixel: it's held at one.
    return juce::jmax (width, pixel);
}

void DeSaturatorView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double seconds = juce::jlimit (0.0, 0.1, (now - lastTick) / 1000.0);
    lastTick = now;

    if (preview.has_value())
        return; // a picture is being taken: hold still

    const auto analyserArea = L::analyser.expanded (2.0f).getSmallestIntegerContainer();

    auto& analyser = processor.getAnalyser();
    analyser.update (seconds);
    if (analyser.hasSignal() || hadSignal)
        repaint (analyserArea);
    hadSignal = analyser.hasSignal();

    const auto ticks = juce::Time::getMillisecondCounter();

    if (tipDue != 0 && ticks >= tipDue)
    {
        tipDue = 0;
        if (ui.hover && ! ui.drag)
        {
            ui.tip = ui.hover;
            repaint();
        }
    }

    if (noteUntil != 0 && ticks >= noteUntil)
    {
        noteUntil = 0;
        ui.note.clear();
        repaint (analyserArea);
    }

    // Whatever changed in the processor, from the host or from here, is drawn on this frame.
    const auto latest = readState();
    if (! (latest == st))
    {
        st = latest;
        refreshCurve();
        syncAccessibility();
        repaint();
    }
}

//==============================================================================
void DeSaturatorView::paint (juce::Graphics& g)
{
    colours = &theme->palette (st.dark);
    pixel = 1.0f / juce::jmax (0.01f, g.getInternalContext().getPhysicalPixelScaleFactor());

    // The analyser is drawn many times a second and everything else only when it changes, so a
    // repaint of just the analyser skips the rest. Nothing else reaches into its frame.
    const bool analyserOnly = L::analyser.expanded (3.0f).contains (g.getClipBounds().toFloat());

    paintBackground (g);

    {
        const juce::Graphics::ScopedSaveState clipped (g);
        g.reduceClipRegion (L::plot.toNearestInt());
        paintComb (g);
    }

    if (! analyserOnly)
    {
        paintHeader (g);
        paintAmount (g);
        paintSmallKnob (g, Control::restore, "Restore", L::small::cx[0]);
        paintSmallKnob (g, Control::mix, "Mix", L::small::cx[1]);
        paintSmallKnob (g, Control::output, "Output", L::small::cx[2]);
        paintPill (g, L::autoGain, Control::autoGain, "Auto gain", st.autoGain, colours->selected, colours->onSelected, theme->fonts().label);
        paintShape (g);
        paintCredit (g);
        paintSwitch (g, L::theme, Control::theme, st.dark ? 1 : 0);
        paintLegend (g);
    }

    paintOverlay (g);
    paintTooltip (g);
}

void DeSaturatorView::paintBackground (juce::Graphics& g) const
{
    const auto& c = *colours;
    const auto& value = theme->fonts().value;
    const auto P = L::plot;

    g.fillAll (c.ground);

    // The frame's stroke is centred on its edge.
    g.setColour (c.text);
    g.drawRect (L::analyser.expanded (0.5f * S::frame), S::frame);

    const float grid = hairline (S::grid);
    for (const float db : { -72.0f, -48.0f, -24.0f, 0.0f })
        dashedLine (g, P.getX(), L::yForDb (db), P.getRight(), L::yForDb (db), c.grid, grid, 2.0f, 4.0f);

    for (const int f : { 50, 100, 200, 500, 1000, 2000, 5000, 10000 })
    {
        const float x = L::xForFreq (f);
        const bool major = f == 100 || f == 1000 || f == 10000;
        line (g, x, P.getY(), x, P.getBottom(), major ? c.grid : c.grid.withAlpha (0.55f), grid);

        const auto label = f == 50 ? juce::String ("50 Hz") : f >= 1000 ? juce::String (f / 1000) + "k" : juce::String (f);
        text (g, value, x, L::strip::baseline, label, c.textMuted, Align::middle);
    }

    line (g, P.getX(), P.getBottom(), P.getRight(), P.getBottom(), c.grid, grid);
}

void DeSaturatorView::paintComb (juce::Graphics& g) const
{
    const auto& c = *colours;
    const auto& analyser = processor.getAnalyser();
    const auto& input = analyser.input();
    const auto& result = analyser.result();

    // What stays (the result) and where the original reached, as y. With no signal both lie along
    // the plot's bottom edge, as in the design's "No signal" picture.
    std::array<float, L::comb::lines> keep, top;
    for (int j = 0; j < L::comb::lines; ++j)
    {
        keep[(size_t) j] = L::yForDb (juce::jlimit (L::dbMin, L::dbMax, result[(size_t) j]));
        top[(size_t) j] = L::yForDb (juce::jlimit (L::dbMin, L::dbMax, input[(size_t) j]));
    }

    // Body: what stays, kept quiet.
    {
        juce::RectangleList<float> body;
        for (int j = 0; j < L::comb::lines; ++j)
            if (keep[(size_t) j] < L::comb::base - 0.05f)
                body.addWithoutMerging ({ L::comb::x (j) - 0.5f * S::body, keep[(size_t) j], S::body, L::comb::base - keep[(size_t) j] });
        g.setColour (c.body.withAlpha (c.bodyAlpha));
        g.fillRectList (body);
    }

    // Removed: hairlines from the result up to the original. In Delta that is what you hear, so
    // it turns heavy and takes the hand's colour. Bypassed, nothing is removed.
    if (! st.bypass)
    {
        const float w = st.delta ? S::removedDelta : hairline (S::hair);
        juce::RectangleList<float> removed;
        for (int j = 0; j < L::comb::lines; ++j)
            if (top[(size_t) j] < keep[(size_t) j] - 0.05f)
                removed.addWithoutMerging ({ L::comb::x (j) - 0.5f * w, top[(size_t) j], w, keep[(size_t) j] - top[(size_t) j] });
        g.setColour (st.delta ? c.hand : c.removed);
        g.fillRectList (removed);
    }

    // The two lines across every comb line, carried level to the plot's edges.
    auto contour = [] (const std::array<float, L::comb::lines>& y)
    {
        juce::Path p;
        p.startNewSubPath (L::plot.getX(), y.front());
        for (int j = 0; j < L::comb::lines; ++j)
            p.lineTo (L::comb::x (j), y[(size_t) j]);
        p.lineTo (L::plot.getRight(), y.back());
        return p;
    };
    auto stroke = [&g] (const juce::Path& p, juce::Colour colour, float width)
    {
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    if (! st.bypass)
        stroke (contour (top), c.original, hairline (S::original));

    if (st.delta && ! st.bypass)
        stroke (contour (keep), c.textMuted, hairline (S::original));
    else
        stroke (contour (keep), c.result, S::result);
}

//==============================================================================
void DeSaturatorView::paintHeader (juce::Graphics& g) const
{
    const auto& c = *colours;

    // The name is a drawing, with its baseline at y 0 in the file.
    const auto& name = theme->wordmark (st.dark);
    g.setColour (name.fill);
    g.fillPath (name.path, juce::AffineTransform::translation (L::wordmark::x, L::wordmark::baseline));

    paintSwitch (g, L::character, Control::character, st.character);
    paintSwitch (g, L::quality, Control::quality, st.quality);

    text (g, theme->fonts().value, L::latency::x, L::latency::baseline, juce::String (st.latencyMs) + " ms", c.textMuted);

    paintPill (g, L::delta, Control::delta, "Delta", st.delta, c.hand, c.onHand, theme->fonts().button);
    paintBypass (g);
}

void DeSaturatorView::paintSwitch (juce::Graphics& g, const L::Switch& sw, Control control, int chosen) const
{
    const auto& c = *colours;
    const auto& font = sw.h > 28.0f ? theme->fonts().button : theme->fonts().label;

    strokePill (g, sw.bounds(), c.text, S::outline);

    for (int i = 0; i < 2; ++i)
    {
        const auto r = sw.segmentBounds (i);
        const bool on = chosen == i;

        if (on)
            fillPill (g, r.reduced (3.0f), c.selected);
        else if (ui.hover == Target { control, i })
            strokePill (g, r.reduced (3.5f), c.text, hairline (1.0f));

        text (g, font, r.getCentreX(), sw.y + 0.5f * sw.h + 4.5f, sw.segments[i].label, on ? c.onSelected : c.text, Align::middle);
    }
}

void DeSaturatorView::paintPill (juce::Graphics& g, juce::Rectangle<float> r, Control control, const char* label, bool on,
                                 juce::Colour onFill, juce::Colour onText, const juce::Font& font) const
{
    const auto& c = *colours;
    const float outline = ui.hover.control == control ? S::outlineHover : S::outline;

    if (on)
        fillPill (g, r, onFill);
    strokePill (g, r, on ? onFill : c.text, outline);
    text (g, font, r.getCentreX(), r.getCentreY() + 4.5f, label, on ? onText : c.text, Align::middle);
}

void DeSaturatorView::paintBypass (juce::Graphics& g) const
{
    const auto& c = *colours;
    const float cx = L::bypass::cx, cy = L::bypass::cy, r = L::bypass::r;
    const auto icon = st.bypass ? c.onSelected : c.text;

    if (st.bypass)
        fillCircle (g, cx, cy, r, c.selected);
    strokeCircle (g, cx, cy, r, c.text, ui.hover.control == Control::bypass ? S::outlineHover : S::outline);

    // The power sign: an open ring and a stem.
    strokeArc (g, cx, cy + 1.0f, 7.0f, 38.0f, 322.0f, icon, 2.0f);
    line (g, cx, cy - 9.0f, cx, cy - 1.0f, icon, 2.0f);
}

void DeSaturatorView::paintLegend (juce::Graphics& g) const
{
    const auto& c = *colours;
    const auto& label = theme->fonts().label;
    const float y = L::legend::baseline, mid = y - 4.0f;
    const bool delta = st.delta && ! st.bypass;

    // Each swatch is drawn as its layer is in the analyser right now.
    line (g, L::legend::result, mid, L::legend::result + L::legend::swatch, mid,
          delta ? c.textMuted : c.result, delta ? hairline (S::original) : S::result);
    text (g, label, L::legend::result + L::legend::textGap, y, "Result", c.textMuted);

    line (g, L::legend::original, mid, L::legend::original + L::legend::swatch, mid,
          st.bypass ? c.textMuted : c.original, hairline (S::original));
    text (g, label, L::legend::original + L::legend::textGap, y, "Original", c.textMuted);

    for (int k = 0; k < 4; ++k)
    {
        const float x = L::legend::removed + 1.0f + 5.0f * (float) k;
        line (g, x, mid - 5.0f, x, mid + 5.0f, delta ? c.hand : c.removed, delta ? 2.0f : hairline (S::hair));
    }
    text (g, label, L::legend::removed + L::legend::textGap, y, "Removed", c.textMuted);
}

//==============================================================================
void DeSaturatorView::paintAmount (juce::Graphics& g) const
{
    using namespace L::amount;
    const auto& c = *colours;
    const float n = st.amount / 100.0f, a = L::knobAngle (n);
    const bool hover = isOn (Control::amount), dragging = ui.drag.control == Control::amount;
    const auto valueColour = st.bypass ? c.textMuted : c.hand;
    const auto ringColour = st.bypass ? c.textMuted : c.text;

    text (g, theme->fonts().title, cx, title, "Amount", c.text, Align::middle);

    for (int t = 0; t <= 10; ++t)
    {
        const float ta = L::knobAngle ((float) t / 10.0f);
        const auto from = polar (cx, cy, tickIn, ta), to = polar (cx, cy, t % 5 != 0 ? tickOut : tickMajor, ta);
        line (g, from.x, from.y, to.x, to.y, hover ? c.text : c.textMuted, S::tick);
    }

    strokeArc (g, cx, cy, track, -135.0f, 135.0f, c.grid, S::amountArc);
    strokeArc (g, cx, cy, track, -135.0f, a, valueColour, S::amountArc);

    // The ring thins as Amount rises, from nearly a disc to a hairline; a notch at the pointer shows its turn.
    const float width = juce::jmap (n, S::ringMax, S::ringMin), middle = r - 0.5f * width;
    const float gap = juce::radiansToDegrees (0.5f * notch / middle);
    strokeArc (g, cx, cy, middle, a + gap, a + 360.0f - gap, ringColour, width);

    // The pointer: a dot on the track with a ring of ground around it.
    const auto dot = polar (cx, cy, track, a);
    const float dotRadius = dragging ? 7.5f : 5.5f;
    fillCircle (g, dot.x, dot.y, dotRadius + 1.0f, c.ground);
    fillCircle (g, dot.x, dot.y, dotRadius - 1.0f, valueColour);

    if (hover)
    {
        g.setColour (c.readout);
        g.fillRect (juce::Rectangle<float> (cx - 0.5f * chipW, chipY, chipW, chipH));
    }
    text (g, theme->fonts().valueLarge, cx, readout, valueText (Control::amount), hover ? c.onReadout : c.text, Align::middle);
}

void DeSaturatorView::paintSmallKnob (juce::Graphics& g, Control control, const char* label, float cx) const
{
    using namespace L::small;
    const auto& c = *colours;
    const auto* parameter = parameterFor (control);
    const float n = parameter->getValue(), a = L::knobAngle (n);
    const bool hover = isOn (control);

    // Output's arc grows from 0 dB, not from the low end.
    const float from = control == Control::output ? L::knobAngle (parameter->convertTo0to1 (0.0f)) : -135.0f;

    strokeArc (g, cx, cy, track, -135.0f, 135.0f, c.grid, S::knobTrack);
    strokeArc (g, cx, cy, track, from, a, st.bypass ? c.textMuted : c.hand, S::knobTrack);

    // A tick at 0 dB, outside the track: on a 60 dB knob unity can't be found by eye without it.
    if (control == Control::output)
    {
        const auto t0 = polar (cx, cy, 33.0f, from), t1 = polar (cx, cy, 38.0f, from);
        line (g, t0.x, t0.y, t1.x, t1.y, hover ? c.text : c.textMuted, S::tick);
    }

    fillCircle (g, cx, cy, r, c.ground);
    strokeCircle (g, cx, cy, r, c.text, hover ? S::outlineHover : S::outline);

    // Square caps: the 3 px pointer reaches half its width past each end.
    const auto p0 = polar (cx, cy, pointerIn - 1.5f, a), p1 = polar (cx, cy, pointerOut + 1.5f, a);
    line (g, p0.x, p0.y, p1.x, p1.y, c.text, 3.0f);

    text (g, theme->fonts().label, cx, L::small::label, label, c.text, Align::middle);

    const auto shown = valueText (control);
    if (hover)
    {
        const float w = L::chipWidth (shown.length());
        g.setColour (c.readout);
        g.fillRect (juce::Rectangle<float> (cx - 0.5f * w, value - 12.5f, w, 17.0f));
    }
    text (g, theme->fonts().value, cx, value, shown, hover ? c.onReadout : c.textMuted, Align::middle);
}

void DeSaturatorView::paintShape (juce::Graphics& g) const
{
    using namespace L::shape;
    const auto& c = *colours;
    const auto& label = theme->fonts().label;
    const auto& value = theme->fonts().value;
    const float x = juce::jmap (st.shape / 100.0f, x1, x2);
    const int body = juce::roundToInt (st.shape);

    text (g, label, 0.5f * (x1 + x2), title, "Shape", c.text, Align::middle);

    line (g, x1, y, x2, y, c.grid, 3.0f);
    line (g, 0.5f * (x1 + x2), y - 6.0f, 0.5f * (x1 + x2), y + 6.0f, c.grid, 1.5f);

    // Both ends show their share.
    text (g, label, x1 - endGap, y - 2.0f, "Harmonics", c.text, Align::end);
    text (g, value, x1 - endGap, y + 13.0f, juce::String (100 - body) + "%", c.textMuted, Align::end);
    text (g, label, x2 + endGap, y - 2.0f, "Body", c.text);
    text (g, value, x2 + endGap, y + 13.0f, juce::String (body) + "%", c.textMuted);

    const juce::Rectangle<float> thumb (x - 0.5f * thumbW, y - 0.5f * thumbH, thumbW, thumbH);
    fillPill (g, thumb, c.selected);
    line (g, x, y - 7.0f, x, y + 7.0f, c.onSelected, 2.0f);

    if (isOn (Control::shape))
    {
        g.setColour (c.text);
        g.drawRoundedRectangle (thumb.expanded (4.0f), 0.5f * thumbW + 4.0f, S::outline);
    }
}

void DeSaturatorView::paintCredit (juce::Graphics& g) const
{
    using namespace L::credit;
    const auto& mark = theme->credit (st.dark);

    // Fitted into its box from the left, centred up and down (SVG's xMinYMid meet).
    if (! mark.viewBox.isEmpty())
    {
        const float scale = juce::jmin (w / mark.viewBox.getWidth(), h / mark.viewBox.getHeight());
        const auto place = juce::AffineTransform::translation (-mark.viewBox.getX(), -mark.viewBox.getY())
                               .scaled (scale)
                               .translated (x, y + 0.5f * (h - mark.viewBox.getHeight() * scale));
        g.setColour (mark.fill);
        g.fillPath (mark.path, place);
    }

    const auto version = preview.has_value() && preview->versionText.isNotEmpty() ? preview->versionText : juce::String (DESAT_VERSION_STRING);
    text (g, theme->fonts().value, versionX, versionBaseline, version, colours->textMuted);
}

//==============================================================================
void DeSaturatorView::paintOverlay (juce::Graphics& g) const
{
    const auto& c = *colours;
    const auto& value = theme->fonts().value;
    const auto P = L::plot;
    const float dim = st.bypass ? bypassDim : 1.0f;

    auto stroke = [&g] (const juce::Path& p, juce::Colour colour, float width)
    {
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };

    // The curve as drawn, a value for every pixel across.
    juce::Path drawn;
    {
        const int steps = juce::roundToInt (P.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float x = P.getX() + (float) i;
            const float y = L::yForValue (curve.valueAt (L::freqForX (x)));
            if (i == 0)
                drawn.startNewSubPath (x, y);
            else
                drawn.lineTo (x, y);
        }
    }

    if (st.quality == 0)
    {
        // Live: the curve you drew, dashed, and the smoothed curve that's applied, solid.
        juce::Path smoothed;
        for (int i = 0; i < desat::curveDisplayPoints; ++i)
        {
            const float x = P.getX() + P.getWidth() * (float) i / (float) (desat::curveDisplayPoints - 1);
            const float y = L::yForValue (applied[(size_t) i]);
            if (i == 0)
                smoothed.startNewSubPath (x, y);
            else
                smoothed.lineTo (x, y);
        }

        stroke (smoothed, c.ground.withAlpha (dim), S::curveUnder);

        juce::Path dashes;
        const float lengths[] = { 4.0f, 4.0f };
        juce::PathStrokeType (S::curveDrawn).createDashedStroke (dashes, drawn, lengths, 2);
        g.setColour (c.hand.withAlpha (dim));
        g.fillPath (dashes);

        stroke (smoothed, c.hand.withAlpha (dim), S::curveApplied);
    }
    else
    {
        // A line of ground under the curve, so it reads over the analyser.
        stroke (drawn, c.ground.withAlpha (dim), S::curveUnder);
        stroke (drawn, c.hand.withAlpha (dim), S::curve);
    }

    // The curve's scale, down the right edge.
    auto scaleLabel = [&] (float baseline, const char* label)
    {
        const float w = L::chipWidth ((int) std::strlen (label)) - 4.0f;
        g.setColour (c.ground.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (P.getRight() - 6.0f - w, baseline - 11.0f, w, 15.0f));
        text (g, value, P.getRight() - 10.0f, baseline, label, c.textMuted, Align::end);
    };
    scaleLabel (P.getY() + 16.0f, "200%");
    scaleLabel (L::yForValue (1.0) - 7.0f, "100%");
    scaleLabel (P.getBottom() - 5.0f, "0%");

    // The state line, top left.
    if (const auto note = stateLine(); note.isNotEmpty())
    {
        g.setColour (c.ground.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (P.getX() + 8.0f, P.getY() + 8.0f, L::chipWidth (note.length()) + 2.0f, 19.0f));
        text (g, value, P.getX() + 15.0f, P.getY() + 21.5f, note, c.text);
    }

    if (curve.count == 0)
        return;

    // Points sit at their own frequency and amount, not on the summed curve.
    {
        if (st.bypass)
            g.beginTransparencyLayer (bypassDim);

        for (int i = 0; i < curve.count; ++i)
        {
            const auto& p = curve.points[(size_t) i];
            const float x = L::xForFreq (p.freq), y = L::yForValue (p.amount);
            const bool selected = ui.selected == i, hover = ui.hover == Target { Control::point, i };
            const bool dragging = ui.drag == Target { Control::point, i };

            if (selected || hover || dragging)
            {
                // The bell's width at half height.
                const float xa = L::xForFreq (juce::jlimit (20.0, 20000.0, p.freq * std::pow (2.0, -0.5 * p.width)));
                const float xb = L::xForFreq (juce::jlimit (20.0, 20000.0, p.freq * std::pow (2.0, 0.5 * p.width)));
                line (g, xa, y, xb, y, c.hand, S::bracket);
                line (g, xa, y - 5.0f, xa, y + 5.0f, c.hand, S::bracket);
                line (g, xb, y - 5.0f, xb, y + 5.0f, c.hand, S::bracket);
            }

            if (dragging && y + L::point::halo < P.getBottom())
                dashedLine (g, x, y + L::point::halo, x, P.getBottom(), c.hand, hairline (1.0f), 2.0f, 3.0f);

            if (selected || dragging)
            {
                strokeCircle (g, x, y, L::point::halo, c.hand, S::bracket);
                fillCircle (g, x, y, L::point::rSelected + 1.0f, c.ground);
                fillCircle (g, x, y, L::point::rSelected - 1.0f, c.hand);
            }
            else
            {
                const float r = hover ? L::point::rHover : L::point::r;
                fillCircle (g, x, y, r, c.ground);
                strokeCircle (g, x, y, r, c.hand, S::pointRing);
            }
        }

        if (st.bypass)
            g.endTransparencyLayer();
    }

    // One readout, over everything: the last point that is selected, under the pointer or held.
    for (int i = curve.count; --i >= 0;)
    {
        if (! (ui.selected == i || ui.hover == Target { Control::point, i } || ui.drag == Target { Control::point, i }))
            continue;

        const auto& p = curve.points[(size_t) i];
        const float x = L::xForFreq (p.freq), y = L::yForValue (p.amount);
        const auto readout = pointText (i);
        const float w = L::chipWidth (readout.length()) + 2.0f;
        const float left = juce::jlimit (P.getX() + 6.0f, P.getRight() - 6.0f - w, x - 0.5f * w);
        const float topEdge = y < P.getY() + 46.0f ? y + 22.0f : y - 39.0f; // under the point when it's near the top

        g.setColour (c.readout);
        g.fillRect (juce::Rectangle<float> (left, topEdge, w, 19.0f));
        text (g, value, left + 7.0f, topEdge + 13.5f, readout, c.onReadout);
        break;
    }
}

void DeSaturatorView::paintTooltip (juce::Graphics& g) const
{
    const auto message = tipText (ui.tip);
    if (message.isEmpty())
        return;

    namespace T = L::tooltip;
    const auto& font = theme->fonts().tooltip;
    const auto lines = wrapTooltip (message);

    int longest = 0;
    float widest = 0.0f;
    for (const auto& l : lines)
    {
        longest = juce::jmax (longest, l.length());
        widest = juce::jmax (widest, textWidth (font, l));
    }

    // The design sizes the box from the character count, which can leave a little less than the
    // padding on the right; the measured width only matters if a line would come close to the edge.
    const float w = juce::jmin (T::maxWidth, juce::jmax ((float) longest * T::size * 0.52f + 2.0f * T::padX, widest + T::padX + 4.0f));
    const float h = (float) lines.size() * T::lineHeight + 2.0f * T::padY - 4.0f;

    float x = 0.0f, y = 0.0f;
    if (const auto anchor = tipAnchor (ui.tip))
    {
        // Under the control, or above it when it would leave the window.
        x = anchor->getCentreX() - 0.5f * w;
        y = anchor->getBottom() + T::gap;
        if (y + h > L::height - 8.0f)
            y = anchor->getY() - T::gap - h;
    }
    else
    {
        x = ui.tipAt.x - 0.5f * w;
        y = ui.tipAt.y + 18.0f;
        if (y + h > L::height - 8.0f)
            y = ui.tipAt.y - 18.0f - h;
    }
    x = juce::jlimit (8.0f, L::width - 8.0f - w, x);

    g.setColour (colours->readout);
    g.fillRect (juce::Rectangle<float> (x, y, w, h));
    for (int i = 0; i < lines.size(); ++i)
        text (g, font, x + T::padX, y + T::padY + 9.0f + (float) i * T::lineHeight, lines[i], colours->onReadout);
}

//==============================================================================
juce::String DeSaturatorView::valueText (Control control) const
{
    switch (control)
    {
        case Control::amount:  return percentText (st.amount);
        case Control::restore: return percentText (st.restore);
        case Control::mix:     return percentText (st.mix);
        case Control::shape:   return percentText (st.shape);
        case Control::output:  return decibelText (st.output);
        case Control::none:
        case Control::autoGain:
        case Control::delta:
        case Control::bypass:
        case Control::character:
        case Control::quality:
        case Control::theme:
        case Control::point:
        case Control::display: break;
    }
    return {};
}

juce::String DeSaturatorView::pointText (int index) const
{
    if (! juce::isPositiveAndBelow (index, curve.count))
        return {};

    const auto& p = curve.points[(size_t) index];
    return frequencyText (p.freq) + "  " + juce::String (juce::roundToInt (p.amount * 100.0)) + "%  " + juce::String (p.width, 1) + " oct";
}

juce::String DeSaturatorView::stateLine() const
{
    // One thing at a time, the most important first.
    if (st.bypass)
        return noteBypass;
    if (st.delta)
        return noteDelta;
    if (ui.note.isNotEmpty())
        return ui.note;
    if (st.quality == 0)
        return noteLive;
    return {};
}

juce::String DeSaturatorView::tipText (Target t)
{
    switch (t.control)
    {
        case Control::amount:    return "How much thinner. The curve scales it at each frequency.";
        case Control::shape:     return "Harmonics takes out overtones. Body takes out weight.";
        case Control::restore:   return "Gives back the peaks that thinning flattens.";
        case Control::mix:       return "How much of the thinned sound you hear against the original.";
        case Control::output:    return "The level after everything else.";
        case Control::autoGain:  return "Keeps the level steady, so thinner doesn't just mean quieter.";
        case Control::delta:     return "Hear only what's being removed.";
        case Control::character: return t.index == 0 ? "Clean: transparent, for fixing." : "Character: goes further, for effect.";
        case Control::quality:   return t.index == 0 ? "Live: no latency. The curve is applied smoothed." : "HQ: full resolution. Adds latency.";
        case Control::bypass:    return "Hear the track untouched.";
        case Control::theme:     return t.index == 0 ? "A paper window." : "An ink window, for a dark room.";
        case Control::display:   return "Double-click to add a point, up to six.";
        case Control::point:     return "Drag to move. Scroll to change its width. Double-click to remove.";
        case Control::none:      break;
    }
    return {};
}

std::optional<juce::Rectangle<float>> DeSaturatorView::tipAnchor (Target t) const
{
    switch (t.control)
    {
        case Control::character: return L::character.segmentBounds (t.index);
        case Control::quality:   return L::quality.segmentBounds (t.index);
        case Control::theme:     return L::theme.segmentBounds (t.index);
        case Control::delta:     return L::delta;
        case Control::autoGain:  return L::autoGain;
        case Control::bypass:    return circleBox (L::bypass::cx, L::bypass::cy, L::bypass::r + 2.0f);
        case Control::shape:     return L::shape::hit;

        case Control::amount:
        {
            // The ring and its value under it.
            using namespace L::amount;
            return juce::Rectangle<float> (cx - r, cy - r, 2.0f * r, chipY + chipH - (cy - r));
        }

        case Control::restore:
        case Control::mix:
        case Control::output:
        {
            using namespace L::small;
            const float x = cx[t.control == Control::restore ? 0 : t.control == Control::mix ? 1 : 2];
            return juce::Rectangle<float> (x - hit, cy - hit, 2.0f * hit, value + 4.5f - (cy - hit));
        }

        case Control::point:
        {
            if (! juce::isPositiveAndBelow (t.index, curve.count))
                return {};
            const auto& p = curve.points[(size_t) t.index];
            const float x = L::xForFreq (p.freq), y = L::yForValue (p.amount);
            // Near the top the readout sits under the point: the tooltip goes under that.
            return juce::Rectangle<float> (x - 12.0f, y - 12.0f, 24.0f, y < L::plot.getY() + 46.0f ? 53.0f : 24.0f);
        }

        case Control::display: // hangs from the pointer
        case Control::none:    break;
    }
    return {};
}

//==============================================================================
Target DeSaturatorView::targetAt (juce::Point<float> p) const
{
    for (const auto& [sw, control] : { std::pair { &L::character, Control::character }, std::pair { &L::quality, Control::quality } })
        for (int i = 0; i < 2; ++i)
            if (sw->segmentBounds (i).contains (p))
                return { control, i };

    if (L::delta.contains (p))
        return { Control::delta, 0 };
    if (p.getDistanceFrom ({ L::bypass::cx, L::bypass::cy }) <= L::bypass::r + 2.0f)
        return { Control::bypass, 0 };
    if (p.getDistanceFrom ({ L::amount::cx, L::amount::cy }) <= L::amount::hit)
        return { Control::amount, 0 };

    const Control knobs[] = { Control::restore, Control::mix, Control::output };
    for (int i = 0; i < 3; ++i)
        if (p.getDistanceFrom ({ L::small::cx[i], L::small::cy }) <= L::small::hit)
            return { knobs[i], 0 };

    if (L::autoGain.contains (p))
        return { Control::autoGain, 0 };
    if (L::shape::hit.contains (p))
        return { Control::shape, 0 };

    for (int i = 0; i < 2; ++i)
        if (L::theme.segmentBounds (i).contains (p))
            return { Control::theme, i };

    if (L::plot.contains (p))
    {
        // The nearest point within reach, else the analyser itself.
        int best = -1;
        float nearest = L::point::hit;
        for (int i = 0; i < curve.count; ++i)
        {
            const auto& q = curve.points[(size_t) i];
            const float d = p.getDistanceFrom ({ L::xForFreq (q.freq), L::yForValue (q.amount) });
            if (d < nearest)
            {
                nearest = d;
                best = i;
            }
        }
        return best >= 0 ? Target { Control::point, best } : Target { Control::display, 0 };
    }

    return {};
}

//==============================================================================
juce::RangedAudioParameter* DeSaturatorView::parameterFor (Control control) const noexcept
{
    const char* id = parameterId (control);
    return id != nullptr ? processor.apvts.getParameter (id) : nullptr;
}

float DeSaturatorView::plainValue (Control control) const
{
    if (const auto* parameter = parameterFor (control))
        return parameter->convertFrom0to1 (parameter->getValue());
    return 0.0f;
}

void DeSaturatorView::beginGesture (Control control)
{
    endGesture();
    gesture = parameterFor (control);
    if (gesture != nullptr)
        gesture->beginChangeGesture();
}

void DeSaturatorView::endGesture()
{
    if (gesture != nullptr)
        gesture->endChangeGesture();
    gesture = nullptr;
}

void DeSaturatorView::setPlain (Control control, float value)
{
    if (auto* parameter = parameterFor (control))
    {
        const float normalised = parameter->convertTo0to1 (parameter->getNormalisableRange().snapToLegalValue (value));
        if (! juce::approximatelyEqual (normalised, parameter->getValue()))
            parameter->setValueNotifyingHost (normalised);
    }
}

void DeSaturatorView::setInOneGesture (Control control, float value)
{
    // A click, a double-click or a wheel step: a whole gesture of its own, unless one is under way.
    const bool own = gesture == nullptr;
    if (own)
        beginGesture (control);
    setPlain (control, value);
    if (own)
        endGesture();
}

float DeSaturatorView::stepFor (Control control) noexcept
{
    return control == Control::output ? 0.5f : 1.0f;
}

void DeSaturatorView::setShapeFromX (float x)
{
    setPlain (Control::shape, juce::jlimit (0.0f, 100.0f, (x - L::shape::x1) / (L::shape::x2 - L::shape::x1) * 100.0f));
}

void DeSaturatorView::setCurve (const desat::Curve& changed)
{
    processor.setCurve (changed);
    curve = processor.getCurve(); // as the processor keeps it, limits applied
    curveDirty = true;
}

void DeSaturatorView::curveEdited()
{
    // The curve is state, not a parameter: tell the host its saved state is out of date.
    if (std::exchange (curveDirty, false))
        processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
}

/** Brings the drawing up to date straight after something here changed it, without waiting for the timer. */
void DeSaturatorView::changed()
{
    st = readState();
    refreshCurve();
    syncAccessibility();
    repaint();
}

//==============================================================================
void DeSaturatorView::setHover (Target t, juce::Point<float> at)
{
    ui.tipAt = ui.tip ? ui.tipAt : at; // a tooltip that's showing stays where it came up
    if (t == ui.hover)
        return;

    ui.hover = t;
    ui.tip = {};
    ui.tipAt = at;
    wheelRemainder = 0.0f;

    // The tooltip's delay starts when the pointer arrives, and never during a drag.
    tipDue = t && ! ui.drag ? juce::Time::getMillisecondCounter() + (juce::uint32) L::tooltip::delayMs : 0;

    updateCursor();
    repaint();
}

void DeSaturatorView::updateCursor()
{
    const auto over = ui.drag ? ui.drag.control : ui.hover.control;
    using Cursor = juce::MouseCursor;

    switch (over)
    {
        case Control::amount:
        case Control::restore:
        case Control::mix:
        case Control::output:    setMouseCursor (Cursor::UpDownResizeCursor); break;
        case Control::shape:     setMouseCursor (Cursor::LeftRightResizeCursor); break;
        case Control::point:     setMouseCursor (Cursor::DraggingHandCursor); break;
        case Control::display:   setMouseCursor (curve.count < desat::Curve::maxPoints ? Cursor::CrosshairCursor : Cursor::NormalCursor); break;
        case Control::autoGain:
        case Control::delta:
        case Control::bypass:
        case Control::character:
        case Control::quality:
        case Control::theme:     setMouseCursor (Cursor::PointingHandCursor); break;
        case Control::none:      setMouseCursor (Cursor::NormalCursor); break;
    }
}

void DeSaturatorView::showNote (const juce::String& note)
{
    ui.note = note;
    noteUntil = juce::Time::getMillisecondCounter() + (juce::uint32) noteMs;
}

//==============================================================================
void DeSaturatorView::mouseMove (const juce::MouseEvent& e)
{
    if (! preview.has_value())
        setHover (targetAt (e.position), e.position);
}

void DeSaturatorView::mouseExit (const juce::MouseEvent& e)
{
    if (preview.has_value())
        return;

    // The wheel is only ours between a click and the pointer leaving, so a host's scroll isn't stolen.
    engaged = false;
    if (! ui.drag)
        setHover ({}, e.position);
}

void DeSaturatorView::mouseDown (const juce::MouseEvent& e)
{
    if (preview.has_value() || ! e.mods.isLeftButtonDown())
        return;

    engaged = true;
    tipDue = 0;
    ui.tip = {};

    const auto t = targetAt (e.position);

    if (isKnob (t.control))
    {
        ui.drag = t;
        beginGesture (t.control);
        dragValue = plainValue (t.control);
        dragLastY = screenPosition (e).y;
    }
    else if (t.control == Control::shape)
    {
        ui.drag = t;
        beginGesture (t.control);
        setShapeFromX (e.position.x);
    }
    else if (t.control == Control::character || t.control == Control::quality)
    {
        setInOneGesture (t.control, (float) t.index);
    }
    else if (isToggle (t.control))
    {
        setInOneGesture (t.control, plainValue (t.control) >= 0.5f ? 0.0f : 1.0f);
    }
    else if (t.control == Control::theme)
    {
        processor.setDarkTheme (t.index == 1);
    }
    else if (t.control == Control::point)
    {
        ui.selected = t.index;
        ui.drag = t;
    }
    else if (t.control == Control::display)
    {
        ui.selected = -1;
    }

    updateCursor();
    changed();
}

void DeSaturatorView::mouseDrag (const juce::MouseEvent& e)
{
    if (preview.has_value() || ! ui.drag)
        return;

    if (isKnob (ui.drag.control))
    {
        // Measured step by step, so Shift can be pressed or let go mid-drag without a jump.
        const auto range = parameterFor (ui.drag.control)->getNormalisableRange();
        const float y = screenPosition (e).y;
        const float fullRange = e.mods.isShiftDown() ? dragFineRangePx : dragFullRangePx;

        dragValue = juce::jlimit ((double) range.start, (double) range.end,
                                  dragValue + (double) ((dragLastY - y) / fullRange * (range.end - range.start)));
        dragLastY = y;
        setPlain (ui.drag.control, (float) dragValue);
    }
    else if (ui.drag.control == Control::shape)
    {
        setShapeFromX (e.position.x);
    }
    else if (ui.drag.control == Control::point && juce::isPositiveAndBelow (ui.drag.index, curve.count))
    {
        // Frequency follows x, amount follows y.
        auto edited = curve;
        auto& p = edited.points[(size_t) ui.drag.index];
        p.freq = juce::jlimit (20.0, 20000.0, L::freqForX (e.position.x));
        p.amount = juce::jlimit (0.0, 2.0, L::valueForY (e.position.y));
        setCurve (edited);
    }

    changed();
}

void DeSaturatorView::mouseUp (const juce::MouseEvent& e)
{
    if (preview.has_value())
        return;

    endGesture();
    curveEdited();

    if (ui.drag)
    {
        ui.drag = {};
        setHover (targetAt (e.position), e.position);
        updateCursor();
    }

    changed();
}

void DeSaturatorView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // A third quick click isn't another double-click.
    if (preview.has_value() || e.getNumberOfClicks() % 2 != 0)
        return;

    const auto t = targetAt (e.position);

    if (isKnob (t.control) || t.control == Control::shape)
    {
        if (const auto* parameter = parameterFor (t.control))
            setInOneGesture (t.control, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    }
    else if (t.control == Control::point)
    {
        auto edited = curve;
        for (int i = t.index; i + 1 < edited.count; ++i)
            edited.points[(size_t) i] = edited.points[(size_t) i + 1];
        --edited.count;

        ui.selected = -1;
        ui.hover = {};
        ui.tip = {};
        tipDue = 0;
        setCurve (edited);
        curveEdited();
    }
    else if (t.control == Control::display)
    {
        if (curve.count >= desat::Curve::maxPoints)
        {
            showNote (noteFull);
        }
        else
        {
            auto edited = curve;
            edited.points[(size_t) edited.count++] = { juce::jlimit (20.0, 20000.0, L::freqForX (e.position.x)),
                                                       juce::jlimit (0.0, 2.0, L::valueForY (e.position.y)),
                                                       widthDefault };
            setCurve (edited);
            curveEdited();

            ui.selected = curve.count - 1;
            ui.hover = { Control::point, ui.selected };
            ui.tip = {};
            tipDue = 0;
        }
    }
    else
    {
        return;
    }

    updateCursor();
    changed();
}

void DeSaturatorView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto t = engaged && ! preview.has_value() ? targetAt (e.position) : Target {};

    // Not ours (the window hasn't been clicked, or there's nothing under the pointer that scrolls):
    // let it go on to the host.
    if (! (isKnob (t.control) || t.control == Control::point) || juce::approximatelyEqual (wheel.deltaY, 0.0f))
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    // One step per notch. A wheel with detents sends one event a notch; a trackpad sends a stream
    // of small ones, which are added up until they make a notch.
    int steps = 0;
    if (wheel.isSmooth)
    {
        wheelRemainder += wheel.deltaY / wheelNotch;
        steps = (int) wheelRemainder;
        wheelRemainder -= (float) steps;
    }
    else
    {
        steps = wheel.deltaY > 0.0f ? 1 : -1;
    }

    if (steps == 0)
        return;

    if (t.control == Control::point)
    {
        auto edited = curve;
        auto& p = edited.points[(size_t) t.index];
        p.width = juce::jlimit (widthMin, widthMax, std::round ((p.width + steps * widthStep) * 10.0) / 10.0);
        setCurve (edited);
        curveEdited();
    }
    else
    {
        const auto range = parameterFor (t.control)->getNormalisableRange();
        const float step = stepFor (t.control);
        const float stepped = std::round ((plainValue (t.control) + (float) steps * step) / step) * step;
        setInOneGesture (t.control, juce::jlimit (range.start, range.end, stepped));
    }

    ui.tip = {};
    tipDue = 0;
    changed();
}

//==============================================================================
void DeSaturatorView::buildAccessibility()
{
    using Role = juce::AccessibilityRole;
    using Model = AccessibleRegion::Model;

    auto add = [this] (juce::Component& parent, juce::Rectangle<float> area, const juce::String& name, const juce::String& help, Model model)
    {
        auto region = std::make_unique<AccessibleRegion> (name, help, std::move (model));
        region->setBounds (area.getSmallestIntegerContainer() - parent.getPosition());
        parent.addAndMakeVisible (*region);
        regions.push_back (std::move (region));
        return regions.back().get();
    };

    auto slider = [this, &add] (Control control, juce::Rectangle<float> area, const char* name)
    {
        const auto range = parameterFor (control)->getNormalisableRange();
        Model m;
        m.role = Role::slider;
        m.text = [this, control] { return valueText (control); };
        m.number = [this, control] { return (double) plainValue (control); };
        m.setNumber = [this, control] (double v) { setInOneGesture (control, (float) v); changed(); };
        m.minimum = range.start;
        m.maximum = range.end;
        m.interval = stepFor (control);
        byControl[(size_t) control] = add (*this, area, name, tipText ({ control, 0 }), std::move (m));
    };

    auto toggle = [this, &add] (Control control, juce::Rectangle<float> area, const char* name)
    {
        Model m;
        m.role = Role::toggleButton;
        m.text = [this, control] { return juce::String (plainValue (control) >= 0.5f ? "on" : "off"); };
        m.isOn = [this, control] { return plainValue (control) >= 0.5f; };
        m.press = [this, control] { setInOneGesture (control, plainValue (control) >= 0.5f ? 0.0f : 1.0f); changed(); };
        byControl[(size_t) control] = add (*this, area, name, tipText ({ control, 0 }), std::move (m));
    };

    // A switch: a group that reads its choice, holding a radio button for each segment.
    auto radios = [this, &add] (Control control, const L::Switch& sw, const char* name, const char* help,
                                std::function<int()> chosen, std::function<void (int)> choose)
    {
        Model group;
        group.role = Role::group;
        group.text = [sw, chosen] { return juce::String (sw.segments[juce::jlimit (0, 1, chosen())].label); };
        auto* parent = add (*this, sw.bounds(), name, help, std::move (group));
        byControl[(size_t) control] = parent;

        for (int i = 0; i < 2; ++i)
        {
            Model m;
            m.role = Role::radioButton;
            m.isOn = [chosen, i] { return chosen() == i; };
            m.press = [this, choose, i] { choose (i); changed(); };
            add (*parent, sw.segmentBounds (i), sw.segments[i].label, tipText ({ control, i }), std::move (m));
        }
    };

    // In the order of the spec's table, which is also the order a screen reader walks them in.
    slider (Control::amount, circleBox (L::amount::cx, L::amount::cy, L::amount::hit), "Amount");
    slider (Control::shape, L::shape::hit, "Shape");
    slider (Control::restore, circleBox (L::small::cx[0], L::small::cy, L::small::hit), "Restore");
    slider (Control::mix, circleBox (L::small::cx[1], L::small::cy, L::small::hit), "Mix");
    slider (Control::output, circleBox (L::small::cx[2], L::small::cy, L::small::hit), "Output");
    toggle (Control::autoGain, L::autoGain, "Auto gain");
    toggle (Control::delta, L::delta, "Delta");

    radios (Control::character, L::character, "Character", "Clean: transparent, for fixing.",
            [this] { return st.character; }, [this] (int i) { setInOneGesture (Control::character, (float) i); });
    radios (Control::quality, L::quality, "Quality", "Live: no latency. The curve is applied smoothed.",
            [this] { return st.quality; }, [this] (int i) { setInOneGesture (Control::quality, (float) i); });

    toggle (Control::bypass, circleBox (L::bypass::cx, L::bypass::cy, L::bypass::r + 2.0f), "Bypass");

    {
        Model group;
        group.role = Role::group;
        group.text = [this] { return juce::String (curve.count) + (curve.count == 1 ? " point" : " points"); };
        auto* parent = add (*this, L::plot, "Thinning curve", tipText ({ Control::display, 0 }), std::move (group));
        byControl[(size_t) Control::display] = parent;

        // A point moves in two directions; a screen reader's slider has one, so it gets the amount.
        for (int i = 0; i < desat::Curve::maxPoints; ++i)
        {
            Model m;
            m.role = Role::slider;
            m.text = [this, i] { return pointText (i); };
            m.number = [this, i] { return i < curve.count ? curve.points[(size_t) i].amount * 100.0 : 100.0; };
            m.setNumber = [this, i] (double v)
            {
                if (i >= curve.count)
                    return;
                auto edited = curve;
                edited.points[(size_t) i].amount = juce::jlimit (0.0, 2.0, v / 100.0);
                setCurve (edited);
                curveEdited();
                changed();
            };
            m.minimum = 0.0;
            m.maximum = 200.0;
            m.interval = 1.0;
            pointRegions[(size_t) i] = add (*parent, {}, "Curve point " + juce::String (i + 1), tipText ({ Control::point, 0 }), std::move (m));
        }
    }

    radios (Control::theme, L::theme, "Theme", "An ink window, for a dark room.",
            [this] { return st.dark ? 1 : 0; }, [this] (int i) { processor.setDarkTheme (i == 1); });

    told = st;
    syncAccessibility();
}

void DeSaturatorView::syncAccessibility()
{
    // Each point's region follows its point; the ones not in use are hidden from the screen reader.
    for (int i = 0; i < desat::Curve::maxPoints; ++i)
    {
        auto* region = pointRegions[(size_t) i];
        if (region == nullptr)
            continue;

        const bool used = i < curve.count;
        region->setVisible (used);
        if (used)
        {
            const auto& p = curve.points[(size_t) i];
            const auto centre = juce::Point<float> (L::xForFreq (p.freq), L::yForValue (p.amount)) - L::plot.getPosition();
            region->setBounds (circleBox (centre.x, centre.y, L::point::hit).getSmallestIntegerContainer());
        }
    }

    // Say what changed since the last time, so a screen reader reads the new value out.
    auto tell = [this] (Control control, bool differs)
    {
        if (differs && byControl[(size_t) control] != nullptr)
            byControl[(size_t) control]->valueChanged();
    };

    tell (Control::amount, ! juce::approximatelyEqual (st.amount, told.amount));
    tell (Control::shape, ! juce::approximatelyEqual (st.shape, told.shape));
    tell (Control::restore, ! juce::approximatelyEqual (st.restore, told.restore));
    tell (Control::mix, ! juce::approximatelyEqual (st.mix, told.mix));
    tell (Control::output, ! juce::approximatelyEqual (st.output, told.output));
    tell (Control::autoGain, st.autoGain != told.autoGain);
    tell (Control::delta, st.delta != told.delta);
    tell (Control::bypass, st.bypass != told.bypass);
    tell (Control::character, st.character != told.character);
    tell (Control::quality, st.quality != told.quality);
    tell (Control::theme, st.dark != told.dark);

    if (st.curveVersion != told.curveVersion)
    {
        tell (Control::display, true);
        for (int i = 0; i < curve.count; ++i)
            if (pointRegions[(size_t) i] != nullptr)
                pointRegions[(size_t) i]->valueChanged();
    }

    told = st;
}
