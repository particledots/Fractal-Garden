#include "StyledPanel.h"

#include <fg/Steps.h>

#include <cmath>

using namespace FgStyle;

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    float textWidth (const juce::Font& font, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    enum Anchor { anchorStart, anchorMiddle, anchorEnd };

    // Draws text so that its vertical middle is at y (the pictures' drawing code does the same), starting at, centred on, or ending at x.
    void drawAt (juce::Graphics& g, const juce::String& s, float x, float y, const juce::Font& font, juce::Colour colour, Anchor anchor = anchorStart)
    {
        g.setFont (font);
        g.setColour (colour);
        const float h = font.getHeight() * 1.6f, w = 900.0f;
        switch (anchor)
        {
            case anchorStart:  g.drawText (s, juce::Rectangle<float> (x, y - h * 0.5f, w, h), juce::Justification::centredLeft, false); break;
            case anchorMiddle: g.drawText (s, juce::Rectangle<float> (x - w * 0.5f, y - h * 0.5f, w, h), juce::Justification::centred, false); break;
            case anchorEnd:    g.drawText (s, juce::Rectangle<float> (x - w, y - h * 0.5f, w, h), juce::Justification::centredRight, false); break;
        }
    }

    void fillCard (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto rr = r.reduced (0.5f);
        g.setColour (card);
        g.fillRoundedRectangle (rr, 8.0f);
        g.setColour (cardEdge);
        g.drawRoundedRectangle (rr, 8.0f, 1.0f);
    }

    void smallCard (juce::Graphics& g, int x, int y, int w, const juce::String& title, int h = 64)
    {
        fillCard (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h));
        drawAt (g, title, (float) x + 12.0f, (float) y + 15.0f, sans (12.5f, true, 0.12f), text);
    }

    // One chip (the pictures' "chip"): a small flat button that is lit in its own colour when selected.
    void drawChip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, bool selected, juce::Colour selectedColour,
                   float fontSize, bool dim = false, bool hasDot = false, float radius = 5.0f)
    {
        juce::Colour fill = selected ? selectedColour : control;
        const juce::Colour edge = selected ? mixColours (selectedColour, juce::Colours::white, 0.3f) : controlEdge;
        if (dim && ! selected)
            fill = mixColours (control, background, 0.4f);
        const auto rr = r.reduced (0.5f);
        g.setColour (fill);
        g.fillRoundedRectangle (rr, radius);
        g.setColour (edge);
        g.drawRoundedRectangle (rr, radius, 1.0f);

        const juce::Colour tc = selected ? background : (dim ? mixColours (muted, background, 0.45f) : text);
        if (hasDot && ! selected)
        {
            g.setColour (selectedColour.withAlpha (dim ? 0.45f : 1.0f));
            g.fillEllipse (r.getX() + 12.0f - 3.5f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
            drawAt (g, label, r.getCentreX() + 5.0f, r.getCentreY(), mono (fontSize, true), tc, anchorMiddle);
        }
        else
            drawAt (g, label, r.getCentreX(), r.getCentreY(), mono (fontSize, true), tc, anchorMiddle);
    }

    juce::Point<float> polar (float cx, float cy, float r, float a)
    {
        return { cx + r * std::sin (a), cy - r * std::cos (a) };
    }

    int roundedInt (float v) { return (int) std::lround (v); }

    // The tab colours, in tab order: Note, Gate Length, Slew, Ratchet, Mod.
    juce::Colour tabColour (int tab)
    {
        static const juce::Colour c[5] = { blue, green, gold, lilac, white };
        return c[juce::jlimit (0, 4, tab)];
    }
    const char* const kTabNames[5] = { "NOTE", "GATE LENGTH", "SLEW", "RATCHET", "MOD" };

    juce::Colour modeColour (int mode)
    {
        static const juce::Colour c[3] = { blue, green, rose };
        return c[juce::jlimit (0, 2, mode)];
    }

    // Our own eight Mod shapes (0 sine, 1 triangle, 2 ramp up, 3 ramp down, 4 square, 5 pulse, 6 pluck, 7 stairs), as in the mockup.
    double shapeValue (int shape, double t)
    {
        switch (shape)
        {
            case 0:  return 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * t);
            case 1:  return 1.0 - std::abs (2.0 * t - 1.0);
            case 2:  return t;
            case 3:  return 1.0 - t;
            case 4:  return t < 0.5 ? 1.0 : 0.0;
            case 5:  return t < 0.25 ? 1.0 : 0.0;
            case 6:  return std::exp (-4.0 * t);
            default: return std::min (3.0, std::floor (t * 4.0)) / 3.0;
        }
    }

    void drawShape (juce::Graphics& g, int shape, float x, float y, float w, float h, juce::Colour colour)
    {
        juce::Path p;
        const int n = 80;
        for (int k = 0; k <= n; ++k)
        {
            const double t = (double) k / n;
            const double v = shapeValue (shape, std::min (t, 0.9999));
            const float px = x + (float) t * w, py = y + h - (float) v * h;
            if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    int readInt (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        return roundedInt (s.getRawParameterValue (id)->load());
    }
}

//==============================================================================
// ChipSelector
ChipSelector::ChipSelector (juce::AudioProcessorValueTreeState& apvts, const char* id, const juce::StringArray& chipLabels,
                            float size, int width, int step, std::vector<juce::Colour> chipColours)
    : parameterId (id), labels (chipLabels), fontSize (size), chipWidth (width), pitch (step), colours (std::move (chipColours))
{
    parameter = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
    jassert (parameter != nullptr);
    attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float v)
    {
        selected = juce::jlimit (0, juce::jmax (0, labels.size() - 1), roundedInt (v));
        repaint();
    });
    attachment->sendInitialUpdate();
}

int ChipSelector::indexAt (juce::Point<int> p) const
{
    if (p.y < 0 || p.y >= getHeight() || p.x < 0)
        return -1;
    const int i = p.x / pitch;
    if (i >= labels.size() || p.x - i * pitch >= chipWidth)
        return -1;
    return i;
}

void ChipSelector::select (int index)
{
    if (index >= 0 && index < labels.size() && attachment != nullptr)
        attachment->setValueAsCompleteGesture ((float) index);
}

