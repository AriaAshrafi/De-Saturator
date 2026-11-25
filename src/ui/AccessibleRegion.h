#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace desat::ui
{
/** What a screen reader is told about one control of the window.

    The window is one drawing with its own hit areas, so its controls are not components.
    Each control gets one of these laid over it instead: invisible, never under the mouse, never
    focused by the keyboard, only there to carry the name, role, value and help of
    design/spec/SPEC.md ("Accessibility") and to pass on what an accessibility client asks for.
*/
class AccessibleRegion : public juce::Component
{
public:
    struct Model
    {
        juce::AccessibilityRole role = juce::AccessibilityRole::group;

        /** The value as the window shows it (a slider's readout, "Clean", "3 points"). */
        std::function<juce::String()> text;

        // Sliders: the value as a number, and how to set it.
        std::function<double()> number;
        std::function<void (double)> setNumber;
        double minimum = 0.0, maximum = 1.0, interval = 0.0;

        // Toggle and radio buttons.
        std::function<bool()> isOn;
        std::function<void()> press;
    };

    AccessibleRegion (const juce::String& name, const juce::String& help, Model modelToUse);

    /** Tells a listening screen reader that the value changed. */
    void valueChanged();

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    Model model;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AccessibleRegion)
};
} // namespace desat::ui
