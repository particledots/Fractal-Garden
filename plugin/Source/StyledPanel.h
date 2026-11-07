// The styled panel, laid out like the approved pictures (FractalGarden_panel_mockup_A / B / C; the exact positions, colours and
// sizes come from FractalGarden_panel_mockup_source.py). It is drawn in a fixed 1200 x 800 space; the editor scales it to the window.
//
// Every control is attached to the same parameter ids as the plain-slider window (see Parameters.h), so saved projects and
// automation work the same whichever window is showing. Milestone 2: the panel draws and edits control values and the sample step
// data; it makes no sound. The read-outs under the knobs are first guesses at what each position will mean; they are tuned together
// with the function in later milestones. The control ids never change.
//
// The step editor (tabs, pages, overview strip, eight step columns) is one component that paints itself; the Branch / Path / Mutate
// knobs are nine cards, three shown at a time (Knob Mode picks which three); the bottom rows are ordinary buttons and menus.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PanelLookAndFeel.h"
#include "Parameters.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
// A row of small buttons for a stepped choice, attached to a choice parameter.
class ChipSelector : public juce::Component
{
public:
    // "colours" gives each chip its own selected colour (empty = amber for all). chipWidth / pitch are in pixels.
    ChipSelector (juce::AudioProcessorValueTreeState&, const char* parameterId, const juce::StringArray& labels,
                  float fontSize, int chipWidth, int pitch, std::vector<juce::Colour> colours = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void select (int index);                        // what a click does; also used by the automatic checks
    int getSelected() const { return selected; }
    int getNumChips() const { return labels.size(); }
    const char* getParameterId() const { return parameterId; }

private:
    int indexAt (juce::Point<int>) const;

    const char* parameterId;
    juce::AudioParameterChoice* parameter = nullptr;
    juce::StringArray labels;
    float fontSize;
    int chipWidth, pitch;
    std::vector<juce::Colour> colours;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;     // declared last: destroyed first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipSelector)
};

//==============================================================================
// One knob with its title, read-out and note (208 x 214, like the pictures).
class KnobCard : public juce::Component
{
public:
    KnobCard (juce::AudioProcessorValueTreeState&, const char* parameterId, const juce::String& title, juce::Colour accent,
              std::function<juce::String (double)> readout, std::function<juce::String()> note, std::vector<double> marks = {});

    void paint (juce::Graphics&) override;
    void resized() override;

    const char* getParameterId() const { return parameterId; }
    const juce::String& getTitle() const { return title; }
    juce::Colour getAccent() const { return accent; }
    juce::Slider& getSlider() { return slider; }
    juce::String getReadoutText() const { return readout (slider.getValue()); }
    juce::String getNoteText() const { return note(); }

    static constexpr int kWidth = 208, kHeight = 214;

private:
    const char* parameterId;
    juce::String title;
    juce::Colour accent;
    std::function<juce::String (double)> readout;
    std::function<juce::String()> note;
    std::vector<double> marks;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;   // after the slider: destroyed first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobCard)
};

//==============================================================================
// The step editor: five tabs, the Mod Out chips' label, eight page chips, the overview strip of all 64 steps and eight step columns.
// It paints itself and handles clicks and drags. A click on a step's ON / OFF button switches the step; a click or drag in a step's box
// sets that step's value for the current tab (milestone 4: a note moves by scale steps, and the read-out is the note name as it
// will sound); a double click puts the value back to its default. Pages beyond LENGTH cannot be chosen.
class StepEditor : public juce::Component
{
public:
    static constexpr int kX = 48, kY = 88, kW = 1104, kH = 292;      // where it sits in the panel

    explicit StepEditor (FractalGardenProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void refresh();                                  // repaints when the steps, tab, page, length or Mod Out mode changed
    void setPlayhead (int step) { if (step != playhead) { playhead = step; repaint(); } }      // the step Logic's playback is on, -1 = none (stopped)

    // For the automatic checks
    int tab() const;
    int page() const;
    void selectTab (int);
    void selectPage (int);
    void toggleStep (int stepIndex);
    void setValue (int stepIndex, int tabIndex, float fraction);      // what a click or drag in the box does: 0 = bottom, 1 = top
    void resetValue (int stepIndex, int tabIndex);                    // what a double click does: the value goes back to its default

private:
    int length() const;
    int modMode() const;
    int scaleIndex() const;
    int keyIndex() const;
    int rootValue() const;
    int baseNoteValue() const;
    int activePages() const;                          // pages that LENGTH reaches
    int stepUnder (juce::Point<int> panelPoint) const;       // the step whose value box is under this point (panel coordinates), or -1
    void dragValue (int stepIndex, int tabIndex, float mouseY);
    double overviewFraction (const fg::Step&, int tabIndex) const;

    FractalGardenProcessor& proc;
    int playhead = -1;
    int lastRevision = -1, lastTab = -1, lastPage = -1, lastLength = -1, lastModMode = -1, lastScale = -1, lastKey = -1, lastRoot = -999;
    int dragStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepEditor)
};

//==============================================================================
// The Chance mix pop-over (picture C): eight small knobs and a DONE button.
class ChanceMixPanel : public juce::Component
{
public:
    explicit ChanceMixPanel (juce::AudioProcessorValueTreeState&);

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onDone;
    const std::vector<juce::String>& controlledParameterIds() const { return ids; }