void ChipSelector::mouseDown (const juce::MouseEvent& e)
{
    select (indexAt (e.getPosition()));
}

void ChipSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        const juce::Colour c = i < (int) colours.size() ? colours[(std::size_t) i] : amber;
        drawChip (g, juce::Rectangle<float> ((float) (i * pitch), 0.0f, (float) chipWidth, (float) getHeight()), labels[i], i == selected, c, fontSize);
    }
}

//==============================================================================
// KnobCard
KnobCard::KnobCard (juce::AudioProcessorValueTreeState& apvts, const char* id, const juce::String& cardTitle, juce::Colour accentColour,
                    std::function<juce::String (double)> readoutFn, std::function<juce::String()> noteFn, std::vector<double> markList)
    : parameterId (id), title (cardTitle), accent (accentColour), readout (std::move (readoutFn)), note (std::move (noteFn)), marks (std::move (markList))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (1.25f * kPi, 2.75f * kPi, true);
    slider.getProperties().set (arcColourProperty, (juce::int64) accent.getARGB());
    slider.onValueChange = [this] { repaint(); };
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
    addAndMakeVisible (slider);
}

void KnobCard::resized()
{
    slider.setBounds (52, 49, 104, 104);
}

void KnobCard::paint (juce::Graphics& g)
{
    fillCard (g, getLocalBounds().toFloat());
    const float w = (float) getWidth();
    drawAt (g, title, w * 0.5f, 21.0f, sans (16.5f, true, 0.12f), text, anchorMiddle);

    const float cx = w * 0.5f, cy = 101.0f;
    for (double m : marks)
    {
        const float a = 1.25f * kPi + (float) m * 1.5f * kPi;
        const auto p0 = polar (cx, cy, 57.0f, a), p1 = polar (cx, cy, 63.0f, a);
        g.setColour (muted);
        g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.8f);
    }

    const juce::String r = readout (slider.getValue());
    float size = 18.0f;
    while (size > 12.0f && textWidth (mono (size), r) > w - 14.0f)
        size -= 0.5f;
    drawAt (g, r, w * 0.5f, 169.0f, mono (size), accent, anchorMiddle);
    drawAt (g, note(), w * 0.5f, 198.0f, sans (13.5f), muted, anchorMiddle);
}

//==============================================================================
// StepEditor
StepEditor::StepEditor (FractalGardenProcessor& p) : proc (p)
{
    setBounds (kX, kY, kW, kH);
}

int StepEditor::length() const   { return readInt (proc.getValueTreeState(), fg::id::length); }
int StepEditor::modMode() const  { return readInt (proc.getValueTreeState(), fg::id::modMode); }
int StepEditor::tab() const      { return proc.getEditorTab(); }
int StepEditor::page() const     { return proc.getEditorPage(); }

void StepEditor::selectTab (int t)   { proc.setEditorTab (t);  refresh(); repaint(); }
void StepEditor::selectPage (int p)  { proc.setEditorPage (p); refresh(); repaint(); }

void StepEditor::toggleStep (int i)
{
    if (i < 0 || i >= fg::kNumSteps) return;
    auto s = proc.getSteps().steps[(std::size_t) i];
    s.on = ! s.on;
    proc.setStep (i, s);
    repaint();
}

void StepEditor::refresh()
{
    const int rev = proc.getStepsRevision(), t = tab(), pg = page(), len = length(), mm = modMode();
    if (rev != lastRevision || t != lastTab || pg != lastPage || len != lastLength || mm != lastModMode)
    {
        lastRevision = rev; lastTab = t; lastPage = pg; lastLength = len; lastModMode = mm;
        repaint();
    }
}

double StepEditor::overviewFraction (const fg::Step& s, int tabIndex) const
{
    switch (tabIndex)
    {
        case 0:  return (s.note - fg::kNoteMin) / (double) (fg::kNoteMax - fg::kNoteMin);
        case 1:  return s.gate / (double) fg::kGateMax;
        case 2:  return s.slew / (double) fg::kSlewMax;
        case 3:  return s.ratchet / (double) fg::kRatchetMax;
        default: return modMode() == 0 ? (s.modShape + 1) / (double) fg::kNumModShapes : s.mod / (double) fg::kModMax;
    }
}

void StepEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::translation ((float) -kX, (float) -kY));    // draw in panel coordinates, like the mockup
    fillCard (g, juce::Rectangle<float> ((float) kX, (float) kY, (float) kW, (float) kH));

    const auto data = proc.getSteps();
    const int tb = tab(), pg = page(), len = length();
    const bool shapesMode = modMode() == 0;
    const juce::Colour tabCol = tabColour (tb);

    // Tabs
    for (int i = 0; i < 5; ++i)
        drawChip (g, { (float) (64 + i * 134), 100.0f, 128.0f, 32.0f }, kTabNames[i], i == tb, tabColour (i), 13.0f, false, true);
    drawAt (g, "MOD OUT", 752.0f, 116.0f, mono (12.0f), muted);

    // Page chips
    drawAt (g, "PAGE", 64.0f, 154.0f, mono (12.0f), muted);
    const int pagesActive = (len + 7) / 8;
    for (int p = 0; p < 8; ++p)
        drawChip (g, { (float) (106 + p * 46), 142.0f, 42.0f, 24.0f }, juce::String (p + 1), p == pg, amber, 13.0f, p >= pagesActive);

    // Overview of all 64 steps
    const float ox = 494.0f, oy = 142.0f, ow = 642.0f, oh = 24.0f, pitch = ow / 64.0f;
    for (int s = 0; s < 64; ++s)
    {
        const bool beyond = s >= len;
        const auto& st = data.steps[(std::size_t) s];
        const float x = ox + (float) s * pitch;
        if (beyond)
        {
            g.setColour (muted.withAlpha (0.25f));
            g.fillRoundedRectangle (x + 1.0f, oy + oh - 3.0f, pitch - 2.2f, 2.5f, 1.0f);
        }
        else
        {
            const float hh = juce::jmax (2.0f, (float) overviewFraction (st, tb) * (oh - 4.0f));
            const juce::Colour col = s == playhead ? teal : tabCol;
            g.setColour (col.withAlpha (st.on ? 1.0f : 0.35f));
            g.fillRoundedRectangle (x + 1.0f, oy + oh - 2.0f - hh, pitch - 2.2f, hh, 1.5f);
        }
    }
    g.setColour (amber);
    g.drawRoundedRectangle (ox + (float) pg * 8.0f * pitch - 1.0f + 0.5f, oy - 2.0f + 0.5f, 8.0f * pitch + 2.0f - 1.0f, oh + 4.0f - 1.0f, 4.0f, 1.6f);

    // The eight step columns
    for (int c = 0; c < 8; ++c)
    {
        const int s = pg * 8 + c;
        const float x = (float) (64 + c * 134);
        const bool beyond = s >= len, playing = s == playhead;
        const auto& st = data.steps[(std::size_t) s];
        const bool on = st.on;
        const float dimf = beyond ? 0.35f : 1.0f;

        drawAt (g, "STEP", x + 4.0f, 185.0f, mono (12.0f), muted.withAlpha (dimf));
        drawAt (g, juce::String (s + 1), x + 44.0f, 185.0f, mono (18.0f, true), (playing ? teal : text).withAlpha (dimf));

        const juce::Rectangle<float> well (x, 198.0f, 128.0f, 110.0f);
        g.setColour (FgStyle::well.withAlpha (beyond ? 0.55f : 1.0f));
        g.fillRoundedRectangle (well.reduced (0.5f), 6.0f);
        g.setColour ((playing ? teal : cardEdge).withAlpha (beyond ? 0.55f : 1.0f));
        g.drawRoundedRectangle (well.reduced (0.5f), 6.0f, playing ? 1.6f : 1.0f);
        if (tb == 0)
        {
            g.setColour (cardEdge.withAlpha (dimf));
            for (int k = 1; k < 5; ++k)
                g.drawLine (x + 6.0f, 198.0f + 110.0f * (float) k / 5.0f, x + 122.0f, 198.0f + 110.0f * (float) k / 5.0f, 1.0f);
        }

        juce::String readout = "-";
        if (! beyond)
        {
            if (tb == 4 && shapesMode)
            {
                const juce::Colour col = on ? tabCol : mixColours (tabCol, background, 0.75f);
                drawShape (g, st.modShape, x + 16.0f, 216.0f, 96.0f, 58.0f, col);
                drawAt (g, on ? fg::kModRateCaptions[st.modRate] : "0 V", x + 64.0f, 294.0f, mono (11.5f),
                        on ? muted : mixColours (muted, background, 0.5f), anchorMiddle);
                readout = fg::kModShapeNames[st.modShape];
            }
            else
            {
                const double frac = overviewFraction (st, tb);
                const float bh = juce::jmax (6.0f, (float) frac * (110.0f - 8.0f));
                const float by = 198.0f + 110.0f - 4.0f - bh;
                const juce::Colour col = on ? tabCol : mixColours (tabCol, background, 0.7f);
                g.setColour (col.withAlpha (on ? 0.35f : 0.5f));
                g.fillRoundedRectangle (x + 22.0f, by, 128.0f - 44.0f, bh, 3.0f);
                g.setColour (col);
                g.fillRoundedRectangle (x + 22.0f, by, 128.0f - 44.0f, 4.0f, 2.0f);
                char nm[16];
                switch (tb)
                {
                    case 0:  fg::noteName (st.note, nm, sizeof nm); readout = nm; break;
                    case 1:  readout = juce::String (st.gate) + " %"; break;
                    case 2:  readout = juce::String (st.slew) + " %"; break;
                    case 3:  readout = "x" + juce::String (st.ratchet); break;
                    default: readout = juce::String (st.mod) + " %"; break;
                }
            }
        }

        if (beyond)
            drawAt (g, "-", x + 64.0f, 323.0f, mono (18.0f), muted.withAlpha (0.35f), anchorMiddle);
        else
            drawAt (g, readout, x + 64.0f, 323.0f, mono (readout.length() < 9 ? 18.0f : 15.0f),
                    on ? amber : mixColours (amber, background, 0.6f), anchorMiddle);

        // ON / OFF button
        const juce::Rectangle<float> b (x, 340.0f, 128.0f, 28.0f);
        if (beyond)
        {
            g.setColour (mixColours (control, background, 0.4f));
            g.fillRoundedRectangle (b.reduced (0.5f), 6.0f);
            g.setColour (mixColours (controlEdge, background, 0.5f));
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            drawAt (g, "BEYOND LENGTH", x + 64.0f, 354.0f, mono (11.0f), muted.withAlpha (0.45f), anchorMiddle);
        }
        else
        {
            g.setColour (on ? amber : control);
            g.fillRoundedRectangle (b.reduced (0.5f), 6.0f);
            g.setColour (on ? mixColours (amber, juce::Colours::white, 0.3f) : controlEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
            drawAt (g, on ? "ON" : "OFF", x + 64.0f, 354.0f, sans (13.5f, true, 0.05f), on ? background : text, anchorMiddle);
        }
    }
}

void StepEditor::dragValue (int stepIndex, int tabIndex, float mouseY)
{
    if (stepIndex < 0 || stepIndex >= fg::kNumSteps || stepIndex >= length())
        return;
    const float f = 1.0f - juce::jlimit (0.0f, 1.0f, (mouseY - 198.0f - 4.0f) / (110.0f - 8.0f));
    auto s = proc.getSteps().steps[(std::size_t) stepIndex];
    switch (tabIndex)
    {
        case 0:  s.note = (std::uint8_t) (fg::kNoteMin + roundedInt (f * (float) (fg::kNoteMax - fg::kNoteMin))); break;
        case 1:  s.gate = (std::uint8_t) roundedInt (f * (float) fg::kGateMax); break;
        case 2:  s.slew = (std::uint8_t) roundedInt (f * (float) fg::kSlewMax); break;
        case 3:  s.ratchet = (std::uint8_t) (1 + roundedInt (f * (float) (fg::kRatchetMax - 1))); break;
        default: s.mod = (std::uint8_t) roundedInt (f * (float) fg::kModMax); break;
    }
    proc.setStep (stepIndex, s);
    repaint();
}

void StepEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition() + juce::Point<int> (kX, kY);        // panel coordinates
    dragStep = -1;

    for (int i = 0; i < 5; ++i)
        if (juce::Rectangle<int> (64 + i * 134, 100, 128, 32).contains (p)) { selectTab (i); return; }
    for (int pg = 0; pg < 8; ++pg)
        if (juce::Rectangle<int> (106 + pg * 46, 142, 42, 24).contains (p)) { selectPage (pg); return; }
    if (juce::Rectangle<int> (494, 142, 642, 24).contains (p))
    {
        selectPage (juce::jlimit (0, 7, (int) ((float) (p.x - 494) / (642.0f / 64.0f) / 8.0f)));
        return;
    }

    for (int c = 0; c < 8; ++c)
    {
        const int s = page() * 8 + c, x = 64 + c * 134;
        if (s >= length())
            continue;
        if (juce::Rectangle<int> (x, 340, 128, 28).contains (p)) { toggleStep (s); return; }
        if (juce::Rectangle<int> (x, 198, 128, 110).contains (p))
        {
            if (tab() == 4 && modMode() == 0)             // Mod, Shapes: a click goes to the next shape
            {
                auto st = proc.getSteps().steps[(std::size_t) s];
                st.modShape = (std::uint8_t) ((st.modShape + 1) % fg::kNumModShapes);
                proc.setStep (s, st);
                repaint();
            }
            else
            {
                dragStep = s;
                dragValue (s, tab(), (float) p.y);
            }
            return;
        }
    }
}

void StepEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep >= 0)
        dragValue (dragStep, tab(), (float) (e.getPosition().y + kY));
}

//==============================================================================
// ChanceMixPanel
ChanceMixPanel::ChanceMixPanel (juce::AudioProcessorValueTreeState& apvts)
{
    static const char* const paramIds[8] = { fg::id::chanceForward, fg::id::chanceBackward, fg::id::chancePendulum, fg::id::chancePingPong,
                                             fg::id::chanceRandom, fg::id::chanceDrunk, fg::id::chanceRunMin, fg::id::chanceRunMax };
    static const char* const names[8] = { "FWD", "BACK", "PEND", "PING-PONG", "RANDOM", "DRUNK", "SHORTEST", "LONGEST" };
    for (int i = 0; i < 8; ++i)
    {
        auto d = std::make_unique<Dial>();
        d->name = names[i];
        d->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        d->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        d->slider.setRotaryParameters (1.25f * kPi, 2.75f * kPi, true);
        d->slider.onValueChange = [this] { repaint(); };
        d->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramIds[i], d->slider);
        addAndMakeVisible (d->slider);
        ids.push_back (paramIds[i]);
        dials.push_back (std::move (d));
    }
    done.getProperties().set ("fontSize", 12.0);
    done.onClick = [this] { if (onDone) onDone(); };
    addAndMakeVisible (done);
}

void ChanceMixPanel::resized()
{
    const float cw = (float) (kWidth - 32) / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        const int cx = (int) std::lround (16.0f + cw * (float) i + cw * 0.5f);
        dials[(std::size_t) i]->slider.setBounds (cx - 27, 70 - 27, 54, 54);
    }
    done.setBounds (kWidth - 76, 8, 64, 24);
}

void ChanceMixPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (juce::Colour (0xff1d2024));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (amber);
    g.drawRoundedRectangle (r, 8.0f, 1.5f);

    drawAt (g, "CHANCE MIX", 16.0f, 18.0f, sans (14.5f, true, 0.12f), text);
    drawAt (g, "weights: how often each order is picked  |  run: steps before it switches", (float) kWidth - 92.0f, 18.0f, sans (11.5f), muted, anchorEnd);

    const float cw = (float) (kWidth - 32) / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        const float cx = 16.0f + cw * (float) i + cw * 0.5f;
        drawAt (g, dials[(std::size_t) i]->name, cx, 108.0f, sans (11.5f, true, 0.05f), text, anchorMiddle);
        drawAt (g, juce::String (roundedInt ((float) dials[(std::size_t) i]->slider.getValue())), cx, 128.0f, mono (15.0f), amber, anchorMiddle);
    }
    g.setColour (cardEdge);
    g.drawLine (16.0f + cw * 6.0f, 44.0f, 16.0f + cw * 6.0f, 136.0f, 1.5f);
}

