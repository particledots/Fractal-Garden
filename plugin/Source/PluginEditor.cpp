#include "PluginEditor.h"

namespace
{
    // Opens at 90% of the panel's natural size; can be made 70% to 150% (3:2 shape kept).
    constexpr int kOpenWidth = 1080, kOpenHeight = 720;
    constexpr int kMinWidth  = 840,  kMinHeight  = 560;
    constexpr int kMaxWidth  = 1800, kMaxHeight  = 1200;
}

FractalGardenEditor::FractalGardenEditor (FractalGardenProcessor& p)
    : juce::AudioProcessorEditor (p), owner (p)
{
    plainLook.setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());

    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    if (auto* aspect = getConstrainer())
        aspect->setFixedAspectRatio ((double) kOpenWidth / (double) kOpenHeight);

    const bool plainWasShowing = owner.getValueTreeState().state.getProperty (FgParams::kViewProperty).toString() == "plain";
    showView (plainWasShowing ? View::plain : View::styled);

    setSize (kOpenWidth, kOpenHeight);
}

FractalGardenEditor::~FractalGardenEditor()
{
    styled.reset();
    plain.reset();
}

void FractalGardenEditor::showView (View view)
{
    styled.reset();
    plain.reset();

    if (view == View::styled)
    {
        styled = std::make_unique<FractalGardenStyledPanel> (owner, JucePlugin_VersionString);
        styled->setLookAndFeel (&styledLook);
        styled->onSwitchToPlain = [this] { requestView (View::plain); };
        addAndMakeVisible (*styled);
    }
    else
    {
        plain = std::make_unique<FractalGardenSliderPanel> (owner.getValueTreeState());
        plain->setLookAndFeel (&plainLook);
        plain->onSwitchToStyled = [this] { requestView (View::styled); };
        addAndMakeVisible (*plain);
    }

    owner.getValueTreeState().state.setProperty (FgParams::kViewProperty, view == View::plain ? "plain" : "styled", nullptr);
    layOutPanel();
    repaint();
}

void FractalGardenEditor::requestView (View view)
{
    // Not switched straight away: the button that asked is part of the window being replaced.
    juce::Component::SafePointer<FractalGardenEditor> safe (this);
    juce::MessageManager::callAsync ([safe, view]
    {
        if (safe != nullptr)
            safe->showView (view);
    });
}

const std::vector<juce::String>& FractalGardenEditor::controlledParameterIds() const
{
    return styled != nullptr ? styled->controlledParameterIds() : plain->controlledParameterIds();
}

void FractalGardenEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16181b));
}

void FractalGardenEditor::layOutPanel()
{
    juce::Component* panel = styled != nullptr ? static_cast<juce::Component*> (styled.get()) : static_cast<juce::Component*> (plain.get());
    if (panel == nullptr)
        return;
    const int w = styled != nullptr ? FractalGardenStyledPanel::kWidth  : FractalGardenSliderPanel::kWidth;
    const int h = styled != nullptr ? FractalGardenStyledPanel::kHeight : FractalGardenSliderPanel::kHeight;
    panel->setBounds (0, 0, w, h);
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) w));
}

void FractalGardenEditor::resized()
{
    layOutPanel();
}
