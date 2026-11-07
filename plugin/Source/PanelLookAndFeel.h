// The look of the styled panel: colours, fonts, and how knobs, buttons and menus are drawn. Appearance only; nothing here touches
// the controls or the step data. Colours and shapes follow the approved Fractal-Garden mockup (milestone 1, which follows the
// Mimic-Phon panel): charcoal panel, amber for values and selection, teal for the lights, and the tab / Knob Mode colours.
// Fonts are Space Grotesk (labels) and IBM Plex Mono (read-outs), embedded so it looks the same on every Mac.
// (Copied from Mimic-Phon's PanelLookAndFeel and renamed; the Zone colours were replaced by the tab colours.)

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace FgStyle
{
    // Colours from the approved mockup
    inline const juce::Colour background { 0xff16181b };   // charcoal
    inline const juce::Colour card       { 0xff1b1e22 };   // knob cards and boxes
    inline const juce::Colour cardEdge   { 0xff2a2e33 };
    inline const juce::Colour well       { 0xff111316 };   // the delay read-out background
    inline const juce::Colour control    { 0xff1f2328 };   // idle button / menu fill
    inline const juce::Colour controlHot { 0xff282c32 };   // button / menu fill under the mouse
    inline const juce::Colour controlEdge{ 0xff363b41 };
    inline const juce::Colour amber      { 0xffe8a33d };   // values and selection
    inline const juce::Colour teal       { 0xff4fb3a9 };   // tick light, unity mark, Tempo Sync when on
    inline const juce::Colour text       { 0xffe9e6df };
    inline const juce::Colour muted      { 0xffcfccc3 };

    // The tab and Knob Mode colours of the mockup: Note blue, Gate green, Slew gold, Ratchet lilac, Mod white, Performance rose.
    inline const juce::Colour blue   { 0xff4f8ac0 };
    inline const juce::Colour green  { 0xff7cc47f };
    inline const juce::Colour gold   { 0xffecb83a };
    inline const juce::Colour lilac  { 0xffa58dc4 };
    inline const juce::Colour white  { 0xffe9e6df };
    inline const juce::Colour rose   { 0xffc0509b };

    // Mixes colour a towards colour b by t (0..1).
    juce::Colour mixColours (juce::Colour a, juce::Colour b, float t);

    // A knob's arc is amber unless its slider carries this property (a colour stored as a 64-bit number).
    // The Branch / Path / Mutate knobs use it for the colour of the Knob Mode.
    inline constexpr const char* arcColourProperty = "arcColour";

    // The two embedded font families. "sans" is Space Grotesk; "mono" is IBM Plex Mono.
    juce::Font sans (float height, bool bold = false, float extraTracking = 0.0f);
    juce::Font mono (float height, bool medium = false);
}

class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    // Knobs: a dark dial with an amber arc showing the value and a pointer (the arc takes the colour in the slider's
    // "arcColour" property when it has one).
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    // Buttons: flat rounded buttons; amber when on. A button with the property "onColour" (a 64-bit colour) uses that
    // colour when on instead. Optional properties: "fontSize" (a number) sets the label size, "stripe" (a 64-bit colour) draws the
    // small coloured line under the label (the Reseed buttons), "glow" (true) lights it for a moment (the momentary buttons).
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool isMouseOver, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Menus
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};
