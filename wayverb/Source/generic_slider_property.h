#pragma once

#include "generic_property_component.h"
#include "../JuceLibraryCode/JuceHeader.h"

template <typename Model>
class generic_slider_property
        : public generic_property_component<Model, double, Slider> {
public:
    generic_slider_property(
            Model& model,
            const String& name,
            double min,
            double max,
            double inc = 0,
            const String& suffix="")
            : generic_property_component<Model, double, Slider>{
                      model,
                      name,
                      25,
                      Slider::SliderStyle::IncDecButtons,
                      Slider::TextEntryBoxPosition::TextBoxLeft} {
        this->content.setIncDecButtonsMode(
                Slider::IncDecButtonMode::incDecButtonsDraggable_AutoDirection);
        // Make the text box read‑only to avoid Cocoa keyUp re‑entrancy
        // issues while we stabilise text editing; users can still adjust via
        // drag or inc/dec buttons. (Typing caused NSView invalidation before.)
        this->content.setTextBoxStyle(
                Slider::TextEntryBoxPosition::TextBoxLeft, true, 80, 21);
        this->content.setChangeNotificationOnlyOnRelease(true);
        this->content.setRange(min, max, inc);
        this->content.setTextValueSuffix(suffix);
    }

    void sliderValueChanged(Slider* s) override {
        // Defer model updates to avoid re-entrancy while JUCE is still
        // dispatching key events on the text box (prevents NSView lifetime
        // issues when property panels rebuild on notify).
        const double v = s->getValue();
        juce::MessageManager::callAsync([this, v] { this->controller_updated(v); });
    }

protected:
    ~generic_slider_property() noexcept = default;

private:
    void set_view(const double& value) override {
        // Avoid updating the slider while the text box is mid‑edit; defer
        // to the message loop so Cocoa key handling completes first.
        const double v = value;
        juce::MessageManager::callAsync([this, v] {
            this->content.setValue(v, dontSendNotification);
        });
    }
};
