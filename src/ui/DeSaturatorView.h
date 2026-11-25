#pragma once

#include "AccessibleRegion.h"
#include "Layout.h"
#include "Theme.h"
#include "plugin/PluginProcessor.h"

#include <array>
#include <optional>
#include <vector>

/** The whole window, drawn at 960 x 600 from design/spec (the editor scales it as one).

    It is one drawing, ported from the design's own drawing code (design/build/ui/core.js), with its
    own hit areas: the controls are not components. Everything it shows is read from the processor
    on a timer, so a change from the host or from loading state shows up by itself.
    It takes no keys and never takes keyboard focus.
*/
class DeSaturatorView : public juce::Component,
                        private juce::Timer
{
public:
    static constexpr int designWidth = 960;
    static constexpr int designHeight = 600;

    explicit DeSaturatorView (DeSaturatorProcessor&);
    ~DeSaturatorView() override;

    //==============================================================================
    /** Everything the pointer can be over. */
    enum class Control { none, amount, restore, mix, output, shape, autoGain, delta, bypass, character, quality, theme, point, display };

    /** A control, and for switches the segment (0 or 1), for curve points the point. */
    struct Target
    {
        Control control = Control::none;
        int index = 0;

        bool operator== (const Target&) const = default;
        explicit operator bool() const noexcept { return control != Control::none; }
    };

    /** For pictures and tests: the pointer's part of a state, set without a mouse.
        While a preview is set the window is frozen: the analyser isn't run and the pointer is ignored. */
    struct Preview
    {
        Target hover, drag, tip;
        int selectedPoint = -1;
        juce::String note;                  // the passing note on the state line
        juce::Point<float> tipAt;           // where a tooltip on the analyser hangs from
        std::optional<bool> dark;           // the theme, without touching the saved preference
        std::optional<double> latencyMs;
        juce::String versionText;           // empty: the plugin's own
    };

    void setPreview (const Preview&);
    void clearPreview();

    // What's on show, for tests.
    juce::String getStateLine() const;
    juce::String getTooltipText() const;
    int getSelectedPoint() const noexcept { return ui.selected; }
    Target getHover() const noexcept { return ui.hover; }
    /** What is under a point of the window, in design px. */
    Target targetAt (juce::Point<float>) const;

    //==============================================================================
    void paint (juce::Graphics&) override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    /** What the window shows of the processor. Compared on the timer to know when to draw again. */
    struct State
    {
        float amount = 30.0f, shape = 50.0f, restore = 0.0f, mix = 100.0f, output = 0.0f;
        bool autoGain = true, delta = false, bypass = false;
        int character = 0, quality = 1;
        bool dark = false;
        int latencyMs = 0;
        int curveVersion = -1;

        bool operator== (const State&) const = default;
    };

    /** The pointer's part of the state. */
    struct Pointer
    {
        Target hover, drag, tip;
        int selected = -1;
        juce::String note;
        juce::Point<float> tipAt;
    };

    void timerCallback() override;

    State readState() const;
    void refreshCurve();
    bool isOn (Control) const noexcept;
    float hairline (float width) const noexcept;

    // Drawing, in the design's order.
    void paintBackground (juce::Graphics&) const;
    void paintComb (juce::Graphics&) const;
    void paintHeader (juce::Graphics&) const;
    void paintSwitch (juce::Graphics&, const desat::ui::layout::Switch&, Control, int chosen) const;
    void paintPill (juce::Graphics&, juce::Rectangle<float>, Control, const char* label, bool on, juce::Colour onFill, juce::Colour onText, const juce::Font&) const;
    void paintBypass (juce::Graphics&) const;
    void paintLegend (juce::Graphics&) const;
    void paintAmount (juce::Graphics&) const;
    void paintSmallKnob (juce::Graphics&, Control, const char* label, float cx) const;
    void paintShape (juce::Graphics&) const;
    void paintCredit (juce::Graphics&) const;
    void paintOverlay (juce::Graphics&) const;
    void paintTooltip (juce::Graphics&) const;

    // Words.
    juce::String valueText (Control) const;
    juce::String pointText (int index) const;
    juce::String stateLine() const;
    static juce::String tipText (Target);
    std::optional<juce::Rectangle<float>> tipAnchor (Target) const;

    // Parameters. Every change from the pointer is inside a gesture, so hosts can record it.
    juce::RangedAudioParameter* parameterFor (Control) const noexcept;
    float plainValue (Control) const;
    void beginGesture (Control);
    void endGesture();
    void setPlain (Control, float plainValue);
    void setInOneGesture (Control, float plainValue);
    void setShapeFromX (float x);
    static float stepFor (Control) noexcept;

    // The curve.
    void setCurve (const desat::Curve&);
    void curveEdited();
    void changed();

    void setHover (Target, juce::Point<float> at);
    void updateCursor();
    void showNote (const juce::String&);
    void buildAccessibility();
    void syncAccessibility();

    DeSaturatorProcessor& processor;
    juce::SharedResourcePointer<desat::ui::Theme> theme;

    State st;
    Pointer ui;
    desat::Curve curve;
    std::array<float, desat::curveDisplayPoints> applied {}; // Live: the curve as applied
    int appliedVersion = -1, appliedQuality = -1;

    // Set at the start of each paint.
    mutable const desat::ui::Palette* colours = nullptr;
    mutable float pixel = 1.0f; // one physical pixel, in design px

    std::optional<Preview> preview;

    // Pointer bookkeeping.
    bool engaged = false;          // clicked since the pointer came in: the wheel is ours
    juce::RangedAudioParameter* gesture = nullptr;
    double dragValue = 0.0;        // the dragged knob's value before the parameter's own snapping
    float dragLastY = 0.0f;
    float wheelRemainder = 0.0f;
    bool curveDirty = false;
    juce::uint32 tipDue = 0, noteUntil = 0;
    double lastTick = 0.0;
    bool hadSignal = false;

    // Accessibility: one region per control, in the spec's order.
    std::vector<std::unique_ptr<desat::ui::AccessibleRegion>> regions;
    std::array<desat::ui::AccessibleRegion*, desat::Curve::maxPoints> pointRegions {};
    std::array<desat::ui::AccessibleRegion*, 14> byControl {}; // indexed by Control
    State told;                                                 // what a screen reader was last told

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeSaturatorView)
};
