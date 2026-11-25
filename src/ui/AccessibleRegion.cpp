#include "AccessibleRegion.h"

namespace desat::ui
{
namespace
{
using Model = AccessibleRegion::Model;

/** The value: the readout text, and for sliders the number behind it. */
class Value : public juce::AccessibilityValueInterface
{
public:
    explicit Value (const Model& m) : model (m) {}

    bool isReadOnly() const override { return model.setNumber == nullptr; }
    double getCurrentValue() const override { return model.number != nullptr ? model.number() : 0.0; }
    juce::String getCurrentValueAsString() const override { return model.text != nullptr ? model.text() : juce::String(); }

    void setValue (double newValue) override
    {
        if (model.setNumber != nullptr)
            model.setNumber (juce::jlimit (model.minimum, model.maximum, newValue));
    }

    void setValueAsString (const juce::String& newValue) override
    {
        // "42%", "-1.5 dB": the number at the front is what's meant.
        if (newValue.containsAnyOf ("0123456789"))
            setValue (newValue.replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-").getDoubleValue());
    }

    AccessibleValueRange getRange() const override
    {
        if (model.number == nullptr)
            return {};
        return { { model.minimum, model.maximum }, model.interval };
    }

private:
    const Model& model;
};

class Handler : public juce::AccessibilityHandler
{
public:
    Handler (AccessibleRegion& region, const Model& m)
        : juce::AccessibilityHandler (region, m.role, actions (m), valueFor (m)), model (m)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = juce::AccessibilityHandler::getCurrentState();
        if (model.isOn != nullptr)
        {
            state = state.withCheckable();
            if (model.isOn())
                state = state.withChecked();
        }
        return state;
    }

private:
    static juce::AccessibilityActions actions (const Model& m)
    {
        juce::AccessibilityActions a;
        if (m.press != nullptr)
        {
            a.addAction (juce::AccessibilityActionType::press, [&m] { m.press(); });
            a.addAction (juce::AccessibilityActionType::toggle, [&m] { m.press(); });
        }
        return a;
    }

    static Interfaces valueFor (const Model& m)
    {
        if (m.text == nullptr)
            return {};
        return { std::make_unique<Value> (m) };
    }

    const Model& model;
};
} // namespace

AccessibleRegion::AccessibleRegion (const juce::String& name, const juce::String& help, Model modelToUse)
    : model (std::move (modelToUse))
{
    setTitle (name);
    setHelpText (help);

    // The window takes no keys and never takes keyboard focus; the mouse goes to the view beneath.
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    setInterceptsMouseClicks (false, false);
}

void AccessibleRegion::valueChanged()
{
    // Costs next to nothing when no accessibility client is listening.
    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

std::unique_ptr<juce::AccessibilityHandler> AccessibleRegion::createAccessibilityHandler()
{
    return std::make_unique<Handler> (*this, model);
}
} // namespace desat::ui
