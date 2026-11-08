#include "SliderPanel.h"
#include <fg/Params.h>

namespace
{
    const juce::Colour kBackground (0xff16181b), kAmber (0xffe8a33d), kText (0xffe9e6df), kMuted (0xffa8a59d);

    juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    constexpr int kLeft = 24, kTop = 84, kColumnWidth = 384, kRowHeight = 29, kRowsPerColumn = 23, kLabelWidth = 120, kControlHeight = 26;
}

FractalGardenSliderPanel::FractalGardenSliderPanel (juce::AudioProcessorValueTreeState& s) : state (s)
{
    for (const auto& spec : fg::kParams)
    {
        controlIds.push_back (spec.id);
        Cell cell;
        switch (spec.kind)
        {
            case fg::Kind::Knob:
            case fg::Kind::Int:
            {
                auto row = std::make_unique<SliderRow>();
                row->label.setText (spec.name, juce::dontSendNotification);
                row->label.setFont (font (14.0f));
                row->label.setColour (juce::Label::textColourId, kText);
                row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
                row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
                row->attachment = std::make_unique<SliderAttachment> (state, spec.id, row->slider);
                addAndMakeVisible (row->label);
                addAndMakeVisible (row->slider);
                cell.a = &row->label; cell.b = &row->slider;
                sliders.push_back (std::move (row));
                break;
            }
            case fg::Kind::Choice:
            {
                auto row = std::make_unique<ChoiceRow>();
                row->label.setText (spec.name, juce::dontSendNotification);
                row->label.setFont (font (14.0f));
                row->label.setColour (juce::Label::textColourId, kText);
                juce::StringArray items;
                for (int i = 0; i < spec.numChoices; ++i)
                    items.add (spec.choices[i]);
                row->box.addItemList (items, 1);
                row->attachment = std::make_unique<ComboBoxAttachment> (state, spec.id, row->box);
                addAndMakeVisible (row->label);
                addAndMakeVisible (row->box);
                cell.a = &row->label; cell.b = &row->box;
                choices.push_back (std::move (row));
                break;
            }
            case fg::Kind::Toggle:
            case fg::Kind::Momentary:
            {
                auto item = std::make_unique<ButtonItem>();
                item->button.setButtonText (spec.name);
                item->button.setClickingTogglesState (true);
                item->attachment = std::make_unique<ButtonAttachment> (state, spec.id, item->button);
                addAndMakeVisible (item->button);
                cell.a = &item->button;
                buttons.push_back (std::move (item));
                break;
            }
        }
        cells.push_back (cell);
    }

    styledPanelButton.onClick = [this] { if (onSwitchToStyled) onSwitchToStyled(); };
    addAndMakeVisible (styledPanelButton);
}

FractalGardenSliderPanel::~FractalGardenSliderPanel() = default;

void FractalGardenSliderPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
    g.setColour (kText);
    g.setFont (font (34.0f, true));
    g.drawText ("FRACTAL-GARDEN", kLeft, 18, 600, 44, juce::Justification::centredLeft);
    g.setColour (kMuted);
    g.setFont (font (13.0f));
    g.drawText ("Plain sliders: one ordinary control for each of the 67 parameters.",
                kLeft, 58, 1000, 20, juce::Justification::centredLeft);
    g.drawText ("Fractal-Garden v" JucePlugin_VersionString " - an original MIDI effect inspired by the feel of a fractal sequencer; not affiliated with Qu-Bit Electronix",
                kLeft, kHeight - 30, kWidth - 2 * kLeft, 20, juce::Justification::centredLeft);
}

void FractalGardenSliderPanel::resized()
{
    styledPanelButton.setBounds (kWidth - 24 - 190, 22, 190, 34);
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        const int col = (int) i / kRowsPerColumn, row = (int) i % kRowsPerColumn;
        const int x = kLeft + col * kColumnWidth, y = kTop + row * kRowHeight;
        if (cells[i].b == nullptr)
            cells[i].a->setBounds (x, y, 250, kControlHeight);
        else
        {
            cells[i].a->setBounds (x, y, kLabelWidth, kControlHeight);
            cells[i].b->setBounds (x + kLabelWidth, y, kColumnWidth - kLabelWidth - 24, kControlHeight);
        }
    }
}
