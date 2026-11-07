#include "PanelLookAndFeel.h"
#include <FgFontData.h>

namespace
{
    juce::Typeface::Ptr load (const void* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    // Made once, the first time a font is asked for.
    struct Faces
    {
        juce::Typeface::Ptr sansRegular = load (FgFontData::SpaceGroteskRegular_ttf, FgFontData::SpaceGroteskRegular_ttfSize);
        juce::Typeface::Ptr sansMedium  = load (FgFontData::SpaceGroteskMedium_ttf,  FgFontData::SpaceGroteskMedium_ttfSize);
        juce::Typeface::Ptr sansBold    = load (FgFontData::SpaceGroteskBold_ttf,    FgFontData::SpaceGroteskBold_ttfSize);
        juce::Typeface::Ptr monoRegular = load (FgFontData::IBMPlexMonoRegular_ttf,  FgFontData::IBMPlexMonoRegular_ttfSize);
        juce::Typeface::Ptr monoMedium  = load (FgFontData::IBMPlexMonoMedium_ttf,   FgFontData::IBMPlexMonoMedium_ttfSize);
    };

    Faces& faces()
    {
        static Faces f;
        return f;
    }

    juce::Font make (juce::Typeface::Ptr face, float height, float tracking)
    {
        // If a font could not be read for any reason, fall back to the system font rather than draw nothing.
        juce::Font f = face != nullptr ? juce::Font (juce::FontOptions (face)) : juce::Font (juce::FontOptions());
        return f.withHeight (height).withExtraKerningFactor (tracking);
    }
}

namespace FgStyle
{
    juce::Font sans (float height, bool bold, float extraTracking)
    {
        return make (bold ? faces().sansBold : faces().sansMedium, height, extraTracking);
    }

    juce::Font mono (float height, bool medium)
    {
        return make (medium ? faces().monoMedium : faces().monoRegular, height, 0.0f);
    }

    juce::Colour mixColours (juce::Colour a, juce::Colour b, float t)
    {
        return a.interpolatedWith (b, t);
    }
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    using namespace FgStyle;

    setColour (juce::ResizableWindow::backgroundColourId, background);

    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::outlineColourId, controlEdge);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::ComboBox::focusedOutlineColourId, amber.withAlpha (0.6f));

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d2024));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::PopupMenu::headerTextColourId, muted);

    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace FgStyle;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float outer = diameter * 0.5f - 1.0f;
    const float arcThickness = outer * 0.13f;
    const float arcRadius = outer - arcThickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool hot = slider.isMouseOverOrDragging() && slider.isEnabled();

    // The arc is amber, unless this knob has asked for another colour (the Knob Mode colour).
    juce::Colour arcColour = amber;
    if (slider.getProperties().contains (arcColourProperty))
        arcColour = juce::Colour ((juce::uint32) (juce::int64) slider.getProperties()[arcColourProperty]);

    // Track (the full sweep), then the coloured arc up to the current value
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2c3036));
    g.strokePath (track, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.002f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (hot ? arcColour.brighter (0.15f) : arcColour);
        g.strokePath (value, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Dial body
    const float bodyRadius = outer * 0.74f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (hot ? 0xff2b3037 : 0xff262a30), centre.x, body.getY(),
                                             juce::Colour (0xff1a1d21), centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff353a41));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // Pointer
    const float r0 = outer * 0.42f, r1 = outer * 0.70f;
    const float s = std::sin (angle), c = std::cos (angle);
    g.setColour (text);
    g.drawLine (centre.x + r0 * s, centre.y - r0 * c, centre.x + r1 * s, centre.y - r1 * c, juce::jmax (2.0f, outer * 0.06f));
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isMouseOver, bool isButtonDown)
{
    using namespace FgStyle;

    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState() || (bool) button.getProperties().getWithDefault ("glow", false);
    const bool usable = button.isEnabled();
    juce::Colour onColour = amber;
    if (button.getProperties().contains ("onColour"))
        onColour = juce::Colour ((juce::uint32) (juce::int64) button.getProperties()["onColour"]);

    juce::Colour fill = control;
    juce::Colour edge = controlEdge;

    if (! usable)
    {
        fill = mixColours (control, background, 0.5f);
        edge = mixColours (controlEdge, background, 0.5f);
    }
    else if (on)
    {
        fill = onColour;
        edge = fill.brighter (0.3f);
    }
    else if (isButtonDown)
        fill = controlHot.brighter (0.15f);
    else if (isMouseOver)
        fill = controlHot;

    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    if (button.getProperties().contains ("stripe"))
    {
        const juce::Colour stripe ((juce::uint32) (juce::int64) button.getProperties()["stripe"]);
        g.setColour (stripe);
        g.fillRoundedRectangle (r.getX() + 7.5f, r.getBottom() - 5.0f, r.getWidth() - 15.0f, 2.5f, 1.2f);
    }
}

juce::Font PanelLookAndFeel::getTextButtonFont (juce::TextButton& b, int)
{
    if (b.getProperties().contains ("fontSize"))
        return FgStyle::sans ((float) (double) b.getProperties()["fontSize"], true, 0.05f);
    return FgStyle::sans (13.5f, true, 0.05f);
}

void PanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    using namespace FgStyle;

    const bool on = button.getToggleState() || (bool) button.getProperties().getWithDefault ("glow", false);
    const bool usable = button.isEnabled();

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (! usable ? muted.withAlpha (0.4f) : (on ? background : text));

    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (6, 2), juce::Justification::centred, 2, 1.0f);
}

void PanelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace FgStyle;

    const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (box.isMouseOver() ? controlHot : control);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? amber.withAlpha (0.6f) : controlEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // Down chevron
    const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.5f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (muted);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font PanelLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return FgStyle::sans (16.5f);
}

void PanelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 32, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font PanelLookAndFeel::getPopupMenuFont()
{
    return FgStyle::sans (16.5f);
}