//==============================================================================
// The panel
FractalGardenStyledPanel::FractalGardenStyledPanel (FractalGardenProcessor& p, const juce::String& version)
    : proc (p), state (p.getValueTreeState()), versionText (version)
{
    setSize (kWidth, kHeight);

    // Header
    plainSliders.getProperties().set ("fontSize", 13.5);
    plainSliders.onClick = [this] { if (onSwitchToPlain) onSwitchToPlain(); };
    plainSliders.setBounds (960, 30, 192, 35);
    addAndMakeVisible (plainSliders);

    // Step editor and the Mod Out chips (on top of its card)
    stepEditor = std::make_unique<StepEditor> (proc);
    addAndMakeVisible (*stepEditor);
    modOutChips = std::make_unique<ChipSelector> (state, fg::id::modMode, juce::StringArray { "SHAPES", "VELOCITY", "ENVELOPE", "SMOOTH" }, 11.0f, 76, 80);
    modOutChips->setBounds (820, 100, 316, 32);
    addAndMakeVisible (*modOutChips);
    controlIds.push_back (fg::id::modMode);

    // The nine Branch / Path / Mutate knobs: three shown at a time
    auto branchCount = [this] { return juce::jlimit (0, 7, roundedInt (floatParam (fg::id::branch) * 7.0f)); };
    auto percent = [] (double v) { return juce::String (roundedInt ((float) (v * 100.0))); };

    auto add = [this] (const char* id, const juce::String& title, juce::Colour accent, std::function<juce::String (double)> readout,
                       std::function<juce::String()> note, std::vector<double> marks, int slot)
    {
        auto card = std::make_unique<KnobCard> (state, id, title, accent, std::move (readout), std::move (note), std::move (marks));
        card->setBounds (48 + slot * 220, 392, KnobCard::kWidth, KnobCard::kHeight);
        addChildComponent (*card);
        controlIds.push_back (id);
        knobCards.push_back (std::move (card));
    };

    // Default mode (blue)
    add (fg::id::branch, "BRANCH", blue,
         [] (double v) { const int n = juce::jlimit (0, 7, roundedInt ((float) v * 7.0f)); return juce::String (n) + (n == 1 ? " branch" : " branches"); },
         [] { return juce::String ("0 ... 7 BRANCHES"); }, {}, 0);
    add (fg::id::path, "PATH", blue,
         [branchCount] (double v)
         {
             const int n = branchCount(), paths = 1 << n;
             return "path " + juce::String (roundedInt ((float) v * (float) (paths - 1)) + 1) + " / " + juce::String (paths);
         },
         [branchCount]
         {
             const int n = branchCount();
             if (n == 0) return juce::String ("NO BRANCHES YET");
             if (n > 5) return "2 ^ " + juce::String (n) + " = " + juce::String (1 << n) + " ROUTES";
             juce::String s;
             for (int i = 0; i < n; ++i) s << (i > 0 ? " x 2" : "2");
             return s + " ROUTES";
         }, {}, 1);
    add (fg::id::mutate, "MUTATE", blue,
         [] (double v)
         {
             return v < 0.5 ? "note " + juce::String (roundedInt ((float) (v / 0.5 * 100.0))) + " %"
                            : "note+gate " + juce::String (roundedInt ((float) ((v - 0.5) / 0.5 * 100.0))) + " %";
         },
         [] { return juce::String ("NOTE  |  + GATE"); }, { 0.5 }, 2);

    // Micro Mutate mode (green): the probability of changing each aspect of the steps for good
    add (fg::id::microRatchet, "MICRO RATCHET", green,
         [percent] (double v) { return v <= 0.0 ? juce::String ("off") : "ratchet " + percent (v) + " %"; },
         [] { return juce::String ("CHANGES RATCHETS"); }, {}, 0);
    add (fg::id::microSlew, "MICRO SLEW", green,
         [percent] (double v) { return v <= 0.0 ? juce::String ("off") : "slew " + percent (v) + " %"; },
         [] { return juce::String ("CHANGES SLEW"); }, {}, 1);
    add (fg::id::microMod, "MICRO MOD", green,
         [percent] (double v) { return v <= 0.0 ? juce::String ("off") : "mod " + percent (v) + " %"; },
         [] { return juce::String ("CHANGES MOD"); }, {}, 2);

    // Performance mode (rose): temporary effects; zone 1 is random, zone 2 is fixed
    add (fg::id::perfRatchet, "RATCHET FX", rose,
         [] (double v)
         {
             return v < 0.5 ? "chance " + juce::String (roundedInt ((float) (v / 0.5 * 100.0))) + " %"
                            : "fixed x" + juce::String (2 + roundedInt ((float) ((v - 0.5) / 0.5 * 6.0)));
         },
         [] { return juce::String ("RANDOM | FIXED x2 ..."); }, { 0.5 }, 0);
    add (fg::id::perfSlew, "SLEW FX", rose,
         [] (double v)
         {
             return v < 0.5 ? "chance " + juce::String (roundedInt ((float) (v / 0.5 * 100.0))) + " %"
                            : "fixed " + juce::String (roundedInt ((float) ((v - 0.5) / 0.5 * 100.0))) + " %";
         },
         [] { return juce::String ("RANDOM | FIXED SLIDE"); }, { 0.5 }, 1);
    add (fg::id::perfMutate, "MUTATE FX", rose,
         [] (double v)
         {
             if (v < 0.5)  return "mutate " + juce::String (roundedInt ((float) (v / 0.5 * 100.0))) + " %";
             if (v < 0.67) return juce::String ("2-step ornaments");
             if (v < 0.84) return juce::String ("4-step ornaments");
             return juce::String ("max + trills");
         },
         [] { return juce::String ("MUTATE | ORNAMENTS | TRILLS"); }, { 0.5, 0.67, 0.84 }, 2);

    knobModeChips = std::make_unique<ChipSelector> (state, fg::id::knobMode, juce::StringArray { "DEFAULT", "MICRO MUTATE", "PERFORMANCE" },
                                                    12.5f, 132, 138, std::vector<juce::Colour> { blue, green, rose });
    knobModeChips->setBounds (726, 420, 408, 28);
    addAndMakeVisible (*knobModeChips);
    controlIds.push_back (fg::id::knobMode);

    // Reseed
    addMomentary (fg::id::reseedAll, "RESEED ALL", 13.0f, { 726, 488, 112, 28 });
    reseedScopeChips = std::make_unique<ChipSelector> (state, fg::id::reseedScope, juce::StringArray { "PAGE", "ALL 64" }, 12.0f, 61, 64);
    reseedScopeChips->setBounds (1008, 488, 126, 28);
    addAndMakeVisible (*reseedScopeChips);
    controlIds.push_back (fg::id::reseedScope);
    addMomentary (fg::id::reseedBranches, "BRANCHES", 11.5f, { 726, 522, 78, 24 });
    addMomentary (fg::id::reseedNote,     "NOTE",     11.5f, { 810, 522, 58, 24 }, blue);
    addMomentary (fg::id::reseedGate,     "GATE",     11.5f, { 874, 522, 58, 24 }, green);
    addMomentary (fg::id::reseedSlew,     "SLEW",     11.5f, { 938, 522, 58, 24 }, gold);
    addMomentary (fg::id::reseedRatchet,  "RATCHET",  11.5f, { 1002, 522, 70, 24 }, lilac);
    addMomentary (fg::id::reseedMod,      "MOD",      11.5f, { 1078, 522, 52, 24 }, white);

    // Row 1: Length, Scale, Key, Order (with its slot), Rate, Ratio
    lengthDown.getProperties().set ("fontSize", 14.0);
    lengthUp.getProperties().set ("fontSize", 14.0);
    lengthDown.onClick = [this] { stepLength (-1); };
    lengthUp.onClick   = [this] { stepLength (+1); };
    lengthDown.setBounds (58, 644, 28, 30);
    lengthUp.setBounds (130, 644, 28, 30);
    addAndMakeVisible (lengthDown);
    addAndMakeVisible (lengthUp);
    controlIds.push_back (fg::id::length);

    addMenu (fg::id::scale,    { 188, 644, 266, 30 });
    addMenu (fg::id::key,      { 486, 644, 60, 30 });
    addMenu (fg::id::order,    { 578, 644, 190, 30 });
    addMenu (fg::id::stepRate, { 896, 644, 126, 30 });
    addMenu (fg::id::ratio,    { 1054, 644, 86, 30 });

    repeatEnds.getProperties().set ("fontSize", 11.0);
    repeatEnds.setClickingTogglesState (true);
    repeatEnds.setBounds (776, 644, 88, 30);
    repeatEndsAttachment = std::make_unique<ButtonAttachment> (state, fg::id::pendRepeat, repeatEnds);
    addAndMakeVisible (repeatEnds);
    controlIds.push_back (fg::id::pendRepeat);

    chanceButton.getProperties().set ("fontSize", 12.0);
    chanceButton.setBounds (776, 644, 88, 30);
    chanceButton.onClick = [this] { openChance (! chancePanel->isVisible()); };
    addChildComponent (chanceButton);

    // Row 2: Pattern, Root, Transpose, Rotate, Resize, Reset, Unmutate
    addMenu (fg::id::patternSlot, { 60, 716, 126, 30 });
    addMomentary (fg::id::patternLoad, "LOAD", 12.5f, { 194, 716, 60, 30 });
    addMomentary (fg::id::patternSave, "SAVE", 12.5f, { 260, 716, 60, 30 });

    rootDial.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    rootDial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    rootDial.setRotaryParameters (1.25f * kPi, 2.75f * kPi, true);
    rootDial.onValueChange = [this] { repaint(); };
    rootDial.setBounds (350, 696, 52, 52);
    rootAttachment = std::make_unique<SliderAttachment> (state, fg::id::root, rootDial);
    addAndMakeVisible (rootDial);
    controlIds.push_back (fg::id::root);

    addMomentary (fg::id::transposeDown, "DOWN",   11.5f, { 542, 716, 50, 30 });
    addMomentary (fg::id::transposeUp,   "UP",     11.5f, { 598, 716, 50, 30 });
    addMomentary (fg::id::rotateLeft,    "LEFT",   11.5f, { 680, 716, 50, 30 });
    addMomentary (fg::id::rotateRight,   "RIGHT",  11.5f, { 736, 716, 50, 30 });
    addMomentary (fg::id::resizeShrink,  "SHRINK", 11.5f, { 818, 716, 60, 30 });
    addMomentary (fg::id::resizeExpand,  "EXPAND", 11.5f, { 884, 716, 60, 30 });
    addMomentary (fg::id::reset,         "TO 1",   12.0f, { 974, 716, 60, 30 });
    addMomentary (fg::id::unmutate,      "UNDO",   12.0f, { 1062, 716, 80, 30 });

    // The Chance mix pop-over (on top of everything; hidden until CHANCE... is pressed)
    chancePanel = std::make_unique<ChanceMixPanel> (state);
    chancePanel->setBounds (420, 458, ChanceMixPanel::kWidth, ChanceMixPanel::kHeight);
    chancePanel->onDone = [this] { openChance (false); };
    addChildComponent (*chancePanel);
    for (const auto& id : chancePanel->controlledParameterIds())
        controlIds.push_back (id);

    const auto view = proc.getPlayerView();
    lastStepCount = view.stepCount;
    lastNoteCount = view.noteCount;
    syncState();
    startTimerHz (30);
}

