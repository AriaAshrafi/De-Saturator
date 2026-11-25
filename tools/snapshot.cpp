// Renders the window in every state of design/states/states.json to PNG files, at 1x and 2x, to
// compare with the design's pictures (tools/compare_snapshots.py does the comparing), and checks
// the window's pointer behaviour with scripted mouse events.
//
//   desat_snapshot [folder]      render every state (default folder: ./snapshots)
//   desat_snapshot --check       run the interaction checks instead; exits 1 if any fails
//
// The analyser is fed the design's own stand-in signal and stand-in thinning (ported from
// design/build/ui/core.js), so the pictures are comparable. Two things are set as in the design's
// pictures rather than as the plugin has them: the version beside the credit ("1.0.0", a sample
// value there) and the theme, which is previewed without touching the saved preference.
// "redlines" is the default state with measurements drawn over it by the design's build; it isn't rendered.

#include "plugin/PluginProcessor.h"
#include "ui/DeSaturatorView.h"
#include "ui/Layout.h"

#include <cstdio>
#include <vector>

namespace
{
using Control = DeSaturatorView::Control;
using Target = DeSaturatorView::Target;
namespace L = desat::ui::layout;

//==============================================================================
// The stand-in signal and thinning of design/build/ui/core.js, line for line.
namespace standin
{
constexpr int numBins = 192;
constexpr double freqMin = 20.0, freqMax = 20000.0, dbMin = -90.0, dbMax = 6.0, tilt = 4.5;
const double octaves = std::log2 (freqMax / freqMin);

using Bins = std::array<double, numBins>;

double binFreq (int i) { return freqMin * std::pow (2.0, octaves * i / (numBins - 1)); }
double lerp (double a, double b, double t) { return a + (b - a) * t; }

/** mulberry32, as JavaScript computes it. */
struct Random
{
    juce::uint32 a;