    static constexpr int kWidth = 640, kHeight = 154;

private:
    struct Dial { juce::Slider slider; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; juce::String name; };
    std::vector<std::unique_ptr<Dial>> dials;
    std::vector<juce::String> ids;
    juce::TextButton done { "DONE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChanceMixPanel)
};

//==============================================================================
// The settings page (milestone 9), a pop-over like the Chance mix one: MIDI channel, the two CC numbers, the base note, the Resize
// method, the Reseed filter (which parts RESEED ALL changes) and the branch-regeneration filter (which edits make the branches again).
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel (juce::AudioProcessorValueTreeState&);

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onDone;
    const std::vector<juce::String>& controlledParameterIds() const { return ids; }

    static constexpr int kWidth = 820, kHeight = 392;

private:
    struct Stepper { juce::Slider slider; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; juce::String name; };
    struct Toggle { juce::TextButton button; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment; };
    juce::AudioProcessorValueTreeState& state;
    std::vector<std::unique_ptr<Stepper>> steppers;       // MIDI channel, Slew CC, Mod CC, Base note
    std::unique_ptr<ChipSelector> resizeChips;
    std::vector<std::unique_ptr<Toggle>> reseedToggles, regenToggles;
    std::vector<juce::String> ids;
    juce::TextButton done { "DONE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

//==============================================================================
class FractalGardenStyledPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth  = 1200;
    static constexpr int kHeight = 800;

    FractalGardenStyledPanel (FractalGardenProcessor&, const juce::String& versionText);
    ~FractalGardenStyledPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Called when the "Plain sliders" button is pressed; the editor swaps windows.
    std::function<void()> onSwitchToPlain;

    // The ids of all parameters that have a control in this window (used by the check that none is missing).
    const std::vector<juce::String>& controlledParameterIds() const { return controlIds; }

    // ---- for the automatic checks and the preview pictures ----
    StepEditor& getStepEditor() { return *stepEditor; }
    int shownKnobMode() const { return shownMode; }
    KnobCard& visibleKnobCard (int slot) { return *knobCards[(std::size_t) (shownMode * 3 + slot)]; }
    bool chanceOpen() const { return chancePanel->isVisible(); }
    void openChance (bool open);
    bool settingsOpen() const { return settingsPanel->isVisible(); }
    void openSettings (bool open);
    juce::Component& settingsComponent() { return *settingsPanel; }      // for the automatic checks
    bool repeatEndsEnabled() const { return repeatEnds.isEnabled() && repeatEnds.isVisible(); }
    bool chanceButtonShown() const { return chanceButton.isVisible(); }
    juce::String headerLine1() const;
    juce::String headerLine2() const;
    juce::String headerCaption() const;                      // Logic's song position (bar and beat), under the header
    bool stepLampLit() const { return previewLights || stepLit; }
    bool gateLampLit() const { return previewLights || gateLit; }
    void setPreviewHeader (const juce::String& line1, const juce::String& line2, bool lightsOn);   // pictures only: fixed header text and lit lights
    void clearPreviewHeader();
    void setPreviewTree (int playingBranch) { previewTreePlay = playingBranch; repaint(); }
    void refreshNow() { timerCallback(); }

private:
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct MomentaryButton { juce::TextButton button; std::unique_ptr<ButtonAttachment> attachment; int glow = 0; };
    struct Menu { juce::ComboBox box; std::unique_ptr<ComboBoxAttachment> attachment; };

    void timerCallback() override;
    int intParam (const char* id) const;
    float floatParam (const char* id) const;
    void addMomentary (const char* id, const juce::String& label, float fontSize, juce::Rectangle<int> bounds, juce::Colour stripe = {});
    void addMenu (const char* id, juce::Rectangle<int> bounds);
    void stepLength (int delta);
    void syncState();
    juce::String rootText() const;

    FractalGardenProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    juce::String versionText;
    std::vector<juce::String> controlIds;

    std::unique_ptr<StepEditor> stepEditor;
    std::unique_ptr<ChipSelector> modOutChips, knobModeChips, reseedScopeChips;
    std::vector<std::unique_ptr<KnobCard>> knobCards;           // 9: Default 3, Micro Mutate 3, Performance 3
    std::vector<std::unique_ptr<MomentaryButton>> momentary;
    std::vector<std::unique_ptr<Menu>> menus;
    juce::TextButton plainSliders { "PLAIN SLIDERS" };
    juce::TextButton lengthDown { "<" }, lengthUp { ">" };
    juce::TextButton repeatEnds { "REPEAT ENDS" };
    std::unique_ptr<ButtonAttachment> repeatEndsAttachment;
    juce::TextButton chanceButton { "CHANCE..." };
    juce::TextButton settingsButton { "SETTINGS..." };
    std::unique_ptr<SettingsPanel> settingsPanel;
    juce::Slider rootDial;
    std::unique_ptr<SliderAttachment> rootAttachment;
    std::unique_ptr<ChanceMixPanel> chancePanel;

    void updateHeaderLights();                  // follows Logic's playback: lamps, playhead, header text (repaints only the header)

    int shownMode = 0;
    int lastSignature = 0;
    std::uint32_t lastStepCount = 0, lastNoteCount = 0;
    int stepLampTicks = 0, gateLampTicks = 0;   // how much longer each lamp stays lit after a step / a note
    bool stepLit = false, gateLit = false;
    juce::String lastLine1, lastLine2, lastCaption;
    bool previewHeader = false, previewLights = false;
    juce::String previewLine1, previewLine2;
    int previewTreePlay = -1;                   // pictures and checks only: >= 0 shows that part (0 = trunk) as playing
    int liveTreeSection = -1;                   // the part Logic's playback is on: 0 = trunk, 1 to 7 = branch, -1 = not playing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalGardenStyledPanel)
};