FractalGardenStyledPanel::~FractalGardenStyledPanel()
{
    stopTimer();
}

int FractalGardenStyledPanel::intParam (const char* id) const    { return roundedInt (state.getRawParameterValue (id)->load()); }
float FractalGardenStyledPanel::floatParam (const char* id) const { return state.getRawParameterValue (id)->load(); }

void FractalGardenStyledPanel::addMomentary (const char* id, const juce::String& label, float fontSize, juce::Rectangle<int> bounds, juce::Colour stripe)
{
    auto m = std::make_unique<MomentaryButton>();
    m->button.setButtonText (label);
    m->button.getProperties().set ("fontSize", (double) fontSize);
    if (stripe != juce::Colour())
        m->button.getProperties().set ("stripe", (juce::int64) stripe.getARGB());
    m->button.setClickingTogglesState (true);
    m->button.setBounds (bounds);
    auto* raw = m.get();
    m->button.onClick = [raw] { raw->glow = 10; raw->button.getProperties().set ("glow", true); raw->button.repaint(); };
    m->attachment = std::make_unique<ButtonAttachment> (state, id, m->button);
    addAndMakeVisible (m->button);
    controlIds.push_back (id);
    momentary.push_back (std::move (m));
}

void FractalGardenStyledPanel::addMenu (const char* id, juce::Rectangle<int> bounds)
{
    const auto* spec = fg::findParam (id);
    jassert (spec != nullptr && spec->kind == fg::Kind::Choice);
    auto m = std::make_unique<Menu>();
    juce::StringArray items;
    for (int i = 0; i < spec->numChoices; ++i)
        items.add (spec->choices[i]);
    m->box.addItemList (items, 1);
    m->box.setColour (juce::ComboBox::textColourId, amber);
    m->box.setBounds (bounds);
    m->attachment = std::make_unique<ComboBoxAttachment> (state, id, m->box);
    addAndMakeVisible (m->box);
    controlIds.push_back (id);
    menus.push_back (std::move (m));
}

void FractalGardenStyledPanel::stepLength (int delta)
{
    if (auto* p = state.getParameter (fg::id::length))
    {
        const int now = roundedInt (p->convertFrom0to1 (p->getValue()));
        const int next = juce::jlimit (1, fg::kNumSteps, now + delta);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
        p->endChangeGesture();
    }
}

void FractalGardenStyledPanel::openChance (bool open)
{
    chancePanel->setVisible (open);
    if (open)
        chancePanel->toFront (false);
    chanceButton.setToggleState (open, juce::dontSendNotification);
    repaint();
}

juce::String FractalGardenStyledPanel::rootText() const
{
    const int v = intParam (fg::id::root);
    return (v >= 0 ? "+" : "-") + juce::String (std::abs (v)) + (std::abs (v) == 1 ? " step" : " steps");
}

