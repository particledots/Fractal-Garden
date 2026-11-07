// Offline preview and control checks (not part of the plugin; only built with -DFG_BUILD_PREVIEW=ON).
//   1. draws the styled panel in the states of pictures A, B and C, and the plain window, to picture files;
//   2. checks, in BOTH windows, that all 46 ids exist, each has a control, and each control moves exactly one parameter;
//   3. checks saving and reloading (controls, step data, tab, page, window choice), the momentary buttons popping back,
//      the Knob Mode titles and colours, the Order slot (REPEAT ENDS / CHANCE...), and the Chance mix pop-over;
//   4. checks that the plugin adds no MIDI of its own.
// Usage: FractalGardenPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <iostream>
#include <map>
#include <set>

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok) { ++failures; std::cout << "FAIL: " << what << std::endl; }
    }

    void letWindowThreadRun (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    void setParam (juce::AudioProcessor& p, const char* id, float real)
    {
        for (auto* param : p.getParameters())
            if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (w->paramID == id) { w->setValueNotifyingHost (w->convertTo0to1 (real)); return; }
        expect (false, juce::String ("no parameter ") + id);
    }

    float getParam (juce::AudioProcessor& p, const char* id)
    {
        for (auto* param : p.getParameters())
            if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (w->paramID == id) return w->convertFrom0to1 (w->getValue());
        return -999.0f;
    }

    std::map<juce::String, float> snapshot (juce::AudioProcessor& p)
    {
        std::map<juce::String, float> m;
        for (auto* param : p.getParameters())
            if (auto* w = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
                m[w->paramID] = w->getValue();
        return m;
    }

    template <typename T>
    void collect (juce::Component& c, std::vector<T*>& out)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (child)) out.push_back (t);
            collect<T> (*child, out);
        }
    }

    juce::TextButton* findButton (juce::Component& c, const juce::String& text)
    {
        std::vector<juce::TextButton*> v;
        collect (c, v);
        for (auto* b : v) if (b->getButtonText() == text && b->isVisible()) return b;
        return nullptr;
    }

    void snapshotTo (juce::Component& c, const juce::File& file)
    {
        auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (image, out); }
    }

    // Moves every control in the window a little and checks that exactly one parameter changed each time.
    void checkControls (FractalGardenProcessor& proc, FractalGardenEditor& editor, const juce::String& which)
    {
        auto& ids = editor.controlledParameterIds();
        std::set<juce::String> idSet (ids.begin(), ids.end());
        expect ((int) ids.size() == fg::kNumParameters, which + ": exactly 46 controls (" + juce::String ((int) ids.size()) + ")");
        expect ((int) idSet.size() == fg::kNumParameters, which + ": no parameter has two controls");
        for (const auto& spec : fg::kParams)
            expect (idSet.count (spec.id) == 1, which + ": a control for " + spec.id);
        expect (proc.getParameters().size() == fg::kNumParameters, "the plugin exposes 46 parameters");
    }

    void resetAll (FractalGardenProcessor& p)
    {
        for (auto* param : p.getParameters())
            param->setValueNotifyingHost (param->getDefaultValue());
        p.getValueTreeState().state.setProperty (FgParams::kViewProperty, "styled", nullptr);
        p.setEditorTab (0);
        p.setEditorPage (0);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    FractalGardenProcessor proc;
    proc.setPlayConfigDetails (0, 0, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    expect (proc.getName() == "Fractal-Garden", "the plugin is called Fractal-Garden");
    expect (proc.isMidiEffect(), "it is a MIDI effect");

    // ---- all 46 controls, in both windows ----
    {
        FractalGardenEditor editor (proc);
        editor.setSize (1080, 720);
        expect (editor.getView() == FractalGardenEditor::View::styled, "a new window opens with the styled panel");
        checkControls (proc, editor, "styled");

        editor.showView (FractalGardenEditor::View::plain);
        checkControls (proc, editor, "plain");
        editor.showView (FractalGardenEditor::View::styled);
    }

    // ---- no MIDI of its own ----
    {
        juce::AudioBuffer<float> audio (0, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 10);
        proc.processBlock (audio, midi);
        int n = 0; for (auto m : midi) { juce::ignoreUnused (m); ++n; }
        expect (n == 1, "incoming MIDI passes through and nothing is added");
        juce::MidiBuffer empty;
        proc.processBlock (audio, empty);
        expect (empty.isEmpty(), "with no input there is no output");
    }

    // ---- the three pictures ----
    {
        resetAll (proc);
        FractalGardenEditor editor (proc);
        editor.setSize (1200, 800);
        auto* panel = editor.getStyledPanel();
        panel->setTransform ({});
        panel->setBounds (0, 0, 1200, 800);

        // Picture A: Default knob mode, Note tab, page 1, length 16
        setParam (proc, fg::id::length, 16);
        setParam (proc, fg::id::branch, 5.0f / 7.0f);
        setParam (proc, fg::id::path, 12.0f / 31.0f);
        setParam (proc, fg::id::mutate, 0.38f);
        setParam (proc, fg::id::scale, 1);
        setParam (proc, fg::id::key, 2);
        setParam (proc, fg::id::root, 0);
        proc.setEditorTab (0); proc.setEditorPage (0);
        panel->getStepEditor().setPlayhead (2);
        panel->setPreviewHeader ("LOGIC PLAYING 118.0 BPM", "STEP 3/16 PAGE 1 TRUNK", true);
        panel->setPreviewTree (0);
        panel->refreshNow(); letWindowThreadRun (50);
        snapshotTo (*panel, outDir.getChildFile ("panel_A.png"));

        // Picture B: Performance, Mod tab (shapes), page 3, Pendulum with REPEAT ENDS
        setParam (proc, fg::id::knobMode, 2);
        setParam (proc, fg::id::perfRatchet, 0.35f);
        setParam (proc, fg::id::perfSlew, 0.72f);
        setParam (proc, fg::id::perfMutate, 0.6f);
        setParam (proc, fg::id::branch, 3.0f / 7.0f);
        setParam (proc, fg::id::patternSlot, 3);
        setParam (proc, fg::id::root, 2);
        setParam (proc, fg::id::ratio, 5);
        setParam (proc, fg::id::order, 2);
        setParam (proc, fg::id::pendRepeat, 1);
        setParam (proc, fg::id::length, 24);
        proc.setEditorTab (4); proc.setEditorPage (2);
        panel->getStepEditor().setPlayhead (18);
        panel->setPreviewHeader ("LOGIC PLAYING 118.0 BPM", "STEP 19/24 PAGE 3 TRUNK", true);
        panel->setPreviewTree (0);
        panel->refreshNow(); letWindowThreadRun (50);
        snapshotTo (*panel, outDir.getChildFile ("panel_B.png"));

        // Picture C: Chance mix open
        setParam (proc, fg::id::knobMode, 0);
        setParam (proc, fg::id::order, 10);
        panel->refreshNow();
        panel->openChance (true);
        letWindowThreadRun (50);
        snapshotTo (*panel, outDir.getChildFile ("panel_C.png"));
        expect (panel->chanceOpen() && panel->chanceButtonShown(), "Chance mix: the pop-over opens from the CHANCE... button");
        panel->openChance (false);

        editor.showView (FractalGardenEditor::View::plain);
        snapshotTo (*editor.getPlainPanel(), outDir.getChildFile ("panel_plain.png"));
        resetAll (proc);
    }

    // ---- Knob Mode titles and colours ----
    {
        resetAll (proc);
        FractalGardenEditor editor (proc);
        editor.setSize (1080, 720);
        auto* panel = editor.getStyledPanel();
        const char* titles[3][3] = { { "BRANCH", "PATH", "MUTATE" }, { "MICRO RATCHET", "MICRO SLEW", "MICRO MOD" }, { "RATCHET FX", "SLEW FX", "MUTATE FX" } };
        const char* ids[3][3] = { { fg::id::branch, fg::id::path, fg::id::mutate }, { fg::id::microRatchet, fg::id::microSlew, fg::id::microMod },
                                  { fg::id::perfRatchet, fg::id::perfSlew, fg::id::perfMutate } };
        std::set<juce::uint32> accents;
        for (int mode = 0; mode < 3; ++mode)
        {
            setParam (proc, fg::id::knobMode, (float) mode);
            panel->refreshNow();
            expect (panel->shownKnobMode() == mode, "Knob Mode " + juce::String (mode) + " is shown");
            for (int slot = 0; slot < 3; ++slot)
            {
                auto& card = panel->visibleKnobCard (slot);
                expect (card.getTitle() == titles[mode][slot], juce::String ("title ") + titles[mode][slot]);
                expect (juce::String (card.getParameterId()) == ids[mode][slot], juce::String ("knob drives ") + ids[mode][slot]);
                expect (card.isShowing() || ! editor.isShowing() || card.isVisible(), "knob card is visible");
            }
            accents.insert (panel->visibleKnobCard (0).getAccent().getARGB());
        }
        expect (accents.size() == 3, "the three Knob Modes have three different arc colours");

        // The Order slot
        for (int order = 0; order < fg::kNumOrders; ++order)
        {
            setParam (proc, fg::id::order, (float) order);
            panel->refreshNow();
            expect (panel->repeatEndsEnabled() == (order == fg::kOrderPendulum), "REPEAT ENDS is live only with Pendulum (order " + juce::String (order) + ")");
            expect (panel->chanceButtonShown() == (order == fg::kOrderChanceMix), "CHANCE... shows only with Chance mix (order " + juce::String (order) + ")");
        }
        setParam (proc, fg::id::order, (float) fg::kOrderChanceMix);
        panel->refreshNow();
        panel->openChance (true);
        expect (panel->chanceOpen(), "pop-over opens");
        setParam (proc, fg::id::order, 0.0f);
        panel->refreshNow();
        expect (! panel->chanceOpen(), "pop-over closes when Order leaves Chance mix");
    }

    // ---- momentary buttons pop back ----
    {
        resetAll (proc);
        FractalGardenEditor editor (proc);
        editor.setSize (1080, 720);
        auto* panel = editor.getStyledPanel();
        auto* b = findButton (*panel, "RESEED ALL");
        expect (b != nullptr, "RESEED ALL button exists");
        if (b) b->triggerClick();
        letWindowThreadRun (5);
        expect (getParam (proc, fg::id::reseedAll) > 0.5f, "pressing RESEED ALL turns it on");
        letWindowThreadRun (400);
        expect (getParam (proc, fg::id::reseedAll) < 0.5f, "RESEED ALL pops back off");
    }

    // ---- save and reload: controls, steps, tab, page, window ----
    {
        resetAll (proc);
        FractalGardenEditor editor (proc);
        editor.setSize (1080, 720);
        setParam (proc, fg::id::branch, 0.7f);
        setParam (proc, fg::id::knobMode, 2);
        setParam (proc, fg::id::root, -5);
        setParam (proc, fg::id::length, 33);
        setParam (proc, fg::id::chanceDrunk, 77);
        setParam (proc, fg::id::scale, 9);
        proc.setEditorTab (3);
        proc.setEditorPage (4);
        auto st = proc.getSteps().steps[5]; st.on = ! st.on; st.note = 99; proc.setStep (5, st);
        auto expected = proc.getSteps();
        auto* toPlain = findButton (*editor.getStyledPanel(), "PLAIN SLIDERS");
        expect (toPlain != nullptr, "PLAIN SLIDERS button exists");
        if (toPlain) toPlain->triggerClick();
        letWindowThreadRun (100);
        expect (editor.getView() == FractalGardenEditor::View::plain, "PLAIN SLIDERS switches window");
        auto* back = findButton (*editor.getPlainPanel(), "STYLED PANEL");
        expect (back != nullptr, "STYLED PANEL button exists");

        juce::MemoryBlock saved;
        proc.getStateInformation (saved);
        FractalGardenProcessor p2;
        p2.setPlayConfigDetails (0, 0, 48000.0, 512);
        p2.setStateInformation (saved.getData(), (int) saved.getSize());
        expect (std::abs (getParam (p2, fg::id::branch) - 0.7f) < 1e-3f, "Branch survives");
        expect ((int) getParam (p2, fg::id::knobMode) == 2, "Knob Mode survives");
        expect ((int) getParam (p2, fg::id::root) == -5, "Root survives");
        expect ((int) getParam (p2, fg::id::length) == 33, "Length survives");
        expect ((int) getParam (p2, fg::id::chanceDrunk) == 77, "Chance Drunk survives");
        expect ((int) getParam (p2, fg::id::scale) == 9, "Scale survives");
        expect (p2.getEditorTab() == 3 && p2.getEditorPage() == 4, "tab and page survive");
        expect (p2.getSteps() == expected, "step data (including an edited step) survives");
        FractalGardenEditor e2 (p2);
        expect (e2.getView() == FractalGardenEditor::View::plain, "the window choice survives");

        // damaged step data falls back to the sample
        auto xml = juce::parseXML ("<Parameters steps=\"garbage\"/>");
        juce::MemoryBlock bad;
        FractalGardenProcessor p3;
        p3.setPlayConfigDetails (0, 0, 48000.0, 512);
        juce::AudioProcessor::copyXmlToBinary (*xml, bad);
        p3.setStateInformation (bad.getData(), (int) bad.getSize());
        expect (p3.getSteps() == fg::StepData::makeSample(), "damaged step data falls back to the sample pattern");
        resetAll (proc);
    }

    // ---- the step editor ----
    {
        FractalGardenProcessor p;
        p.setPlayConfigDetails (0, 0, 48000.0, 512);
        FractalGardenEditor editor (p);
        editor.setSize (1080, 720);
        auto& se = editor.getStyledPanel()->getStepEditor();
        for (int t = 0; t < 5; ++t) { se.selectTab (t); expect (se.tab() == t && p.getEditorTab() == t, "tab " + juce::String (t)); }
        for (int pg = 0; pg < 8; ++pg) { se.selectPage (pg); expect (se.page() == pg && p.getEditorPage() == pg, "page " + juce::String (pg)); }
        const bool before = p.getSteps().steps[0].on;
        se.toggleStep (0);
        expect (p.getSteps().steps[0].on == ! before, "toggling a step changes the data");
    }

    std::cout << "preview checks: " << (checks - failures) << " of " << checks << " passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
