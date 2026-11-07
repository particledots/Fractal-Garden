// The plugin window. It shows either the styled panel (the default, matching the approved pictures) or the plain-slider window.
// Both have one control for every one of the 46 parameters and use the same ids, so saved projects and automation work the same
// whichever is showing. The choice is remembered in the saved project. Each is laid out in a fixed 1200 x 800 space and scaled to
// the window, keeping the 3:2 shape.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PanelLookAndFeel.h"
#include "StyledPanel.h"
#include "SliderPanel.h"

#include <memory>
#include <vector>

class FractalGardenEditor : public juce::AudioProcessorEditor
{
public:
    explicit FractalGardenEditor (FractalGardenProcessor&);
    ~FractalGardenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    enum class View { styled, plain };
    View getView() const { return styled != nullptr ? View::styled : View::plain; }
    void showView (View);

    // For the automatic checks
    const std::vector<juce::String>& controlledParameterIds() const;
    FractalGardenStyledPanel* getStyledPanel() { return styled.get(); }
    FractalGardenSliderPanel* getPlainPanel() { return plain.get(); }

private:
    void requestView (View);        // switches after the current click has finished
    void layOutPanel();

    FractalGardenProcessor& owner;
    juce::LookAndFeel_V4 plainLook;        // declared before the panels, so they outlive them
    PanelLookAndFeel styledLook;
    std::unique_ptr<FractalGardenStyledPanel> styled;
    std::unique_ptr<FractalGardenSliderPanel> plain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalGardenEditor)
};