juce::String FractalGardenStyledPanel::headerLine1() const
{
    if (previewHeader)
        return previewLine1;
    const auto h = proc.getHostSnapshot();
    if (! h.known)
        return "LOGIC: NO TRANSPORT YET";
    return juce::String (h.playing ? "LOGIC PLAYING" : "LOGIC STOPPED") + (h.tempoKnown ? " " + juce::String (h.bpm, 1) + " BPM" : juce::String());
}

juce::String FractalGardenStyledPanel::headerLine2() const
{
    if (previewHeader)
        return previewLine2;
    const auto view = proc.getPlayerView();
    return "STEP " + (view.running && view.step >= 0 ? juce::String (view.step + 1) : juce::String ("-")) + "/" + juce::String (intParam (fg::id::length))
           + " PAGE " + juce::String (proc.getEditorPage() + 1) + " TRUNK";
}

juce::String FractalGardenStyledPanel::headerCaption() const
{
    const auto h = proc.getHostSnapshot();
    const auto d = proc.getDiagnostics();
    juce::String position = "BAR -";
    if (h.ppqKnown)
    {
        const double barLength = 4.0 * (double) juce::jmax (1, h.sigNum) / (double) juce::jmax (1, h.sigDen);     // in quarter notes
        const double beatLength = 4.0 / (double) juce::jmax (1, h.sigDen);
        const double bars = std::floor (h.ppq / barLength);
        const int beat = (int) std::floor ((h.ppq - bars * barLength) / beatLength + 1e-9) + 1;
        position = "BAR " + juce::String ((int) bars + 1) + " BEAT " + juce::String (juce::jmin (beat, juce::jmax (1, h.sigNum)));
    }
    // (The test part is temporary, for milestone 3: the lamp click sends one note while Logic is stopped.)
    return position + "  |  LAMP = TEST NOTE  |  STOPPED BLOCKS " + juce::String (d.stoppedBlocks) + "  |  TEST NOTES " + juce::String (d.testNotesSent)
           + "/" + juce::String (d.testRequests) + "  |  NO POSITION " + juce::String (d.noPositionBlocks);
}

void FractalGardenStyledPanel::setPreviewHeader (const juce::String& l1, const juce::String& l2, bool lit)
{
    previewHeader = true; previewLine1 = l1; previewLine2 = l2; previewLights = lit; repaint();
}

void FractalGardenStyledPanel::clearPreviewHeader()
{
    previewHeader = false; previewLights = false; repaint();
}

void FractalGardenStyledPanel::syncState()
{
    // Knob Mode: three of the nine knob cards are shown
    const int mode = juce::jlimit (0, 2, intParam (fg::id::knobMode));
    for (std::size_t i = 0; i < knobCards.size(); ++i)
        knobCards[i]->setVisible ((int) i / 3 == mode);
    if (mode != shownMode)
    {
        shownMode = mode;
        repaint();
    }

    // The slot next to Order: REPEAT ENDS for Pendulum, CHANCE... for Chance mix, greyed for the other orders
    const int order = intParam (fg::id::order);
    const bool chance = order == fg::kOrderChanceMix;
    repeatEnds.setVisible (! chance);
    repeatEnds.setEnabled (order == fg::kOrderPendulum);
    chanceButton.setVisible (chance);
    if (! chance && chancePanel->isVisible())
        openChance (false);
}

void FractalGardenStyledPanel::timerCallback()
{
    // The glow of the momentary buttons (they pop back at once; the glow shows that they were pressed)
    for (auto& m : momentary)
        if (m->glow > 0 && --m->glow == 0)
        {
            m->button.getProperties().set ("glow", false);
            m->button.repaint();
        }

    syncState();
    stepEditor->refresh();
    updateHeaderLights();

    // The parts the panel paints itself depend on these
    const auto h = proc.getHostSnapshot();
    int sig = 17;
    auto mixIn = [&sig] (int v) { sig = sig * 31 + v; };
    mixIn (intParam (fg::id::length));
    mixIn (intParam (fg::id::root));
    mixIn (juce::jlimit (0, 7, roundedInt (floatParam (fg::id::branch) * 7.0f)));
    mixIn (h.known ? 1 : 0); mixIn (h.playing ? 1 : 0); mixIn (h.tempoKnown ? 1 : 0); mixIn (roundedInt ((float) h.bpm * 10.0f));
    mixIn (proc.getEditorPage());
    mixIn (chancePanel->isVisible() ? 1 : 0);
    if (sig != lastSignature)
    {
        const int oldBranch = lastSignature;   // (only used to decide whether the Path card needs a repaint)
        juce::ignoreUnused (oldBranch);
        lastSignature = sig;
        repaint();
        for (auto& c : knobCards)
            c->repaint();                      // the Path read-out depends on the Branch knob
    }
}

void FractalGardenStyledPanel::updateHeaderLights()
{
    // The window looks at the sequencer about 30 times a second, so a lamp is lit for a few looks after each step or note.
    const auto view = proc.getPlayerView();
    if (stepLampTicks > 0) --stepLampTicks;
    if (gateLampTicks > 0) --gateLampTicks;
    if (view.stepCount != lastStepCount) { lastStepCount = view.stepCount; stepLampTicks = 3; }
    if (view.noteCount != lastNoteCount) { lastNoteCount = view.noteCount; gateLampTicks = 3; }
    const bool newStepLit = view.running && stepLampTicks > 0;
    const bool newGateLit = view.running && (view.gate || gateLampTicks > 0);

    stepEditor->setPlayhead (view.running ? view.step : -1);

    const auto line1 = headerLine1(), line2 = headerLine2(), caption = headerCaption();
    if (newStepLit != stepLit || newGateLit != gateLit || line1 != lastLine1 || line2 != lastLine2 || caption != lastCaption)
    {
        stepLit = newStepLit; gateLit = newGateLit;
        lastLine1 = line1; lastLine2 = line2; lastCaption = caption;
        repaint (590, 10, 570, 80);             // only the header, not the whole window
    }
}

void FractalGardenStyledPanel::mouseDown (const juce::MouseEvent& e)
{
    // The STEP lamp (and the label next to it): asks for the temporary test note, which sounds only while Logic is stopped.
    if (juce::Rectangle<int> (840, 18, 90, 30).contains (e.getPosition()))
        proc.requestTestNote();
}

