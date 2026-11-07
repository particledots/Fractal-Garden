// The plain-slider window: one ordinary control for every one of the 46 parameters, in the order of the control list. It is the
// fallback to the styled panel and uses the same parameter ids (Parameters.h), so saved projects and automation do not care which
// window is showing. Laid out in the same fixed 1200 x 800 space as the styled panel.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

#include <functional>
#include <memory>
#include <vector>

class FractalGardenSliderPanel : public juce::Component
{
public:
    static constexpr int kWidth  = 1200;
    static constexpr int kHeight = 800;

    explicit FractalGardenSliderPanel (juce::AudioProcessorValueTreeState&);
    ~FractalGardenSliderPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    const std::vector<juce::String>& controlledParameterIds() const { return controlIds; }
    std::function<void()> onSwitchToStyled;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // The attachment is declared after its control, so it is destroyed first.
    struct SliderRow  { juce::Label label; juce::Slider slider; std::unique_ptr<SliderAttachment> attachment; };
    struct ChoiceRow  { juce::Label label; juce::ComboBox box;  std::unique_ptr<ComboBoxAttachment> attachment; };
    struct ButtonItem { juce::TextButton button;                std::unique_ptr<ButtonAttachment> attachment; };
    struct Cell { juce::Component* a = nullptr; juce::Component* b = nullptr; };

    juce::AudioProcessorValueTreeState& state;
    std::vector<juce::String> controlIds;
    std::vector<std::unique_ptr<SliderRow>>  sliders;
    std::vector<std::unique_ptr<ChoiceRow>>  choices;
    std::vector<std::unique_ptr<ButtonItem>> buttons;
    std::vector<Cell> cells;                                    // in control-list order
    juce::TextButton styledPanelButton { "STYLED PANEL" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalGardenSliderPanel)
};