    double next()
    {
        a += 0x6D2B79F5u;
        juce::uint32 t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

/** A bass line, a pad, kick, snare and hats at 120 BPM, as levels in dB with the display's ballistics. */
struct Signal
{
    explicit Signal (juce::uint32 seed) : random { seed }
    {
        level.fill (dbMin);
        target.fill (0.0);
        noise.fill (0.0);
    }

    void step (double dt)
    {
        t += dt;

        static const int notes[] = { 0, 5, 3, 7 };
        const int bar = (int) std::floor (t / 2.0) % 4;
        const double f0 = 55.0 * std::pow (2.0, notes[bar] / 12.0);
        const double kick = std::exp (-std::fmod (t, 0.5) / 0.09), hat = std::exp (-std::fmod (t, 0.25) / 0.04);
        const double snare = std::exp (-std::fmod (t + 0.5, 1.0) / 0.12);
        const double swell = 0.5 + 0.5 * std::sin (t * 1.3), r0 = f0 * 4.0 * 1.2599;

        for (int i = 0; i < numBins; ++i)
        {
            const double f = binFreq (i), o = std::log2 (f / 1000.0);
            double p = std::pow (10.0, (-48.0 - 3.0 * o) / 10.0);

            for (int h = 1; h <= 64; ++h)
            {
                const double fh = f0 * h;
                if (fh > 20000.0)
                    break;
                const double d = std::log2 (f / fh) / 0.04;
                if (d > 5.0 || d < -5.0)
                    continue;
                p += std::pow (10.0, (-8.0 - 8.0 * std::log2 ((double) h) - (h % 2 != 0 ? 0.0 : 3.0)) / 10.0) * std::exp (-0.5 * d * d);
            }

            for (int h = 1; h <= 24; ++h)
            {
                const double fh = r0 * h;
                if (fh > 20000.0)
                    break;
                const double d = std::log2 (f / fh) / 0.03;
                if (d > 5.0 || d < -5.0)
                    continue;
                p += std::pow (10.0, (-18.0 - 6.0 * std::log2 ((double) h) - 6.0 * (1.0 - swell)) / 10.0) * std::exp (-0.5 * d * d);
            }

            const double dk = std::log2 (f / 58.0) / 0.55;
            p += std::pow (10.0, -0.4) * kick * std::exp (-0.5 * dk * dk);
            const double ds = std::log2 (f / 2500.0) / 1.6;
            p += std::pow (10.0, -2.2) * snare * std::exp (-0.5 * ds * ds);
            if (f > 3000.0)
            {
                const double dh = std::log2 (f / 11000.0) / 0.8;
                p += std::pow (10.0, -2.4) * hat * std::exp (-0.5 * dh * dh);
            }

            noise[(size_t) i] = noise[(size_t) i] * 0.8 + (random.next() - 0.5) * 1.2;
            target[(size_t) i] = 10.0 * std::log10 (p) + tilt * o + noise[(size_t) i];
        }

        for (int i = 0; i < numBins; ++i)
        {
            const double v = juce::jlimit (dbMin, dbMax,
                                           0.25 * target[(size_t) std::max (0, i - 1)] + 0.5 * target[(size_t) i]
                                               + 0.25 * target[(size_t) std::min (numBins - 1, i + 1)]);
            auto& shown = level[(size_t) i];
            shown = v > shown ? lerp (shown, v, 0.7) : std::max (v, shown - 28.0 * dt);
        }
    }

    double t = 0.0;
    Random random;
    Bins level, target, noise;
};

/** The frame every picture uses: seed 7, 200 steps of a sixtieth of a second. */
Bins snapshot()
{
    Signal s (7);
    for (int i = 0; i < 200; ++i)
        s.step (1.0 / 60.0);
    return s.level;
}

struct Settings
{
    double amount = 30.0, shape = 50.0, restore = 0.0, mix = 100.0;
    bool character = false, bypass = false;
};

/** The stand-in thinning: dB removed per bin, after Mix and before Output and Auto gain. */
Bins reduce (const Settings& st, const Bins& applied, const Bins& original)
{
    Bins out;
    out.fill (0.0);
    if (st.bypass)
        return out;

    const double s = st.shape / 100.0, amount = st.amount / 100.0, most = st.character ? 36.0 : 18.0;
    const double mix = st.mix / 100.0, restore = st.restore / 100.0;

    for (int i = 0; i < numBins; ++i)
    {
        const double f = binFreq (i);
        const double harmonics = 1.0 / (1.0 + std::exp (-std::log2 (f / 1400.0) / 0.5));
        const double b = std::log2 (f / 170.0) / 1.1;
        const double body = std::exp (-0.5 * b * b);

        double sum = 0.0;
        int n = 0;
        for (int j = i - 4; j <= i + 4; ++j)
            if (j >= 0 && j < numBins)
            {
                sum += original[(size_t) j];
                ++n;
            }

        const double peak = juce::jlimit (0.0, 1.0, (original[(size_t) i] - sum / n) / 6.0 + 0.5);
        const double weight = (1.0 - s) * harmonics * (0.45 + 0.55 * peak) + s * body * (0.8 + 0.2 * peak);
        out[(size_t) i] = std::min (amount * applied[(size_t) i] * weight * most * (1.0 - 0.55 * restore * peak) * mix,
                                    original[(size_t) i] - dbMin);
    }
    return out;
}
} // namespace standin

//==============================================================================
struct Point
{
    double f, a, w;
};

/** One state of design/build/states.mjs: the settings, the pointer's part, the size. */
struct StateSpec
{
    juce::String slug;
    double amount = 30.0, shape = 50.0, restore = 0.0, mix = 100.0, output = 0.0;
    bool autoGain = true, delta = false, bypass = false, character = false, hq = true, dark = false, silent = false;
    std::vector<Point> points;
    DeSaturatorView::Preview pointer;
    float scale = 1.0f;
};

std::vector<Point> workPoints() { return { { 140.0, 0.35, 1.4 }, { 3300.0, 1.75, 0.7 }, { 9500.0, 1.35, 0.5 } }; }

StateSpec opening (const char* slug)
{
    StateSpec s;
    s.slug = slug;
    return s;
}

/** "In use": Amount 55%, three points. Most states start from it. */
StateSpec work (const char* slug)
{
    StateSpec s;
    s.slug = slug;
    s.amount = 55.0;
    s.shape = 40.0;
    s.restore = 15.0;
    s.output = -1.5;
    s.points = workPoints();
    return s;
}

std::vector<StateSpec> allStates()
{
    std::vector<StateSpec> states;
    auto add = [&states] (StateSpec s, auto&&... change)
    {
        (change (s), ...);
        states.push_back (std::move (s));
    };

    add (opening ("default"));
    add (work ("in-use"));
    add (opening ("amount-hover"), [] (StateSpec& s) { s.pointer.hover = { Control::amount, 0 }; });
    add (work ("amount-drag"), [] (StateSpec& s) { s.amount = 78.0; s.pointer.hover = s.pointer.drag = { Control::amount, 0 }; });
    add (work ("knob-hover"), [] (StateSpec& s) { s.pointer.hover = { Control::mix, 0 }; });
    add (work ("knob-drag"), [] (StateSpec& s) { s.restore = 42.0; s.pointer.hover = s.pointer.drag = { Control::restore, 0 }; });
    add (work ("shape-drag"), [] (StateSpec& s) { s.shape = 72.0; s.pointer.hover = s.pointer.drag = { Control::shape, 0 }; });
    add (work ("switch-hover"), [] (StateSpec& s) { s.pointer.hover = { Control::character, 1 }; });
    add (work ("point-hover"), [] (StateSpec& s) { s.pointer.hover = { Control::point, 2 }; });
    add (work ("point-drag"), [] (StateSpec& s)
    {
        s.points[1] = { 2400.0, 1.9, 0.7 };
        s.pointer.selectedPoint = 1;
        s.pointer.hover = s.pointer.drag = { Control::point, 1 };
    });
    add (work ("point-selected"), [] (StateSpec& s) { s.pointer.selectedPoint = 1; });
    add (work ("points-full"), [] (StateSpec& s)
    {
        s.points = { { 60.0, 0.2, 1.2 }, { 220.0, 0.6, 1.0 }, { 900.0, 1.25, 1.0 }, { 2600.0, 1.8, 0.8 }, { 6000.0, 1.4, 0.7 }, { 13000.0, 0.9, 0.9 } };
        s.pointer.note = "Six points is the most";
    });
    add (work ("delta-on"), [] (StateSpec& s) { s.delta = true; });
    add (work ("clean"));
    add (work ("character"), [] (StateSpec& s) { s.character = true; });
    add (work ("hq"));
    add (work ("live"), [] (StateSpec& s) { s.hq = false; s.pointer.selectedPoint = 1; });
    add (work ("bypassed"), [] (StateSpec& s) { s.bypass = true; });
    add (work ("silent"), [] (StateSpec& s) { s.silent = true; });
    add (opening ("tooltip"), [] (StateSpec& s) { s.pointer.hover = s.pointer.tip = { Control::delta, 0 }; });
    add (work ("tooltip-point"), [] (StateSpec& s) { s.pointer.hover = s.pointer.tip = { Control::point, 1 }; });
    add (work ("scale-75"), [] (StateSpec& s) { s.scale = 0.75f; });
    add (work ("scale-200"), [] (StateSpec& s) { s.scale = 2.0f; });

    // The same window in the dark theme.
    for (const char* slug : { "default", "in-use", "amount-drag", "point-drag", "delta-on", "live", "bypassed", "tooltip" })
        for (size_t i = 0; i < states.size(); ++i)
            if (states[i].slug == slug)
            {
                auto dark = states[i];
                dark.slug << "-dark";
                dark.dark = true;
                states.push_back (std::move (dark));
                break;
            }

    add (work ("theme-hover"), [] (StateSpec& s) { s.pointer.hover = s.pointer.tip = { Control::theme, 1 }; });
    return states;
}

//==============================================================================
void setParameter (DeSaturatorProcessor& processor, const char* id, float plainValue)
{
    auto* parameter = processor.apvts.getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
}

float parameterValue (DeSaturatorProcessor& processor, const char* id)
{
    const auto* parameter = processor.apvts.getParameter (id);
    return parameter->convertFrom0to1 (parameter->getValue());
}

desat::Curve curveOf (const std::vector<Point>& points)
{
    desat::Curve c;
    for (const auto& p : points)
        c.points[(size_t) c.count++] = { p.f, p.a, p.w };
    return c;
}

/** Puts the processor and the view into a state, with the stand-in signal on the analyser. */
void apply (DeSaturatorProcessor& processor, DeSaturatorView& view, const StateSpec& s, const standin::Bins& signal)
{
    setParameter (processor, ParamIDs::amount, (float) s.amount);
    setParameter (processor, ParamIDs::shape, (float) s.shape);
    setParameter (processor, ParamIDs::restore, (float) s.restore);
    setParameter (processor, ParamIDs::mix, (float) s.mix);
    setParameter (processor, ParamIDs::output, (float) s.output);
    setParameter (processor, ParamIDs::autoGain, s.autoGain ? 1.0f : 0.0f);
    setParameter (processor, ParamIDs::delta, s.delta ? 1.0f : 0.0f);
    setParameter (processor, ParamIDs::bypass, s.bypass ? 1.0f : 0.0f);
    setParameter (processor, ParamIDs::character, s.character ? 1.0f : 0.0f);
    setParameter (processor, ParamIDs::quality, s.hq ? 1.0f : 0.0f);
    processor.setCurve (curveOf (s.points));

    // The curve the stand-in thins by is the processor's applied curve (smoothed in Live), read at the stand-in's bins.
    std::array<float, desat::curveDisplayPoints> effective;
    processor.getEffectiveCurve (effective);

    standin::Bins applied, original;
    for (int i = 0; i < standin::numBins; ++i)
    {
        const double at = (double) i * (desat::curveDisplayPoints - 1) / (standin::numBins - 1);
        const int i0 = juce::jmin (desat::curveDisplayPoints - 2, (int) at);
        applied[(size_t) i] = standin::lerp (effective[(size_t) i0], effective[(size_t) i0 + 1], at - i0);
    }

    if (s.silent)
        original.fill (standin::dbMin);
    else
        original = signal;

    const auto removed = standin::reduce ({ s.amount, s.shape, s.restore, s.mix, s.character, s.bypass }, applied, original);

    // Each comb line reads between the two bins around its own frequency.
    std::array<float, Analyser::numLines> input, result;
    for (int j = 0; j < Analyser::numLines; ++j)
    {
        const double at = ((double) j + 0.5) / Analyser::numLines * (standin::numBins - 1);
        const int i0 = (int) std::floor (at), i1 = std::min (standin::numBins - 1, i0 + 1);
        const double o = standin::lerp (original[(size_t) i0], original[(size_t) i1], at - i0);
        const double r = standin::lerp (removed[(size_t) i0], removed[(size_t) i1], at - i0);
        input[(size_t) j] = (float) o;
        result[(size_t) j] = (float) (o - r);
    }
    processor.getAnalyser().setForDisplay (input, result);

    auto pointer = s.pointer;
    pointer.dark = s.dark;
    pointer.versionText = "1.0.0";
    view.setPreview (pointer);
}

bool save (DeSaturatorView& view, const juce::File& file, float scale)
{
    const auto image = view.createComponentSnapshot (view.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
}

int render (const juce::File& folder)
{
    if (! folder.createDirectory())
    {
        std::printf ("Can't make %s\n", folder.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("Saving to %s\n", folder.getFullPathName().toRawUTF8());

    DeSaturatorProcessor processor;
    DeSaturatorView view (processor);
    const auto signal = standin::snapshot();

    int failed = 0;
    for (const auto& s : allStates())
    {
        apply (processor, view, s, signal);
        const bool ok = save (view, folder.getChildFile (s.slug + ".png"), s.scale)
                        && save (view, folder.getChildFile (s.slug + "@2x.png"), 2.0f * s.scale);
        std::printf ("  %s%s\n", s.slug.toRawUTF8(), ok ? "" : "  FAILED");
        failed += ok ? 0 : 1;
    }
    return failed == 0 ? 0 : 1;
}

//==============================================================================
// The interaction checks: the pointer behaviour of design/spec/SPEC.md, driven as a person would.

/** Counts what the host is told, to check every change sits inside a gesture. */
struct HostLog : juce::AudioProcessorListener
{
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override
    {
        ++changes;
        if (open == 0)
            ++changesOutsideGesture;
    }

    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
    {
        if (details.nonParameterStateChanged)
            ++stateChanges;
    }

    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { ++begins; ++open; }
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override { ++ends; --open; }

    int changes = 0, changesOutsideGesture = 0, begins = 0, ends = 0, open = 0, stateChanges = 0;
};

struct Checker
{
    DeSaturatorProcessor& processor;
    DeSaturatorView& view;
    int failures = 0;

    void expect (bool ok, const char* what)
    {
        std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
        failures += ok ? 0 : 1;
    }

    bool about (double a, double b, double tolerance = 0.011) const { return std::abs (a - b) <= tolerance; }
    float value (const char* id) const { return parameterValue (processor, id); }

    juce::MouseEvent event (float x, float y, bool down, bool shift = false, int clicks = 1) const
    {
        int flags = down ? juce::ModifierKeys::leftButtonModifier : 0;
        if (shift)
            flags |= juce::ModifierKeys::shiftModifier;
        const juce::Point<float> position (x, y);
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys (flags), juce::MouseInputSource::defaultPressure,
                 0.0f, 0.0f, 0.0f, 0.0f, &view, &view, now, position, now, clicks, false };
    }

    void move (float x, float y) { view.mouseMove (event (x, y, false)); }
    void press (float x, float y) { view.mouseDown (event (x, y, true)); }
    void drag (float x, float y, bool shift = false) { view.mouseDrag (event (x, y, true, shift)); }
    void release (float x, float y) { view.mouseUp (event (x, y, false)); }
    void click (float x, float y) { move (x, y); press (x, y); release (x, y); }

    // As JUCE delivers a double-click: down, up, down, up, then the double-click itself.
    void doubleClick (float x, float y)
    {
        click (x, y);
        view.mouseDown (event (x, y, true, false, 2));
        view.mouseUp (event (x, y, false, false, 2));
        view.mouseDoubleClick (event (x, y, false, false, 2));
    }

    void wheel (float x, float y, float deltaY) { view.mouseWheelMove (event (x, y, false), { 0.0f, deltaY, false, false, false }); }
    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
};

int check()
{
    DeSaturatorProcessor processor;
    DeSaturatorView view (processor);
    HostLog host;
    processor.addListener (&host);
    Checker c { processor, view };

    const float ax = L::amount::cx, ay = L::amount::cy;
    const float notch = 0.234375f; // one wheel detent, as JUCE reports it on Windows

    std::printf ("Knobs\n");
    c.move (ax, ay);
    c.wheel (ax, ay, notch);
    c.expect (c.about (c.value (ParamIDs::amount), 30.0), "the wheel does nothing before the window is clicked");

    c.press (ax, ay);
    c.drag (ax, ay - 24.0f);
    c.expect (c.about (c.value (ParamIDs::amount), 40.0), "dragging up 24 px raises Amount by a tenth of its range (240 px for all of it)");
    c.drag (ax, ay - 24.0f - 120.0f, true);
    c.expect (c.about (c.value (ParamIDs::amount), 50.0), "with Shift, 120 px more is another tenth (1200 px for all of it)");
    c.drag (ax, ay + 2000.0f);
    c.expect (c.about (c.value (ParamIDs::amount), 0.0), "a drag stops at the bottom of the range");
    c.release (ax, ay + 2000.0f);
    c.expect (host.begins == 1 && host.ends == 1 && host.changes >= 3, "the whole drag is one gesture");

    c.doubleClick (ax, ay);
    c.expect (c.about (c.value (ParamIDs::amount), 30.0), "double-click resets Amount to 30%");

    c.wheel (ax, ay, notch);
    c.expect (c.about (c.value (ParamIDs::amount), 31.0), "after a click, one notch up is +1%");
    c.wheel (ax, ay, -notch);
    c.wheel (ax, ay, -notch);
    c.expect (c.about (c.value (ParamIDs::amount), 29.0), "and one notch down is -1%");

    const float ox = L::small::cx[2], oy = L::small::cy;
    c.move (ox, oy);
    c.wheel (ox, oy, notch);
    c.expect (c.about (c.value (ParamIDs::output), 0.5), "Output steps by 0.5 dB a notch");
    c.press (ox, oy);
    c.drag (ox, oy - 60.0f);
    c.release (ox, oy - 60.0f);
    c.expect (c.about (c.value (ParamIDs::output), 9.5), "Output drags over its 36 dB in 240 px");
    c.doubleClick (ox, oy);
    c.expect (c.about (c.value (ParamIDs::output), 0.0), "double-click resets Output to 0 dB");

    c.view.mouseExit (c.event (ox, oy, false));
    c.move (ax, ay);
    c.wheel (ax, ay, notch);
    c.expect (c.about (c.value (ParamIDs::amount), 29.0), "once the pointer has left, the wheel waits for a click again");

    std::printf ("Shape\n");
    c.press (230.0f, L::shape::y);
    c.expect (c.about (c.value (ParamIDs::shape), 25.0), "pressing the track brings the thumb to the pointer");
    c.drag (450.0f, L::shape::y + 40.0f);
    c.expect (c.about (c.value (ParamIDs::shape), 75.0), "the thumb follows the pointer across");
    c.drag (900.0f, L::shape::y);
    c.expect (c.about (c.value (ParamIDs::shape), 100.0), "and stops at the end");
    c.release (900.0f, L::shape::y);
    c.doubleClick (340.0f, L::shape::y);
    c.expect (c.about (c.value (ParamIDs::shape), 50.0), "double-click resets Shape to 50%");

    std::printf ("Switches and buttons\n");
    c.click (L::character.segmentBounds (1).getCentreX(), 28.0f);
    c.expect (c.about (c.value (ParamIDs::character), 1.0), "Character is chosen by clicking it");
    c.click (L::character.segmentBounds (0).getCentreX(), 28.0f);
    c.expect (c.about (c.value (ParamIDs::character), 0.0), "and Clean by clicking Clean");
    c.click (L::quality.segmentBounds (0).getCentreX(), 28.0f);
    c.expect (c.about (c.value (ParamIDs::quality), 0.0) && view.getStateLine() == "Live: the applied curve is smoothed", "Live shows its note on the state line");
    c.click (L::delta.getCentreX(), L::delta.getCentreY());
    c.expect (c.value (ParamIDs::delta) > 0.5f && view.getStateLine().startsWith ("Delta"), "Delta turns on and its note comes before Live's");
    c.click (L::bypass::cx, L::bypass::cy);
    c.expect (c.value (ParamIDs::bypass) > 0.5f && view.getStateLine() == "Bypassed", "Bypass turns on and its note comes first");
    c.click (L::bypass::cx, L::bypass::cy);
    c.click (L::delta.getCentreX(), L::delta.getCentreY());
    c.click (L::quality.segmentBounds (1).getCentreX(), 28.0f);
    c.expect (c.value (ParamIDs::bypass) < 0.5f && c.value (ParamIDs::delta) < 0.5f && c.about (c.value (ParamIDs::quality), 1.0) && view.getStateLine().isEmpty(),
              "and all three go back");
    c.click (L::autoGain.getCentreX(), L::autoGain.getCentreY());
    c.expect (c.value (ParamIDs::autoGain) < 0.5f, "Auto gain turns off");
    c.expect (view.targetAt ({ L::theme.segmentBounds (1).getCentreX(), 574.0f }) == Target { Control::theme, 1 },
              "Dark is under the pointer at the bottom right (not clicked here: it would change the saved preference)");
    c.expect (host.changesOutsideGesture == 0 && host.begins == host.ends && host.open == 0, "every parameter change so far was inside a gesture");

    std::printf ("Curve\n");
    const float px = L::xForFreq (1000.0), py = L::yForValue (1.5);
    c.doubleClick (px, py);
    auto curve = processor.getCurve();
    c.expect (curve.count == 1 && c.about (curve.points[0].freq, 1000.0, 1.0) && c.about (curve.points[0].amount, 1.5) && c.about (curve.points[0].width, 1.0),
              "double-click on the empty analyser adds a point there, 1 octave wide");
    c.expect (view.getSelectedPoint() == 0 && host.stateChanges >= 1, "the new point is selected, and the host is told its state changed");

    c.move (px + 5.0f, py + 5.0f);
    c.expect (view.getHover() == Target { Control::point, 0 }, "a point is under the pointer within 13 px");
    c.move (px + 12.0f, py + 12.0f);
    c.expect (view.getHover() == Target { Control::display, 0 }, "and not beyond");

    c.move (px, py);
    c.wheel (px, py, notch);
    c.expect (c.about (processor.getCurve().points[0].width, 1.1, 1.0e-9), "the wheel on a point widens it by 0.1 octave");
    for (int i = 0; i < 40; ++i)
        c.wheel (px, py, -notch);
    c.expect (c.about (processor.getCurve().points[0].width, 0.3, 1.0e-9), "down to 0.3 octave and no further");

    c.press (px, py);
    c.drag (L::xForFreq (200.0), L::yForValue (0.5));
    c.release (L::xForFreq (200.0), L::yForValue (0.5));
    curve = processor.getCurve();
    c.expect (c.about (curve.points[0].freq, 200.0, 0.5) && c.about (curve.points[0].amount, 0.5), "dragging a point: frequency follows x, amount follows y");
    c.expect (view.getSelectedPoint() == 0, "it stays selected after release");
    c.click (400.0f, 300.0f);
    c.expect (view.getSelectedPoint() == -1, "clicking the empty analyser deselects");

    for (int i = 1; i < 6; ++i)
        c.doubleClick (100.0f + 90.0f * (float) i, 150.0f);
    c.expect (processor.getCurve().count == 6, "six points can be added");
    c.doubleClick (630.0f, 380.0f);
    c.expect (processor.getCurve().count == 6 && view.getStateLine() == "Six points is the most", "a seventh isn't: the state line says so");
    c.pump (2300);
    c.expect (view.getStateLine().isEmpty(), "for two seconds");

    c.doubleClick (L::xForFreq (200.0), L::yForValue (0.5));
    c.expect (processor.getCurve().count == 5 && view.getSelectedPoint() == -1, "double-click on a point removes it");

    std::printf ("Tooltips and the host\n");
    c.view.mouseExit (c.event (0.0f, 0.0f, false));
    c.move (L::delta.getCentreX(), L::delta.getCentreY());
    c.pump (400);
    c.expect (view.getTooltipText().isEmpty(), "no tooltip before 600 ms");
    c.pump (400);
    c.expect (view.getTooltipText() == "Hear only what's being removed.", "the tooltip comes up after 600 ms");
    c.press (L::delta.getCentreX(), L::delta.getCentreY());
    c.expect (view.getTooltipText().isEmpty(), "and goes on a press");
    c.release (L::delta.getCentreX(), L::delta.getCentreY());
    c.expect (view.getStateLine().startsWith ("Delta"), "(that press turned Delta on)");

    c.expect (host.changesOutsideGesture == 0 && host.begins == host.ends && host.open == 0,
              "every change from the pointer was inside a gesture, and none is left open");

    setParameter (processor, ParamIDs::bypass, 1.0f); // the host, this time
    c.pump (100);
    c.expect (view.getStateLine() == "Bypassed", "a change from the host shows up in the window by itself");

    processor.setCurve ({});
    c.pump (100);
    c.expect (view.targetAt ({ L::xForFreq (1000.0), 150.0f }) == Target { Control::display, 0 }, "and so does a curve loaded by the host");

    c.expect (! view.getWantsKeyboardFocus() && ! view.getMouseClickGrabsKeyboardFocus(), "the view never takes keyboard focus");
    bool childrenQuiet = true;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            childrenQuiet = childrenQuiet && ! child->getWantsKeyboardFocus() && ! child->getMouseClickGrabsKeyboardFocus();
            walk (*child);
        }
    };
    walk (view);
    c.expect (childrenQuiet && view.getNumChildComponents() > 0, "nor does anything in it");

    processor.removeListener (&host);
    if (c.failures == 0)
        std::printf ("All checks passed\n");
    else
        std::printf ("%d checks FAILED\n", c.failures);
    return c.failures == 0 ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 1 && juce::String (argv[1]) == "--check")
        return check();

    return render (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshots"));
}