//==============================================================================
void FractalGardenStyledPanel::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // Title and header box
    drawAt (g, "FRACTAL-GARDEN", 48.0f, 40.0f, sans (46.0f, true, 0.12f), text);
    drawAt (g, "particledots / 64-step fractal sequencer (MIDI effect)", 50.0f, 72.0f, mono (16.0f), muted);

    g.setColour (well);
    g.fillRoundedRectangle (juce::Rectangle<float> (600.5f, 16.5f, 339.0f, 57.0f), 8.0f);
    g.setColour (cardEdge);
    g.drawRoundedRectangle (juce::Rectangle<float> (600.5f, 16.5f, 339.0f, 57.0f), 8.0f, 1.0f);
    drawAt (g, headerLine1(), 616.0f, 34.0f, mono (14.0f), text);
    drawAt (g, headerLine2(), 616.0f, 56.0f, mono (14.0f), amber);
    drawAt (g, "STEP", 884.0f, 34.0f, mono (13.0f), muted, anchorEnd);
    drawAt (g, "GATE", 884.0f, 56.0f, mono (13.0f), muted, anchorEnd);
    g.setColour (teal.withAlpha (stepLampLit() ? 1.0f : 0.25f));
    g.fillEllipse (902.0f, 28.0f, 12.0f, 12.0f);
    g.setColour (amber.withAlpha (gateLampLit() ? 1.0f : 0.25f));
    g.fillEllipse (902.0f, 50.0f, 12.0f, 12.0f);
    drawAt (g, headerCaption(), 1152.0f, 84.0f, mono (11.0f), muted, anchorEnd);

    // Knob Mode
    fillCard (g, { 712.0f, 392.0f, 440.0f, 62.0f });
    drawAt (g, "KNOB MODE", 726.0f, 408.0f, sans (14.5f, true, 0.12f), text);
    drawAt (g, "Micro: changes steps for good  |  Performance: does not", 1138.0f, 408.0f, sans (11.5f), muted, anchorEnd);

    // Reseed
    fillCard (g, { 712.0f, 462.0f, 440.0f, 88.0f });
    drawAt (g, "RESEED", 726.0f, 478.0f, sans (14.5f, true, 0.12f), text);
    drawAt (g, "WHEN A SINGLE TAB:", 856.0f, 502.0f, mono (11.5f), muted);

    // Tree: the play order of the trunk and the branches, with a letter for what each branch did (sample letters until milestone 6)
    fillCard (g, { 712.0f, 558.0f, 440.0f, 48.0f });
    drawAt (g, "TREE", 726.0f, 575.0f, mono (12.0f), muted);
    drawAt (g, "play order", 726.0f, 593.0f, sans (11.5f), muted);
    {
        const int nb = juce::jlimit (0, 7, roundedInt (floatParam (fg::id::branch) * 7.0f));
        static const char* const letters[7] = { "R", "I", "T", "M", "?", "R", "I" };
        const bool trunkPlays = previewTreePlay == 0;
        const juce::Rectangle<float> trunk (790.0f, 569.0f, 60.0f, 26.0f);
        g.setColour (trunkPlays ? teal : control);
        g.fillRoundedRectangle (trunk.reduced (0.5f), 5.0f);
        g.setColour (trunkPlays ? mixColours (teal, juce::Colours::white, 0.3f) : controlEdge);
        g.drawRoundedRectangle (trunk.reduced (0.5f), 5.0f, 1.0f);
        drawAt (g, "TRUNK", trunk.getCentreX(), trunk.getCentreY(), mono (11.5f, true), trunkPlays ? background : text, anchorMiddle);
        for (int i = 0; i < 7; ++i)
        {
            const juce::Rectangle<float> cell ((float) (854 + i * 40), 569.0f, 36.0f, 26.0f);
            const bool active = i < nb, playing = previewTreePlay == i + 1;
            g.setColour (active ? (playing ? teal : control) : mixColours (control, background, 0.5f));
            g.fillRoundedRectangle (cell.reduced (0.5f), 5.0f);
            g.setColour (active ? (playing ? mixColours (teal, juce::Colours::white, 0.3f) : controlEdge) : mixColours (controlEdge, background, 0.6f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, 1.0f);
            if (active)
                drawAt (g, letters[i], cell.getCentreX(), cell.getCentreY(), mono (13.0f, true), playing ? background : text, anchorMiddle);
            else
                drawAt (g, "-", cell.getCentreX(), cell.getCentreY(), mono (13.0f, true), muted.withAlpha (0.35f), anchorMiddle);
        }
    }

    // Row 1
    const int y1 = 618;
    smallCard (g, 48, y1, 120, "LENGTH");
    drawAt (g, juce::String (intParam (fg::id::length)), 108.0f, (float) y1 + 41.0f, mono (20.0f), amber, anchorMiddle);
    smallCard (g, 176, y1, 290, "SCALE  (28)");
    smallCard (g, 474, y1, 84, "KEY");
    smallCard (g, 566, y1, 310, "ORDER  (11)");
    smallCard (g, 884, y1, 150, "RATE  (step)");
    smallCard (g, 1042, y1, 110, "RATIO");

    // Row 2
    const int y2 = 690;
    smallCard (g, 48, y2, 286, "PATTERN  (16 slots)");
    fillCard (g, { 342.0f, (float) y2, 180.0f, 64.0f });
    drawAt (g, "ROOT", 412.0f, (float) y2 + 20.0f, sans (12.5f, true, 0.12f), text);
    drawAt (g, rootText(), 412.0f, (float) y2 + 42.0f, mono (15.0f), amber);
    smallCard (g, 530, y2, 130, "TRANSPOSE");
    smallCard (g, 668, y2, 130, "ROTATE");
    smallCard (g, 806, y2, 150, "RESIZE");
    smallCard (g, 964, y2, 80, "RESET");
    smallCard (g, 1052, y2, 100, "UNMUTATE");

    // Footer
    drawAt (g, "Fractal-Garden v" + versionText + " - an original MIDI effect inspired by the feel of a fractal sequencer; not affiliated with Qu-Bit Electronix",
            48.0f, 780.0f, mono (13.0f), muted);
}

void FractalGardenStyledPanel::resized() {}
