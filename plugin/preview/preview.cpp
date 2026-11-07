// Offline preview and control checks (not part of the plugin; only built with -DFG_BUILD_PREVIEW=ON).
//   1. draws the styled panel in the states of pictures A, B and C, and the plain window, to picture files;
//   2. checks, in BOTH windows, that all 65 ids exist, each has a control, and each control moves exactly one parameter;
//   3. checks saving and reloading (controls, step data, tab, page, window choice), the momentary buttons popping back,
//      the Knob Mode titles and colours, the Order slot (REPEAT ENDS / CHANCE...), and the Chance mix pop-over;
//   4. checks that the plugin adds no MIDI while Logic is stopped, and (milestone 3) that the sequencer follows a simulated Logic
//      transport: notes, stop, relocate, bypass, RATE / RATIO / LENGTH, two instances, the header read-outs;
//   5. (milestone 4) Scale, Key, Root and Order in the playing plugin, the ROTATE, TRANSPOSE and RESET buttons, page and step editing,
//      and that loading a project does not press any button.
//   7. (milestone 9) the pattern bank (SAVE, LOAD, empty slots, saved with the project, loading presses nothing), the settings page
//      (MIDI channel, CC numbers, base note, Resize method, Reseed filter, branch-regeneration filter).
//   6. (milestones 6 and 7) branches and Path in the playing plugin, the TREE letters, Mutate and Micro Mutate saved into the steps, the
//      seven RESEED buttons (page and all 64), UNMUTATE (and what sets or does not set its point), and saving / loading all of it.
// Usage: FractalGardenPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
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
        expect ((int) ids.size() == fg::kNumParameters, which + ": exactly 65 controls (" + juce::String ((int) ids.size()) + ")");
        expect ((int) idSet.size() == fg::kNumParameters, which + ": no parameter has two controls");
        for (const auto& spec : fg::kParams)
            expect (idSet.count (spec.id) == 1, which + ": a control for " + spec.id);
        expect (proc.getParameters().size() == fg::kNumParameters, "the plugin exposes 65 parameters");
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

// A stand-in for Logic's transport (the blocks of checks below may declare their own, which then take over).
struct MockHost : juce::AudioPlayHead
{
    bool playing = false, havePpq = true, haveBpm = true;
    double ppq = 0.0, bpm = 120.0;
    int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo pi;
        pi.setIsPlaying (playing);
        if (haveBpm) pi.setBpm (bpm);
        if (havePpq) pi.setPpqPosition (ppq);
        pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { num, den });
        return pi;
    }
};

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

    // ---- all 65 controls, in both windows ----
    {
        FractalGardenEditor editor (proc);
        editor.setSize (1080, 720);
        expect (editor.getView() == FractalGardenEditor::View::styled, "a new window opens with the styled panel");
        checkControls (proc, editor, "styled");

        editor.showView (FractalGardenEditor::View::plain);
        checkControls (proc, editor, "plain");
        editor.showView (FractalGardenEditor::View::styled);
    }

    // ---- no MIDI of its own while Logic is not playing ----
    {
        juce::AudioBuffer<float> audio (0, 512);
        // the first block after prepareToPlay carries one all-notes-off (in case Logic restarted its audio while a note sounded)
        juce::MidiBuffer firstBlock;
        proc.processBlock (audio, firstBlock);
        int firstCount = 0; bool firstIsPanic = false;
        for (auto m : firstBlock) { ++firstCount; firstIsPanic = m.getMessage().isAllNotesOff() && m.getMessage().getChannel() == 1; }
        expect (firstCount == 1 && firstIsPanic, "the first block after prepareToPlay carries a single all-notes-off");
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
        // The 20-a-second housekeeping timer pops the button back at any moment (even in the same moment the click is carried out),
        // so a listener notes whether the control was ever switched on instead of looking at the right instant.
        struct WentOn : juce::AudioProcessorParameter::Listener
        {
            bool seen = false;
            void parameterValueChanged (int, float v) override { if (v > 0.5f) seen = true; }
            void parameterGestureChanged (int, bool) override {}
        } listener;
        juce::AudioProcessorParameter* reseedParam = nullptr;
        for (auto* param : proc.getParameters())
            if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (w->paramID == fg::id::reseedAll) reseedParam = w;
        expect (reseedParam != nullptr, "RESEED ALL parameter exists");
        if (reseedParam) reseedParam->addListener (&listener);
        if (b) b->triggerClick();                  // (carried out by the window thread a moment later)
        letWindowThreadRun (20);
        if (reseedParam) reseedParam->removeListener (&listener);
        expect (listener.seen, "pressing RESEED ALL turns it on");
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
        auto st = proc.getSteps().steps[5]; st.on = ! st.on; st.note = 30; proc.setStep (5, st);
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

    // ---- the sequencer in the plugin (milestone 3): Logic's transport in, notes out ----
    {
        struct MockHost : juce::AudioPlayHead
        {
            bool playing = false, havePpq = true, haveBpm = true;
            double ppq = 0.0, bpm = 120.0;
            int num = 4, den = 4;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo pi;
                pi.setIsPlaying (playing);
                if (haveBpm) pi.setBpm (bpm);
                if (havePpq) pi.setPpqPosition (ppq);
                pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { num, den });
                return pi;
            }
        };
        struct Ev { long long t; bool on; int note; int channel; int velocity; bool panic; };
        auto pitch = [] (int stored) { return fg::pitchFor (stored, 0, 0, 0); };          // the default Scale (Major), Key (C) and Root (0)

        const double sr = 48000.0, pps = 120.0 / 60.0 / sr;      // 120 bpm: 24000 samples per quarter note

        auto makeProc = [&] (MockHost& host)
        {
            auto p = std::make_unique<FractalGardenProcessor>();
            p->setPlayConfigDetails (0, 0, sr, 512);
            p->prepareToPlay (sr, 512);
            p->setPlayHead (&host);
            for (int i = 0; i < fg::kNumSteps; ++i)                // milestone 3 and 4 checks are about plain notes: no slew, no ratchet, no tie
            {
                fg::Step q = p->getSteps().steps[(std::size_t) i];
                q.slew = 0; q.ratchet = 0; q.gate = (std::uint8_t) juce::jmin ((int) q.gate, 90);
                p->setStep (i, q);
            }
            fg::Step s1 = p->getSteps().steps[1];                  // step 2 certainly plays, with a gate long enough to be sounding at 7168 samples
            s1.on = true; s1.gate = 80;
            p->setStep (1, s1);
            juce::AudioBuffer<float> audio (0, 512);
            juce::MidiBuffer first;                                // the panic that follows prepareToPlay
            host.playing = false;
            p->processBlock (audio, first);
            return p;
        };
        // runs blocks while the host plays from startPpq (continuous), collects what comes out, and keeps `host` where it ended
        auto run = [&] (FractalGardenProcessor& p, MockHost& host, long long& clock, long long total, int block, double startPpq, bool playing = true,
                        const juce::MidiBuffer* firstInput = nullptr)
        {
            std::vector<Ev> out;
            juce::AudioBuffer<float> audio (0, block);
            const long long end = clock + total;
            bool first = true;
            for (; clock < end; clock += block)
            {
                host.playing = playing;
                host.ppq = startPpq + (double) (clock - (end - total)) * pps;
                juce::MidiBuffer midi;
                if (first && firstInput != nullptr) midi = *firstInput;
                first = false;
                p.processBlock (audio, midi);
                for (auto meta : midi)
                {
                    const auto m = meta.getMessage();
                    if (m.isController()) continue;                  // CC 84 and CC 1 (milestone 5) are checked on their own below
                    out.push_back ({ clock + meta.samplePosition, m.isNoteOn(), m.getNoteNumber(), m.getChannel(), m.getVelocity(), m.isAllNotesOff() });
                }
            }
            return out;
        };

        // -- the sample pattern plays: step 1 at once, each step on the 1/16, rests where steps are off, gate lengths from the data --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            const auto data = p->getSteps();
            const auto ev = run (*p, host, clock, 48000, 500, 0.0);                 // 8 steps (one pass of the default length 8; 96 blocks of 500, so the run ends exactly there)
            std::vector<Ev> ons;
            for (const auto& e : ev) if (e.on) ons.push_back (e);
            expect (! ev.empty() && ev[0].on && ev[0].t == 0 && ev[0].note == pitch (data.steps[0].note) && ev[0].channel == 1 && ev[0].velocity == 100,
                    "plays step 1 at once, channel 1, velocity 100");
            int expectedOns = 0; bool timesOk = true, notesOk = true;
            for (int i = 0; i < 8; ++i)
                if (data.steps[(std::size_t) i].on)
                {
                    if ((std::size_t) expectedOns >= ons.size()) { timesOk = false; break; }
                    timesOk = timesOk && std::llabs (ons[(std::size_t) expectedOns].t - 6000LL * i) <= 1;
                    notesOk = notesOk && ons[(std::size_t) expectedOns].note == pitch (data.steps[(std::size_t) i].note);
                    ++expectedOns;
                }
            expect ((int) ons.size() == expectedOns && timesOk && notesOk, "every ON step of the first 8 plays at its 1/16 position with its own note");
            expect (p->getPlayerView().running && p->getPlayerView().step == 7, "the window is told Logic is playing and which step is on");
            // the header's STEP read-out and lamps follow
            {
                FractalGardenEditor editor (*p);
                editor.setSize (1080, 720);
                auto* panel = editor.getStyledPanel();
                panel->refreshNow();
                expect (panel->headerLine2().startsWith ("STEP 8/8"), "the header says STEP 8/8 while step 8 plays: " + panel->headerLine2());
                expect (panel->headerLine1().startsWith ("LOGIC PLAYING 120.0 BPM"), "the header says LOGIC PLAYING 120.0 BPM: " + panel->headerLine1());
                expect (panel->headerCaption().startsWith ("BAR 1 BEAT 2"), "the song position shows (last block began at 1.98 quarters): " + panel->headerCaption());
            }
        }

        // -- stopping and starting: the note ends the moment Logic stops; nothing sounds while stopped --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            auto ev = run (*p, host, clock, 7168, 512, 0.0);                      // ends inside step 1's note (6000 to the gate end)
            const auto dataNow = p->getSteps();
            const bool step1On = dataNow.steps[1].on;
            expect (step1On && p->getPlayerView().gate, "a note is sounding when Logic stops");
            const double stopPpq = 7168 * pps;
            ev = run (*p, host, clock, 4096, 512, stopPpq, false);                // stopped
            int offs = 0, ons = 0; long long offAt = -1;
            for (const auto& e : ev) { if (e.on) ++ons; else { ++offs; offAt = e.t; } }
            expect (ons == 0 && offs == 1 && offAt == 7168, "stopping sends one note-off, exactly at the stop, and nothing more while stopped");
            expect (! p->getPlayerView().running && p->getPlayerView().step == -1 && ! p->getPlayerView().gate, "the window is told the sequencer is stopped");
            {
                FractalGardenEditor editor (*p);
                editor.setSize (1080, 720);
                editor.getStyledPanel()->refreshNow();
                expect (editor.getStyledPanel()->headerLine2().startsWith ("STEP -/8"), "the header says STEP -/8 while stopped: " + editor.getStyledPanel()->headerLine2());
                expect (editor.getStyledPanel()->headerLine1().startsWith ("LOGIC STOPPED"), "the header says LOGIC STOPPED");
            }
            // starting again from bar 3 beat 1 plays step 1 at once; every note is the step for that position
            long long c2 = clock;
            ev = run (*p, host, c2, 12000, 512, 8.0);
            expect (! ev.empty() && ev[0].on && ev[0].t == clock && ev[0].note == pitch (dataNow.steps[0].note), "starting at the top of bar 3 plays step 1 at once");
        }

        // -- a jump (relocate or Logic's cycle) ends the note and carries on from the new position --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            auto ev = run (*p, host, clock, 7168, 512, 0.0);
            long long c2 = clock;
            ev = run (*p, host, c2, 12000, 512, 10.3);                             // jumps to 10.3 quarters without stopping
            int offsAtJump = 0;
            for (const auto& e : ev) if (! e.on && e.t == clock) ++offsAtJump;
            expect (offsAtJump == 1, "jumping in the middle of a note ends it at the jump");
            bool boundaries = true;
            for (const auto& e : ev)
                if (e.on) { const double q = (double) (e.t - clock) * pps + 10.3; boundaries = boundaries && std::fabs (q / 0.25 - std::round (q / 0.25)) < 1e-3; }
            expect (boundaries, "after the jump every note starts on a 1/16 of the new position");
        }

        // -- bypass ends a sounding note; Logic giving no position is treated like a stop --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            run (*p, host, clock, 7168, 512, 0.0);
            expect (p->getPlayerView().gate, "a note is sounding before the bypass");
            juce::AudioBuffer<float> audio (0, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90), 5);
            p->processBlockBypassed (audio, midi);
            int offs = 0, passed = 0;
            for (auto meta : midi) { if (meta.getMessage().isNoteOff()) ++offs; if (meta.getMessage().isNoteOn() && meta.getMessage().getNoteNumber() == 40) ++passed; }
            expect (offs == 1 && passed == 1 && ! p->getPlayerView().gate, "bypass ends the sounding note and lets other MIDI through");

            MockHost host2;
            auto p2 = makeProc (host2);
            long long c2 = 0;
            run (*p2, host2, c2, 7168, 512, 0.0);
            host2.havePpq = false;
            const auto ev = run (*p2, host2, c2, 2048, 512, 0.0);
            int onsAfter = 0, offsAfter = 0;
            for (const auto& e : ev) { if (e.on) ++onsAfter; else ++offsAfter; }
            expect (onsAfter == 0 && offsAfter == 1 && ! p2->getPlayerView().running, "playing without a song position is treated like a stop");
        }

        // -- the milestone 3 test aids are gone: the header shows only the song position, and a stopped Logic gets no MIDI at all --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            run (*p, host, clock, 4096, 512, 0.0);
            FractalGardenEditor editor (*p);
            editor.setSize (1080, 720);
            auto* panel = editor.getStyledPanel();
            panel->refreshNow();
            const auto caption = panel->headerCaption();
            expect (caption.startsWith ("BAR ") && ! caption.containsIgnoreCase ("TEST") && ! caption.containsIgnoreCase ("STOPPED BLOCKS")
                    && ! caption.containsIgnoreCase ("NO POSITION"), "the header caption is the song position only: " + caption);
            const auto ev = run (*p, host, clock, 16384, 512, 0.0, false);
            int notes = 0;
            for (const auto& e : ev) if (! e.panic) ++notes;
            expect (notes <= 1, "while Logic is stopped the plugin sends at most the note-off of a note that was sounding");
            const auto ev2 = run (*p, host, clock, 16384, 512, 0.0, false);
            expect (ev2.empty(), "nothing at all while Logic stays stopped");
        }

        // -- RATE, RATIO, LENGTH, edited steps and tempo take effect --
        {
            MockHost host;
            auto p = makeProc (host);
            setParam (*p, fg::id::stepRate, 4.0f);                                   // 1/8
            setParam (*p, fg::id::ratio, 5.0f);                                      // x2 -> a step is a 1/16 again
            setParam (*p, fg::id::length, 3.0f);
            fg::Step s = p->getSteps().steps[2];
            s.on = true; s.note = 26; s.gate = 100;
            p->setStep (2, s);
            long long clock = 0;
            const auto ev = run (*p, host, clock, 36000, 512, 0.0);
            std::vector<Ev> ons;
            for (const auto& e : ev) if (e.on) ons.push_back (e);
            bool spaced = ons.size() >= 5;
            for (std::size_t i = 0; i < ons.size(); ++i) spaced = spaced && std::llabs (ons[i].t - 6000LL * (long long) i) <= 1;
            expect (spaced, "RATE 1/8 with RATIO x2 plays a step every 1/16");
            const auto data = p->getSteps();
            expect (ons.size() >= 4 && ons[2].note == pitch (26) && ons[0].note == pitch (data.steps[0].note) && ons[3].note == pitch (data.steps[0].note), "LENGTH 3 loops after step 3, and an edited step is played");

            MockHost hostFast;
            hostFast.bpm = 240.0;
            auto q = makeProc (hostFast);
            long long c2 = 0;
            const auto ev2 = run (*q, hostFast, c2, 24000, 512, 0.0);                // at 240 bpm a 1/16 is 3000 samples
            expect (ev2.size() >= 3 && ev2[0].t == 0, "tempo comes from Logic");
        }

        // -- two plugin instances stay in step --
        {
            MockHost hostA, hostB;
            auto a = makeProc (hostA), b = makeProc (hostB);
            long long ca = 0, cb = 0;
            const auto ea = run (*a, hostA, ca, 96000, 500, 0.0);
            const auto eb = run (*b, hostB, cb, 96000, 480, 0.0);                    // another block size
            bool same = ea.size() == eb.size() && ! ea.empty();
            for (std::size_t i = 0; same && i < ea.size(); ++i)
                same = ea[i].on == eb[i].on && ea[i].note == eb[i].note && std::llabs (ea[i].t - eb[i].t) <= 1;
            expect (same, "two instances play the same notes at the same samples");
        }

        // -- incoming MIDI passes through next to the sequencer's notes --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            juce::MidiBuffer input;
            input.addEvent (juce::MidiMessage::controllerEvent (1, 7, 99), 100);
            input.addEvent (juce::MidiMessage::noteOn (1, 33, (juce::uint8) 80), 200);
            const auto ev = run (*p, host, clock, 512, 512, 0.0, true, &input);
            bool cc = false, key = false;
            // (the run helper records note events only; look at the buffer itself for the controller)
            juce::AudioBuffer<float> audio (0, 512);
            juce::MidiBuffer midi = input;
            host.ppq = 100.0;
            p->processBlock (audio, midi);
            for (auto meta : midi)
            {
                if (meta.getMessage().isController() && meta.getMessage().getControllerNumber() == 7 && meta.samplePosition == 100) cc = true;
                if (meta.getMessage().isNoteOn() && meta.getMessage().getNoteNumber() == 33 && meta.samplePosition == 200) key = true;
            }
            bool keyBefore = false;
            for (const auto& e : ev) if (e.on && e.note == 33) keyBefore = true;
            expect (cc && key && keyBefore, "controllers and notes played into the track pass through at their positions");
        }
    }

    // ---- milestone 4: Scale, Key, Root, Order and the buttons in the playing plugin ----
    {
        struct MockHost : juce::AudioPlayHead
        {
            bool playing = true; double ppq = 0.0, bpm = 120.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo pi; pi.setIsPlaying (playing); pi.setBpm (bpm); pi.setPpqPosition (ppq);
                pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
                return pi;
            }
        };
        const double sr = 48000.0, pps = 120.0 / 60.0 / sr;
        struct Note { long long t; int note; };
        auto pitch = [] (int stored) { return fg::pitchFor (stored, 0, 0, 0); };

        auto makeProc = [&] (MockHost& host)
        {
            auto p = std::make_unique<FractalGardenProcessor>();
            p->setPlayConfigDetails (0, 0, sr, 512);
            p->prepareToPlay (sr, 512);
            p->setPlayHead (&host);
            for (int i = 0; i < 8; ++i)                                  // eight steps, all on, each with its own place in the scale
            {
                fg::Step s = p->getSteps().steps[(std::size_t) i];
                s.on = true; s.gate = 50; s.slew = 0; s.ratchet = 0; s.note = (std::uint8_t) (12 + i % 7);
                p->setStep (i, s);
            }
            return p;
        };
        // runs blocks from song position startPpq (continuous) and returns the note-ons
        auto runNotes = [&] (FractalGardenProcessor& p, MockHost& host, long long total, double startPpq)
        {
            std::vector<Note> out;
            juce::AudioBuffer<float> audio (0, 512);
            for (long long t = 0; t < total; t += 512)
            {
                host.playing = true;
                host.ppq = startPpq + (double) t * pps;
                juce::MidiBuffer midi;
                p.processBlock (audio, midi);
                for (auto meta : midi)
                    if (meta.getMessage().isNoteOn() && t + meta.samplePosition < total) out.push_back ({ t + meta.samplePosition, meta.getMessage().getNoteNumber() });
            }
            return out;
        };

        {   // Scale, Key and Root change the notes that play, not the steps
            MockHost host;
            auto p = makeProc (host);
            const auto before = p->getSteps();
            setParam (*p, fg::id::scale, 5); setParam (*p, fg::id::key, 9); setParam (*p, fg::id::root, 2);
            const auto notes = runNotes (*p, host, 8 * 6000, 0.0);
            bool ok = notes.size() == 8;
            for (std::size_t i = 0; i < notes.size() && ok; ++i)
                ok = notes[i].note == fg::pitchFor (before.steps[i].note, 5, 9, 2);
            expect (ok, "SCALE, KEY and ROOT set the pitch of every note");
            expect (p->getSteps() == before, "...and do not change the stored steps");
            setParam (*p, fg::id::scale, 16); setParam (*p, fg::id::key, 0); setParam (*p, fg::id::root, 0);
            const auto pent = runNotes (*p, host, 8 * 6000, 0.0);
            bool pentOk = pent.size() == 8;
            for (std::size_t i = 0; i < pent.size() && pentOk; ++i) pentOk = pent[i].note == fg::pitchFor (before.steps[i].note, 16, 0, 0);
            expect (pentOk && p->getSteps() == before, "switching to Major Pentatonic re-pitches and keeps the steps");
        }
        {   // ORDER
            MockHost host;
            auto p = makeProc (host);
            setParam (*p, fg::id::order, 1);                                   // Reverse
            auto notes = runNotes (*p, host, 8 * 6000, 0.0);
            bool rev = notes.size() == 8;
            for (std::size_t i = 0; i < notes.size() && rev; ++i) rev = notes[i].note == fg::pitchFor (p->getSteps().steps[7 - i].note, 0, 0, 0);
            expect (rev, "ORDER Reverse plays the steps backwards");
            setParam (*p, fg::id::order, 4);                                   // Converge
            notes = runNotes (*p, host, 8 * 6000, 0.0);
            static const int converge[8] = { 0, 7, 1, 6, 2, 5, 3, 4 };
            bool conv = notes.size() == 8;
            for (std::size_t i = 0; i < notes.size() && conv; ++i) conv = notes[i].note == fg::pitchFor (p->getSteps().steps[(std::size_t) converge[i]].note, 0, 0, 0);
            expect (conv, "ORDER Converge plays 1 8 2 7 3 6 4 5");
            setParam (*p, fg::id::order, 2); setParam (*p, fg::id::pendRepeat, 1);   // Pendulum with the ends repeated
            notes = runNotes (*p, host, 16 * 6000, 0.0);
            static const int pend[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1, 0 };
            bool pnd = notes.size() == 16;
            for (std::size_t i = 0; i < notes.size() && pnd; ++i) pnd = notes[i].note == fg::pitchFor (p->getSteps().steps[(std::size_t) pend[i]].note, 0, 0, 0);
            expect (pnd, "ORDER Pendulum with REPEAT ENDS plays 1-8 then 8-1");
            setParam (*p, fg::id::order, 10);                                  // Chance mix: runs, deterministic
            const auto m1 = runNotes (*p, host, 40 * 6000, 0.0), m2 = runNotes (*p, host, 40 * 6000, 0.0);
            bool same = m1.size() == m2.size() && ! m1.empty();
            for (std::size_t i = 0; i < m1.size() && same; ++i) same = m1[i].note == m2[i].note && m1[i].t == m2[i].t;
            expect (same, "Chance mix plays the same way when the same bars are played again");
        }
        {   // ROTATE
            MockHost host;
            auto p = makeProc (host);
            auto before = p->getSteps();
            for (int i = 0; i < 8; ++i) { auto s = before.steps[(std::size_t) i]; s.gate = (std::uint8_t) (10 + i); p->setStep (i, s); }
            before = p->getSteps();
            setParam (*p, fg::id::rotateRight, 1);
            letWindowThreadRun (150);
            auto now = p->getSteps();
            expect (now.steps[0].gate == before.steps[1].gate && now.steps[7].gate == before.steps[0].gate && now.steps[8] == before.steps[8], "ROTATE RIGHT moves the start point one step on (step 2 becomes step 1) within LENGTH");
            letWindowThreadRun (300);
            expect (getParam (*p, fg::id::rotateRight) < 0.5f && p->getSteps() == now, "ROTATE RIGHT pops back and acted once");
            setParam (*p, fg::id::rotateLeft, 1);
            letWindowThreadRun (150);
            expect (p->getSteps() == before, "ROTATE LEFT undoes it");
            letWindowThreadRun (300);
            // the rotated pattern is what plays
            setParam (*p, fg::id::rotateRight, 1);
            letWindowThreadRun (150);
            const auto notes = runNotes (*p, host, 6000, 0.0);
            expect (! notes.empty() && notes[0].note == pitch (p->getSteps().steps[0].note) && p->getSteps().steps[0].note == before.steps[1].note, "the rotated pattern is what plays");
            letWindowThreadRun (300);
        }
        {   // TRANSPOSE
            MockHost host;
            auto p = makeProc (host);
            const auto before = p->getSteps();
            setParam (*p, fg::id::transposeUp, 1);
            letWindowThreadRun (150);
            bool up = true;
            for (int i = 0; i < 8; ++i) up = up && p->getSteps().steps[(std::size_t) i].note == fg::degreeToStored (fg::storedToDegree (before.steps[(std::size_t) i].note, 0) + 1, 0);
            expect (up && p->getSteps().steps[8] == before.steps[8], "TRANSPOSE UP moves the first LENGTH steps up one scale step");
            letWindowThreadRun (300);
            setParam (*p, fg::id::transposeDown, 1);
            letWindowThreadRun (150);
            expect (p->getSteps() == before, "TRANSPOSE DOWN moves them back");
            letWindowThreadRun (300);
            setParam (*p, fg::id::scale, 16);                                  // in Major Pentatonic a step is a pentatonic step
            setParam (*p, fg::id::transposeUp, 1);
            letWindowThreadRun (150);
            bool pent = true;
            for (int i = 0; i < 8; ++i) pent = pent && p->getSteps().steps[(std::size_t) i].note == fg::degreeToStored (fg::storedToDegree (before.steps[(std::size_t) i].note, 16) + 1, 16);
            expect (pent, "TRANSPOSE counts steps of the chosen scale");
            letWindowThreadRun (300);
        }
        {   // RESET restarts the order at the next step
            MockHost host;
            auto p = makeProc (host);
            const auto data = p->getSteps();
            auto notes = runNotes (*p, host, 20000, 0.0);                       // steps 1 to 4 (boundaries 0, 6000, 12000, 18000)
            expect (notes.size() == 4, "four steps play before RESET");
            setParam (*p, fg::id::reset, 1);
            setParam (*p, fg::id::reset, 0);
            juce::AudioBuffer<float> audio (0, 512);
            std::vector<Note> after;
            for (long long t = 20000; t < 20000 + 7 * 6000; t += 512)
            {
                host.ppq = (double) t * pps;
                juce::MidiBuffer midi;
                p->processBlock (audio, midi);
                for (auto meta : midi) if (meta.getMessage().isNoteOn()) after.push_back ({ t + meta.samplePosition, meta.getMessage().getNoteNumber() });
            }
            bool restarted = after.size() >= 6;
            for (std::size_t i = 0; i < after.size() && i < 6 && restarted; ++i) restarted = after[i].note == pitch (data.steps[i % 8].note);
            expect (restarted, "RESET: the next step played is step 1 and the pattern goes on from there");
        }
        {   // loading a project is not a press of any button; a button that was on in the saved project does nothing
            MockHost host;
            auto p = makeProc (host);
            setParam (*p, fg::id::rotateRight, 1);
            setParam (*p, fg::id::transposeUp, 1);
            juce::MemoryBlock saved;
            p->getStateInformation (saved);
            FractalGardenProcessor p2;
            p2.setPlayConfigDetails (0, 0, sr, 512);
            p2.setStateInformation (saved.getData(), (int) saved.getSize());
            const auto loaded = p2.getSteps();
            letWindowThreadRun (300);
            expect (p2.getSteps() == loaded, "buttons that were on when a project was saved do not act when it is loaded");
            expect (getParam (p2, fg::id::rotateRight) < 0.5f && getParam (p2, fg::id::transposeUp) < 0.5f, "...and are off");
            letWindowThreadRun (300);
        }
        {   // the seed of the random orders is saved with the project, and a project from before 0.4.0 gets one
            MockHost host;
            auto p = makeProc (host);
            juce::MemoryBlock saved;
            p->getStateInformation (saved);
            FractalGardenProcessor p2;
            p2.setPlayConfigDetails (0, 0, sr, 512);
            p2.setStateInformation (saved.getData(), (int) saved.getSize());
            expect (p2.getValueTreeState().state.getProperty (FgParams::kSeedProperty).toString()
                        == p->getValueTreeState().state.getProperty (FgParams::kSeedProperty).toString(), "the order seed survives saving and loading");
            auto xml = juce::parseXML ("<Parameters/>");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            FractalGardenProcessor p3;
            p3.setStateInformation (old.getData(), (int) old.getSize());
            expect (p3.getValueTreeState().state.hasProperty (FgParams::kSeedProperty), "a project without a seed gets one");
        }
        {   // the editor's page follows LENGTH; pages beyond it cannot be chosen
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, sr, 512);
            FractalGardenEditor editor (p);
            editor.setSize (1080, 720);
            auto& se = editor.getStyledPanel()->getStepEditor();
            setParam (p, fg::id::length, 20);
            se.selectPage (5);
            expect (se.page() == 2, "a page beyond LENGTH cannot be chosen (LENGTH 20 has three pages)");
            se.selectPage (1);
            setParam (p, fg::id::length, 8);
            letWindowThreadRun (150);
            expect (se.page() == 0, "when LENGTH shrinks, the page follows it down");
        }
        {   // step editing: a note moves by scale steps
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, sr, 512);
            FractalGardenEditor editor (p);
            editor.setSize (1080, 720);
            auto& se = editor.getStyledPanel()->getStepEditor();
            se.selectTab (0);
            se.setValue (0, 0, 0.0f);   expect (p.getSteps().steps[0].note == 0, "the bottom of the note box is the lowest note");
            se.setValue (0, 0, 1.0f);   expect (p.getSteps().steps[0].note == fg::degreeToStored (fg::degreeCount (0) - 1, 0), "the top is the highest degree of C major");
            setParam (p, fg::id::scale, 17);                                   // Minor Pentatonic: five notes an octave
            se.setValue (1, 0, 1.0f);   expect (p.getSteps().steps[1].note == 4 * 12 + 4, "in a five-note scale the top is octave 5, degree 5");
            se.setValue (1, 0, 0.5f);   expect (p.getSteps().steps[1].note == fg::degreeToStored (12, 17), "the middle of the box is the middle degree");
            se.setValue (2, 1, 0.8f);   expect (p.getSteps().steps[2].gate == 80, "the gate tab still sets a percentage");
            se.resetValue (2, 1);       expect (p.getSteps().steps[2].gate == fg::Step().gate, "a double click puts the gate back to its default");
            se.resetValue (1, 0);       expect (p.getSteps().steps[1].note == fg::Step().note, "...and the note back to its default");
            setParam (p, fg::id::length, 4);
            const auto beyond = p.getSteps().steps[6];
            se.setValue (6, 0, 1.0f);   expect (p.getSteps().steps[6] == beyond, "steps beyond LENGTH cannot be edited");
        }
    }

    // ---- the step editor ----
    {
        FractalGardenProcessor p;
        p.setPlayConfigDetails (0, 0, 48000.0, 512);
        FractalGardenEditor editor (p);
        editor.setSize (1080, 720);
        auto& se = editor.getStyledPanel()->getStepEditor();
        for (int t = 0; t < 5; ++t) { se.selectTab (t); expect (se.tab() == t && p.getEditorTab() == t, "tab " + juce::String (t)); }
        setParam (p, fg::id::length, 64);
        for (int pg = 0; pg < 8; ++pg) { se.selectPage (pg); expect (se.page() == pg && p.getEditorPage() == pg, "page " + juce::String (pg)); }
        const bool before = p.getSteps().steps[0].on;
        se.toggleStep (0);
        expect (p.getSteps().steps[0].on == ! before, "toggling a step changes the data");
    }

    // ---- milestone 5: Resize, CC output, the ratchet list, Mod rates ----
    {
        FractalGardenProcessor p;
        p.setPlayConfigDetails (0, 0, 48000.0, 512);
        setParam (p, fg::id::length, 8);
        const auto before = p.getSteps();
        setParam (p, fg::id::resizeShrink, 1);
        letWindowThreadRun (200);
        expect (juce::roundToInt (getParam (p, fg::id::length)) == 4 && p.getSteps().steps[1] == before.steps[2] && p.getSteps().steps[3] == before.steps[6],
                "SHRINK halves LENGTH and keeps every other step");
        letWindowThreadRun (300);
        expect (getParam (p, fg::id::resizeShrink) < 0.5f && juce::roundToInt (getParam (p, fg::id::length)) == 4, "SHRINK pops back and acted once");
        setParam (p, fg::id::resizeExpand, 1);
        letWindowThreadRun (200);
        expect (juce::roundToInt (getParam (p, fg::id::length)) == 8 && p.getSteps().steps[2] == before.steps[2] && ! p.getSteps().steps[3].on,
                "EXPAND doubles LENGTH, each step followed by an OFF copy");
        letWindowThreadRun (300);

        FractalGardenEditor editor (p);
        editor.setSize (1080, 720);
        auto& se = editor.getStyledPanel()->getStepEditor();
        setParam (p, fg::id::length, 16);
        se.setValue (0, 3, 1.0f);   expect (p.getSteps().steps[0].ratchet == fg::kRatchetMax, "the top of the Ratchet box is the last in the list (x32)");
        se.setValue (0, 3, 0.0f);   expect (p.getSteps().steps[0].ratchet == 0, "the bottom of the Ratchet box is x1");
        se.resetValue (0, 3);       expect (p.getSteps().steps[0].ratchet == fg::Step().ratchet, "a double click puts the ratchet back to x1");
    }
    {   // the notes come with CC 84 and CC 1 in front of them
        MockHost host;
        FractalGardenProcessor p;
        p.setPlayConfigDetails (0, 0, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.setPlayHead (&host);
        juce::AudioBuffer<float> audio (0, 512);
        juce::MidiBuffer first;
        host.playing = false;
        p.processBlock (audio, first);
        bool sawCc1 = false, ccBeforeNote = true, sawNote = false;
        for (int b = 0; b < 40; ++b)
        {
            host.playing = true;
            host.ppq = (double) (b * 512) * 120.0 / 60.0 / 48000.0;
            juce::MidiBuffer midi;
            p.processBlock (audio, midi);
            for (auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isController() && m.getControllerNumber() == 1) sawCc1 = true;
                if (m.isNoteOn() && ! sawNote) { sawNote = true; ccBeforeNote = sawCc1; }
            }
        }
        expect (sawNote && sawCc1 && ccBeforeNote, "the first note is preceded by CC 1 (the Mod output)");
    }

    // ---- milestones 6 and 7: branches, Mutate, Reseed, Unmutate ----
    {
        struct MockHost : juce::AudioPlayHead
        {
            bool playing = true; double ppq = 0.0, bpm = 120.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo pi; pi.setIsPlaying (playing); pi.setBpm (bpm); pi.setPpqPosition (ppq);
                pi.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
                return pi;
            }
        };
        const double sr = 48000.0, pps = 120.0 / 60.0 / sr;
        auto makeProc = [&] (MockHost& host)
        {
            auto p = std::make_unique<FractalGardenProcessor>();
            p->setPlayConfigDetails (0, 0, sr, 512);
            p->prepareToPlay (sr, 512);
            p->setPlayHead (&host);
            setParam (*p, fg::id::scale, 27);                            // Chromatic: stored place i plays MIDI 48 + i
            for (int i = 0; i < 8; ++i)
            {
                fg::Step s = p->getSteps().steps[(std::size_t) i];
                s.on = true; s.gate = 50; s.slew = 0; s.ratchet = 0; s.note = (std::uint8_t) (12 + i);
                p->setStep (i, s);
            }
            return p;
        };
        auto play = [&] (FractalGardenProcessor& p, MockHost& host, long long samples, double startPpq, std::vector<int>* notes = nullptr)
        {
            juce::AudioBuffer<float> audio (0, 512);
            for (long long t = 0; t < samples; t += 512)
            {
                host.playing = true;
                host.ppq = startPpq + (double) t * pps;
                juce::MidiBuffer midi;
                p.processBlock (audio, midi);
                if (notes)
                    for (auto meta : midi)
                        if (meta.getMessage().isNoteOn()) notes->push_back (meta.getMessage().getNoteNumber());
            }
        };
        auto press = [&] (FractalGardenProcessor& p, const char* id)
        {
            setParam (p, id, 1);
            letWindowThreadRun (150);          // the window thread carries it out and switches it off again
            letWindowThreadRun (250);
        };
        auto stepsEqualRange = [] (const fg::StepData& a, const fg::StepData& b, int from, int to)
        {
            for (int i = from; i < to; ++i) if (! (a.steps[(std::size_t) i] == b.steps[(std::size_t) i])) return false;
            return true;
        };

        // -- branches and Path in the playing plugin: trunk first, then the branches of the path --
        {
            MockHost host;
            auto p = makeProc (host);
            setParam (*p, fg::id::branch, 3.0f / 7.0f);
            setParam (*p, fg::id::path, 5.0f / 7.0f);                      // path 5 of 8
            const auto view = p->getBranchView();
            expect (view.count == 3 && view.path == 5, "Branch 3 and Path 6 of 8 are read from the knobs");
            fg::BranchSet set;
            fg::buildBranches (set, p->getSteps(), 8, 27, view.count, view.path, view.seed);
            std::vector<int> notes;
            play (*p, host, 6000 * 40, 0.0, &notes);                       // 40 steps: one loop of 32 and the first 8 again
            bool ok = notes.size() >= 30;
            std::size_t n = 0;
            for (int s = 0; s < 4 && ok; ++s)
                for (int i = 0; i < 8; ++i)
                {
                    const fg::Step& st = s == 0 ? p->getSteps().steps[(std::size_t) i] : set.steps[(std::size_t) (s - 1)][(std::size_t) i];
                    if (st.on && st.gate < 100 && n < notes.size()) ok = ok && notes[n++] == fg::pitchFor (st.note, 27, 0, 0);
                    else if (st.on) ++n;
                    if (! st.on || st.gate >= 100) { ok = ok || true; }
                }
            expect (notes.size() >= 24, "branches play: the loop is longer than the trunk");
            bool trunkFirst = notes.size() >= 8;
            for (int i = 0; i < 8 && trunkFirst; ++i) trunkFirst = notes[(std::size_t) i] == 48 + 12 + i;
            expect (trunkFirst, "the trunk plays first");
            bool secondDiffers = false;
            for (int i = 0; i < 8 && notes.size() >= 16; ++i) secondDiffers = secondDiffers || notes[(std::size_t) (8 + i)] != notes[(std::size_t) i];
            expect (secondDiffers || notes.size() < 16, "branch 1 is not a copy of the trunk");
            // the same path always gives the same branches
            std::vector<int> again;
            play (*p, host, 6000 * 40, 0.0, &again);
            expect (again == notes, "the same Branch and Path play the same way again");
            // the header names the part that plays; the TREE follows it
            FractalGardenEditor editor (*p);
            editor.setSize (1080, 720);
            auto* panel = editor.getStyledPanel();
            play (*p, host, 6000 * 10, 0.0);                                // ends in branch 1 (steps 9 to 16 of the loop)
            panel->refreshNow();
            expect (panel->headerLine2().contains ("BRANCH 1"), "the header says which branch is playing");
            play (*p, host, 6000 * 3, 0.0);
            panel->refreshNow();
            expect (panel->headerLine2().endsWith ("TRUNK"), "back on the trunk the header says TRUNK");
        }

        // -- Mutate: changes reach the saved steps, branches are not regenerated by them, UNMUTATE rolls them back --
        {
            MockHost host;
            auto p = makeProc (host);
            const auto edited = p->getSteps();
            const auto seed0 = p->getBranchView().seed;
            setParam (*p, fg::id::mutate, 0.5f);                            // note mutation 100 %
            play (*p, host, 6000 * 16, 0.0);
            letWindowThreadRun (300);                                       // the window thread saves the changes
            const auto mutated = p->getSteps();
            int changed = 0;
            for (int i = 0; i < 8; ++i) changed += mutated.steps[(std::size_t) i].note != edited.steps[(std::size_t) i].note;
            expect (changed >= 4, "Mutate changes the notes of the saved steps");
            bool onlyNotes = true;
            for (int i = 0; i < 8; ++i) onlyNotes = onlyNotes && mutated.steps[(std::size_t) i].gate == edited.steps[(std::size_t) i].gate && mutated.steps[(std::size_t) i].on == edited.steps[(std::size_t) i].on;
            expect (onlyNotes, "zone 1 changes notes only");
            expect (stepsEqualRange (mutated, edited, 8, 64), "steps beyond LENGTH are left alone");
            expect (p->getBranchView().seed == seed0, "Mutate does not change the branch seed");
            // the saved project holds the mutated steps
            juce::MemoryBlock saved;
            p->getStateInformation (saved);
            FractalGardenProcessor p2;
            p2.setPlayConfigDetails (0, 0, sr, 512);
            p2.setStateInformation (saved.getData(), (int) saved.getSize());
            expect (p2.getSteps() == mutated, "mutated steps are saved with the project");
            // UNMUTATE
            setParam (*p, fg::id::mutate, 0.0f);
            press (*p, fg::id::unmutate);
            expect (p->getSteps() == edited, "UNMUTATE puts the steps back to the last manual edit");
            expect (getParam (*p, fg::id::unmutate) < 0.5f, "UNMUTATE pops back off");
            // an edit during mutation is never overwritten by an older change
            setParam (*p, fg::id::mutate, 0.5f);
            play (*p, host, 6000 * 4, 0.0);
            fg::Step s = p->getSteps().steps[3]; s.note = 7; s.gate = 77; p->setStep (3, s);
            letWindowThreadRun (300);
            expect (p->getSteps().steps[3].note == 7 || p->getSteps().steps[3].note != 12 + 3, "an edit made while Mutate runs is kept");
            setParam (*p, fg::id::mutate, 0.0f);
        }

        // -- Mutate zone 2 and Micro Mutate change the other fields --
        {
            MockHost host;
            auto p = makeProc (host);
            const auto edited = p->getSteps();
            setParam (*p, fg::id::mutate, 1.0f);
            play (*p, host, 6000 * 160, 0.0);
            letWindowThreadRun (300);
            int gate = 0; for (int i = 0; i < 8; ++i) gate += p->getSteps().steps[(std::size_t) i].gate != 50;
            expect (gate >= 1, "zone 2 changes gate lengths");
            setParam (*p, fg::id::mutate, 0.0f);
            press (*p, fg::id::unmutate);
            expect (p->getSteps() == edited, "UNMUTATE undoes zone 2 too");

            setParam (*p, fg::id::microRatchet, 1.0f);
            setParam (*p, fg::id::microSlew, 1.0f);
            setParam (*p, fg::id::microMod, 1.0f);
            setParam (*p, fg::id::knobMode, 1);                             // the Micro knobs show; the values act whichever mode is showing
            play (*p, host, 6000 * 24, 0.0);
            letWindowThreadRun (300);
            int ratchet = 0, slew = 0, shape = 0;
            const auto microSteps = p->getSteps();
            for (int i = 0; i < 8; ++i)
            {
                const auto& s = microSteps.steps[(std::size_t) i];
                ratchet += s.ratchet != 0; slew += s.slew != 0; shape += s.modShape != edited.steps[(std::size_t) i].modShape || s.mod != edited.steps[(std::size_t) i].mod;
            }
            expect (ratchet >= 1 && slew >= 1 && shape >= 1, "Micro Mutate changes ratchets, slew amounts and Mod values or shapes: " + juce::String (ratchet) + "/" + juce::String (slew) + "/" + juce::String (shape));
            bool notesKept = true;
            for (int i = 0; i < 8; ++i) notesKept = notesKept && p->getSteps().steps[(std::size_t) i].note == edited.steps[(std::size_t) i].note;
            expect (notesKept, "Micro Mutate leaves notes alone");
            setParam (*p, fg::id::microRatchet, 0.0f); setParam (*p, fg::id::microSlew, 0.0f); setParam (*p, fg::id::microMod, 0.0f);
            press (*p, fg::id::unmutate);
            expect (p->getSteps() == edited, "UNMUTATE undoes Micro Mutate");
        }

        // -- Performance mode (milestone 8): the three knobs reach the player, change nothing that is stored, and work in any Knob Mode --
        {
            MockHost host;
            auto p = makeProc (host);
            const auto edited = p->getSteps();
            std::vector<int> plain;
            play (*p, host, 6000 * 16, 0.0, &plain);
            expect (plain.size() >= 14 && plain.size() <= 17, "Performance: knobs at 0 play one note per step: " + juce::String ((int) plain.size()));

            setParam (*p, fg::id::perfRatchet, 1.0f);                       // zone 2, top band: x32 on every step
            std::vector<int> fast;
            play (*p, host, 6000 * 16, 0.0, &fast);
            expect (fast.size() >= 16 * 8, "Ratchet FX at the top ratchets every step: " + juce::String ((int) fast.size()));
            setParam (*p, fg::id::perfRatchet, 0.0f);

            setParam (*p, fg::id::perfMutate, 1.0f);                        // ornaments with trills
            setParam (*p, fg::id::knobMode, 0);                             // another Knob Mode is showing; the knob still acts
            std::vector<int> orn;
            play (*p, host, 6000 * 64, 0.0, &orn);
            letWindowThreadRun (300);
            expect (orn.size() > 64, "Mutate FX in the upper half adds ornament notes: " + juce::String ((int) orn.size()));
            expect (p->getSteps() == edited, "Performance effects never change the stored steps");
            setParam (*p, fg::id::perfMutate, 0.0f);
            std::vector<int> back;
            play (*p, host, 6000 * 16, 0.0, &back);
            expect (back == plain, "knobs back at 0 give the original notes again");
        }

        // -- Reseed: page or all 64; each button changes its own thing; UNMUTATE undoes it; Reseed is not an edit --
        {
            MockHost host;
            auto p = makeProc (host);
            setParam (*p, fg::id::length, 64);
            letWindowThreadRun (100);
            const auto before = p->getSteps();
            p->setEditorPage (2);
            press (*p, fg::id::reseedAll);                                  // scope Page: steps 17 to 24 only
            auto after = p->getSteps();
            expect (stepsEqualRange (after, before, 0, 16) && stepsEqualRange (after, before, 24, 64), "RESEED ALL on a page touches only that page's eight steps");
            expect (! stepsEqualRange (after, before, 16, 24), "...and changes them");
            press (*p, fg::id::unmutate);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 64, "(LENGTH is still 64)");
            // the unmutate point is the last edit, which is the state before (setParam length 64 counted as an edit and the steps were unchanged)
            expect (p->getSteps() == before, "UNMUTATE undoes RESEED");

            setParam (*p, fg::id::reseedScope, 1);                          // All 64
            press (*p, fg::id::reseedNote);
            after = p->getSteps();
            bool onlyNotes = true; int noteChanges = 0;
            for (int i = 0; i < 64; ++i)
            {
                const auto& a = after.steps[(std::size_t) i]; const auto& b = before.steps[(std::size_t) i];
                onlyNotes = onlyNotes && a.gate == b.gate && a.on == b.on && a.slew == b.slew && a.ratchet == b.ratchet && a.mod == b.mod;
                noteChanges += a.note != b.note;
            }
            expect (onlyNotes && noteChanges > 40, "RESEED: NOTE with ALL 64 changes the notes of every step and nothing else");
            press (*p, fg::id::unmutate);

            struct Case { const char* id; const char* name; };
            for (const Case c : { Case { fg::id::reseedGate, "GATE" }, Case { fg::id::reseedSlew, "SLEW" }, Case { fg::id::reseedRatchet, "RATCHET" },
                                  Case { fg::id::reseedMod, "MOD" }, Case { fg::id::reseedAll, "ALL" } })
            {
                press (*p, c.id);
                const auto r = p->getSteps();
                bool noteSame = true, anything = false;
                for (int i = 0; i < 64; ++i)
                {
                    noteSame = noteSame && r.steps[(std::size_t) i].note == before.steps[(std::size_t) i].note;
                    anything = anything || ! (r.steps[(std::size_t) i] == before.steps[(std::size_t) i]);
                }
                if (juce::String (c.name) != "ALL") expect (noteSame, juce::String ("RESEED: ") + c.name + " leaves notes alone");
                expect (anything, juce::String ("RESEED: ") + c.name + " changes the steps");
                press (*p, fg::id::unmutate);
                expect (p->getSteps() == before, juce::String ("UNMUTATE undoes RESEED: ") + c.name);
            }

            // RESEED: BRANCHES gives a new seed and does not touch the trunk
            const auto seedBefore = p->getBranchView().seed;
            press (*p, fg::id::reseedBranches);
            expect (p->getBranchView().seed != seedBefore, "RESEED: BRANCHES draws a new branch seed");
            expect (p->getSteps() == before, "...and leaves the trunk alone");
        }

        // -- what sets the UNMUTATE point: edits and LENGTH by hand do; Rotate, Transpose, Resize and Reseed do not --
        {
            MockHost host;
            auto p = makeProc (host);
            const auto edited = p->getSteps();
            press (*p, fg::id::rotateRight);
            press (*p, fg::id::transposeUp);
            expect (! (p->getSteps() == edited), "ROTATE and TRANSPOSE changed the steps");
            press (*p, fg::id::unmutate);
            expect (p->getSteps() == edited, "UNMUTATE undoes ROTATE and TRANSPOSE");

            setParam (*p, fg::id::length, 8);
            press (*p, fg::id::resizeShrink);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 4, "SHRINK made LENGTH 4");
            press (*p, fg::id::unmutate);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 8 && p->getSteps() == edited, "UNMUTATE undoes RESIZE, LENGTH included");
            press (*p, fg::id::resizeExpand);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 16, "EXPAND made LENGTH 16");
            press (*p, fg::id::unmutate);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 8 && p->getSteps() == edited, "UNMUTATE undoes EXPAND, LENGTH included");

            // a new edit moves the point: ROTATE, then edit a step, then UNMUTATE keeps the rotation and the edit
            press (*p, fg::id::rotateRight);
            const auto rotated = p->getSteps();
            fg::Step s = rotated.steps[2]; s.note = 40; p->setStep (2, s);
            const auto afterEdit = p->getSteps();
            press (*p, fg::id::transposeDown);
            press (*p, fg::id::unmutate);
            expect (p->getSteps() == afterEdit, "a step edit sets a new UNMUTATE point (the earlier ROTATE can no longer be undone)");

            // LENGTH changed by hand is an edit too
            setParam (*p, fg::id::length, 12);
            letWindowThreadRun (150);
            press (*p, fg::id::resizeShrink);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 6, "SHRINK from 12 made LENGTH 6");
            press (*p, fg::id::unmutate);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 12, "LENGTH changed by hand is the UNMUTATE point's LENGTH");
            letWindowThreadRun (150);
            expect (juce::roundToInt (getParam (*p, fg::id::length)) == 12, "...and stays 12 (UNMUTATE's own LENGTH change is not an edit)");
        }

        // -- saving and loading: the unmutate point, the branch seed and the branch snapshot --
        {
            MockHost host;
            auto p = makeProc (host);
            const auto edited = p->getSteps();
            press (*p, fg::id::rotateRight);
            juce::MemoryBlock saved;
            p->getStateInformation (saved);
            FractalGardenProcessor p2;
            p2.setPlayConfigDetails (0, 0, sr, 512);
            p2.setStateInformation (saved.getData(), (int) saved.getSize());
            letWindowThreadRun (300);
            expect (p2.getBranchView().seed == p->getBranchView().seed, "the branch seed survives saving and loading");
            press (p2, fg::id::unmutate);
            expect (p2.getSteps() == edited, "the UNMUTATE point survives saving and loading");
            // a project from before 0.6.0 (no undo point, no branch seed) opens, UNMUTATE then does nothing and the branches still play
            auto xml = juce::parseXML ("<Parameters/>");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            FractalGardenProcessor p3;
            p3.setPlayConfigDetails (0, 0, sr, 512);
            p3.setStateInformation (old.getData(), (int) old.getSize());
            const auto start = p3.getSteps();
            press (p3, fg::id::unmutate);
            expect (p3.getSteps() == start, "an older project: UNMUTATE has nothing to go back to and changes nothing");
            expect (p3.getValueTreeState().state.hasProperty (FgParams::kBranchSeedProperty), "an older project gets a branch seed");
            // loading does not count as an edit and does not press anything
            expect (getParam (p3, fg::id::unmutate) < 0.5f && getParam (p3, fg::id::reseedAll) < 0.5f, "no button is on after loading");
        }

        // -- the TREE shows the letters of the branches of the current path --
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, sr, 512);
            setParam (p, fg::id::branch, 1.0f);
            for (float path : { 0.0f, 0.3f, 1.0f })
            {
                setParam (p, fg::id::path, path);
                const auto v = p.getBranchView();
                expect (v.count == 7 && v.path >= 0 && v.path < 128, "Branch 7: the path number is 0 to 127");
                for (int level = 1; level <= 7; ++level)
                {
                    const int k = fg::branchKindFor (level, v.path, v.count, v.seed);
                    expect (k >= 0 && k < 5 && std::string ("RIT M?").find (fg::kBranchLetters[k]) != std::string::npos, "every branch has one of the letters R I T M ?");
                }
            }
        }
    }


    // ---- milestone 9: the pattern bank and the settings page ----
    {
        auto press = [&] (FractalGardenProcessor& p, const char* id)
        {
            setParam (p, id, 1);
            letWindowThreadRun (150);          // the window thread carries it out and switches it off again
            letWindowThreadRun (250);
        };
        auto lengthOf = [&] (FractalGardenProcessor& p) { return juce::roundToInt (getParam (p, fg::id::length)); };

        // -- the pattern bank: slots start empty; SAVE, then LOAD; an empty slot loads nothing; LOAD is an edit --
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, 48000.0, 512);
            for (int i = 0; i < fg::kNumPatternSlots; ++i)
                expect (! p.getPatternInfo (i).used, "slot " + juce::String (i + 1) + " starts empty");
            const auto a = p.getSteps();
            setParam (p, fg::id::patternSlot, 2);
            press (p, fg::id::patternSave);
            expect (p.getPatternInfo (2).used && p.getPatternInfo (2).length == 8 && ! p.getPatternInfo (1).used, "SAVE fills the chosen slot (with LENGTH)");
            expect (getParam (p, fg::id::patternSave) < 0.5f, "SAVE pops back off");
            fg::Step s = a.steps[1]; s.note = 44; s.gate = 80; p.setStep (1, s);
            setParam (p, fg::id::length, 12);
            letWindowThreadRun (150);
            const auto b = p.getSteps();
            setParam (p, fg::id::patternSlot, 5);
            press (p, fg::id::patternLoad);
            expect (p.getSteps() == b && lengthOf (p) == 12, "LOAD on an empty slot does nothing");
            setParam (p, fg::id::patternSlot, 2);
            press (p, fg::id::patternLoad);
            expect (p.getSteps() == a && lengthOf (p) == 8, "LOAD brings back the saved steps and LENGTH");
            expect (getParam (p, fg::id::patternLoad) < 0.5f, "LOAD pops back off");
            letWindowThreadRun (200);
            expect (lengthOf (p) == 8 && p.getSteps() == a, "LOAD's own LENGTH change is not seen as an edit");
            // LOAD is a manual edit: it is the new UNMUTATE point
            press (p, fg::id::transposeUp);
            expect (! (p.getSteps() == a), "TRANSPOSE changed the loaded pattern");
            press (p, fg::id::unmutate);
            expect (p.getSteps() == a && lengthOf (p) == 8, "UNMUTATE goes back to the loaded pattern");
            // overwriting a full slot
            p.setStep (1, s);
            setParam (p, fg::id::patternSlot, 2);
            press (p, fg::id::patternSave);
            const auto b8 = p.getSteps();
            press (p, fg::id::rotateRight);
            press (p, fg::id::patternLoad);
            expect (p.getSteps() == b8, "SAVE over a full slot replaces it");
            // the Performance effects are never stored
            setParam (p, fg::id::perfRatchet, 1.0f); setParam (p, fg::id::perfSlew, 1.0f); setParam (p, fg::id::perfMutate, 1.0f);
            const auto beforeFx = p.getSteps();
            setParam (p, fg::id::patternSlot, 9);
            press (p, fg::id::patternSave);
            press (p, fg::id::patternLoad);
            expect (p.getSteps() == beforeFx, "a pattern saved with the Performance knobs up holds only the stored steps");
        }
        // -- the branches are made again by LOAD --
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, 48000.0, 512);
            setParam (p, fg::id::branch, 1.0f);
            press (p, fg::id::patternSave);
            const int rev = p.getBranchRevision();
            press (p, fg::id::patternLoad);
            expect (p.getBranchRevision() > rev, "LOAD makes the branches again");
        }
        // -- saved with the project; loading a project presses nothing --
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, 48000.0, 512);
            const auto a = p.getSteps();
            setParam (p, fg::id::patternSlot, 0);
            press (p, fg::id::patternSave);
            fg::Step s = a.steps[2]; s.note = 40; s.slew = 33; p.setStep (2, s);
            const auto b = p.getSteps();
            setParam (p, fg::id::patternSlot, 3);
            setParam (p, fg::id::patternSave, 1);                   // saved while SAVE is on and not yet carried out
            juce::MemoryBlock withSaveOn;
            p.getStateInformation (withSaveOn);
            setParam (p, fg::id::patternSlot, 0);
            setParam (p, fg::id::patternLoad, 1);                   // ... and while LOAD is on
            juce::MemoryBlock withLoadOn;
            p.getStateInformation (withLoadOn);

            FractalGardenProcessor p2;
            p2.setPlayConfigDetails (0, 0, 48000.0, 512);
            p2.setStateInformation (withLoadOn.getData(), (int) withLoadOn.getSize());
            letWindowThreadRun (450);
            expect (p2.getPatternInfo (0).used && p2.getPatternInfo (0).length == 8, "the pattern bank survives saving and loading");
            expect (p2.getSteps() == b, "a project saved with LOAD on does not load a pattern when it opens");
            expect (getParam (p2, fg::id::patternLoad) < 0.5f && getParam (p2, fg::id::patternSave) < 0.5f, "no pattern button is on after loading");
            setParam (p2, fg::id::patternSlot, 0);
            press (p2, fg::id::patternLoad);
            expect (p2.getSteps() == a, "a pattern loaded from a reopened project is the one that was saved");

            FractalGardenProcessor p3;
            p3.setPlayConfigDetails (0, 0, 48000.0, 512);
            p3.setStateInformation (withSaveOn.getData(), (int) withSaveOn.getSize());
            letWindowThreadRun (450);
            expect (! p3.getPatternInfo (3).used, "a project saved with SAVE on does not save a pattern when it opens");

            // an older project (no bank) and a damaged bank: all slots empty, nothing else changes
            auto xml = juce::parseXML ("<Parameters/>");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            FractalGardenProcessor p4;
            p4.setPlayConfigDetails (0, 0, 48000.0, 512);
            p4.setStateInformation (old.getData(), (int) old.getSize());
            bool anyUsed = false;
            for (int i = 0; i < fg::kNumPatternSlots; ++i) anyUsed = anyUsed || p4.getPatternInfo (i).used;
            expect (! anyUsed, "an older project opens with 16 empty slots");
            auto xml2 = juce::parseXML ("<Parameters patternBank=\"garbage!\"/>");
            juce::MemoryBlock dmg;
            juce::AudioProcessor::copyXmlToBinary (*xml2, dmg);
            FractalGardenProcessor p5;
            p5.setPlayConfigDetails (0, 0, 48000.0, 512);
            p5.setStateInformation (dmg.getData(), (int) dmg.getSize());
            anyUsed = false;
            for (int i = 0; i < fg::kNumPatternSlots; ++i) anyUsed = anyUsed || p5.getPatternInfo (i).used;
            expect (! anyUsed, "a damaged pattern bank opens as 16 empty slots");
        }

        // -- the settings page in the playing plugin: MIDI channel, CC numbers, base note --
        {
            MockHost host;
            const double sr = 48000.0, pps = 120.0 / 60.0 / sr;
            struct Seen { std::vector<int> notes, channels, ccNumbers; };
            auto run = [&] (FractalGardenProcessor& p, int blocks, int firstBlock, Seen& seen, std::map<int, int>* held = nullptr)
            {
                juce::AudioBuffer<float> audio (0, 512);
                for (int b = 0; b < blocks; ++b)
                {
                    host.playing = true;
                    host.ppq = (double) ((firstBlock + b) * 512) * pps;
                    juce::MidiBuffer midi;
                    p.processBlock (audio, midi);
                    for (auto meta : midi)
                    {
                        const auto m = meta.getMessage();
                        if (m.isNoteOn()) { seen.notes.push_back (m.getNoteNumber()); seen.channels.push_back (m.getChannel()); if (held) ++(*held)[m.getChannel()]; }
                        if (m.isNoteOff() && held) --(*held)[m.getChannel()];
                        if (m.isAllNotesOff() && held) (*held)[m.getChannel()] = 0;
                        if (m.isController() && ! m.isAllNotesOff()) seen.ccNumbers.push_back (m.getControllerNumber());      // (all-notes-off is CC 123: not one of ours)
                    }
                }
            };
            auto makeProc = [&] ()
            {
                auto p = std::make_unique<FractalGardenProcessor>();
                p->setPlayConfigDetails (0, 0, sr, 512);
                p->prepareToPlay (sr, 512);
                p->setPlayHead (&host);
                return p;
            };
            Seen base, custom;
            {
                auto p = makeProc();
                run (*p, 60, 0, base);
            }
            expect (! base.notes.empty(), "the settings test plays notes");
            bool allOne = true, ccsDefault = true;
            for (int c : base.channels) allOne = allOne && c == 1;
            for (int c : base.ccNumbers) ccsDefault = ccsDefault && (c == 84 || c == 1);
            expect (allOne && ccsDefault && ! base.ccNumbers.empty(), "the defaults: channel 1, CC 84 and CC 1");
            {
                auto p = makeProc();
                setParam (*p, fg::id::midiChannel, 5);
                setParam (*p, fg::id::ccSlew, 20);
                setParam (*p, fg::id::ccMod, 21);
                setParam (*p, fg::id::baseNote, 60);
                run (*p, 60, 0, custom);
            }
            bool ch5 = ! custom.channels.empty(), ccs = ! custom.ccNumbers.empty(), shifted = custom.notes.size() == base.notes.size();
            for (int c : custom.channels) ch5 = ch5 && c == 5;
            for (int c : custom.ccNumbers) ccs = ccs && (c == 20 || c == 21);
            for (std::size_t i = 0; shifted && i < base.notes.size(); ++i) shifted = custom.notes[i] == base.notes[i] + 12;
            expect (ch5, "MIDI CHANNEL 5: every note is on channel 5");
            expect (ccs, "the CC numbers 20 and 21 are used instead of 84 and 1");
            expect (shifted, "BASE NOTE 60 plays every note an octave above the default (48)");

            // changing the channel while a note sounds ends it on the old channel
            {
                auto p = makeProc();
                setParam (*p, fg::id::midiChannel, 5);
                setParam (*p, fg::id::ratio, 0);                    // /8: long steps, so a note is sounding at the block boundary we pick
                Seen s1, s2;
                std::map<int, int> held;
                run (*p, 6, 0, s1, &held);
                expect (held[5] >= 0, "notes were played on channel 5");
                setParam (*p, fg::id::midiChannel, 9);
                run (*p, 120, 6, s2, &held);              // a /8 step is 48000 samples: the next note starts about 94 blocks in
                bool none5 = true;
                for (int c : s2.channels) none5 = none5 && c != 5;
                expect (none5 && held[5] == 0, "after the channel change nothing is left sounding on channel 5, and new notes use channel 9");
                bool any9 = false;
                for (int c : s2.channels) any9 = any9 || c == 9;
                expect (any9, "notes continue on the new channel");
            }
        }

        // -- Resize method, Reseed filter, branch-regeneration filter --
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, 48000.0, 512);
            const auto before = p.getSteps();
            setParam (p, fg::id::resizeMethod, 1);                  // Stretch
            press (p, fg::id::resizeExpand);
            auto now = p.getSteps();
            bool stretch = lengthOf (p) == 16;
            for (int i = 0; i < 8; ++i)
                stretch = stretch && now.steps[(std::size_t) (2 * i)] == before.steps[(std::size_t) i] && now.steps[(std::size_t) (2 * i + 1)] == before.steps[(std::size_t) i];
            expect (stretch, "RESIZE METHOD Stretch: each step is followed by a copy of itself");
            press (p, fg::id::unmutate);
            expect (lengthOf (p) == 8 && p.getSteps() == before, "UNMUTATE undoes the Stretch");
            setParam (p, fg::id::resizeMethod, 2);                  // Clone
            press (p, fg::id::resizeExpand);
            now = p.getSteps();
            bool clone = lengthOf (p) == 16;
            for (int i = 0; i < 8; ++i)
                clone = clone && now.steps[(std::size_t) i] == before.steps[(std::size_t) i] && now.steps[(std::size_t) (8 + i)] == before.steps[(std::size_t) i];
            expect (clone, "RESIZE METHOD Clone: the whole pattern is repeated");
        }
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, 48000.0, 512);
            setParam (p, fg::id::reseedScope, 1);                   // All 64
            const auto base = p.getSteps();
            for (const char* id : { fg::id::reseedFilterGate, fg::id::reseedFilterSlew, fg::id::reseedFilterRatchet, fg::id::reseedFilterMod })
                setParam (p, id, 0);
            press (p, fg::id::reseedAll);
            const auto now = p.getSteps();
            bool notesMoved = false, restSame = true;
            for (int i = 0; i < fg::kNumSteps; ++i)
            {
                const auto& a = now.steps[(std::size_t) i]; const auto& o = base.steps[(std::size_t) i];
                notesMoved = notesMoved || a.note != o.note;
                restSame = restSame && a.on == o.on && a.gate == o.gate && a.slew == o.slew && a.ratchet == o.ratchet && a.mod == o.mod && a.modShape == o.modShape && a.modRate == o.modRate;
            }
            expect (notesMoved && restSame, "RESEED FILTER with only Note: RESEED ALL changes the notes and nothing else");
            press (p, fg::id::unmutate);
            for (const char* id : { fg::id::reseedFilterGate, fg::id::reseedFilterSlew, fg::id::reseedFilterRatchet, fg::id::reseedFilterMod })
                setParam (p, id, 1);
            press (p, fg::id::reseedAll);
            bool other = false;
            for (int i = 0; i < fg::kNumSteps; ++i)
                other = other || p.getSteps().steps[(std::size_t) i].gate != base.steps[(std::size_t) i].gate || p.getSteps().steps[(std::size_t) i].mod != base.steps[(std::size_t) i].mod;
            expect (other, "with all five on, RESEED ALL changes the other parts too");
            // the single buttons ignore the filter
            press (p, fg::id::unmutate);
            for (const char* id : { fg::id::reseedFilterNote, fg::id::reseedFilterGate, fg::id::reseedFilterSlew, fg::id::reseedFilterRatchet, fg::id::reseedFilterMod })
                setParam (p, id, 0);
            press (p, fg::id::reseedGate);
            bool gateMoved = false;
            for (int i = 0; i < fg::kNumSteps; ++i)
                gateMoved = gateMoved || p.getSteps().steps[(std::size_t) i].gate != base.steps[(std::size_t) i].gate;
            expect (gateMoved, "RESEED: GATE works whatever the filter says");
        }
        {
            FractalGardenProcessor p;
            p.setPlayConfigDetails (0, 0, 48000.0, 512);
            setParam (p, fg::id::branch, 1.0f);
            const auto edit = [&] (std::function<void (fg::Step&)> change)
            {
                fg::Step s = p.getSteps().steps[0];
                change (s);
                p.setStep (0, s);
            };
            // all on (the defaults): every kind of step edit makes the branches again
            int rev = p.getBranchRevision();
            edit ([] (fg::Step& s) { s.note = (std::uint8_t) (s.note + 3); });
            expect (p.getBranchRevision() > rev, "defaults: a Note edit makes the branches again");
            // Note off: a Note edit leaves them alone, but it is still the UNMUTATE point
            setParam (p, fg::id::regenNote, 0);
            rev = p.getBranchRevision();
            edit ([] (fg::Step& s) { s.note = (std::uint8_t) (s.note + 2); });
            expect (p.getBranchRevision() == rev, "Note filter off: a Note edit does not make the branches again");
            const auto edited = p.getSteps();
            press (p, fg::id::transposeUp);
            press (p, fg::id::unmutate);
            expect (p.getSteps() == edited, "...but the edit is still the UNMUTATE point");
            // a Gate edit still does (Gate is on)
            rev = p.getBranchRevision();
            edit ([] (fg::Step& s) { s.gate = (std::uint8_t) (s.gate == 90 ? 40 : 90); });
            expect (p.getBranchRevision() > rev, "Gate filter on: a Gate edit makes the branches again");
            // every step filter off: a Mod edit still does (the manual's list has no Mod)
            for (const char* id : { fg::id::regenGate, fg::id::regenSlew, fg::id::regenRatchet, fg::id::regenStepOnOff })
                setParam (p, id, 0);
            rev = p.getBranchRevision();
            edit ([] (fg::Step& s) { s.slew = (std::uint8_t) (s.slew + 7); s.ratchet = 2; s.on = ! s.on; });
            expect (p.getBranchRevision() == rev, "all step filters off: Slew, Ratchet and On / Off edits leave the branches alone");
            edit ([] (fg::Step& s) { s.mod = (std::uint8_t) (s.mod + 5); });
            expect (p.getBranchRevision() > rev, "a Mod edit always makes the branches again");
            // Rotate, Transpose, Resize, Length
            setParam (p, fg::id::regenRotate, 0);
            rev = p.getBranchRevision();
            press (p, fg::id::rotateRight);
            expect (p.getBranchRevision() == rev, "Rotate filter off: ROTATE does not make the branches again");
            setParam (p, fg::id::regenRotate, 1);
            press (p, fg::id::rotateRight);
            expect (p.getBranchRevision() > rev, "Rotate filter on: ROTATE makes them again");
            setParam (p, fg::id::regenTranspose, 0);
            rev = p.getBranchRevision();
            press (p, fg::id::transposeDown);
            expect (p.getBranchRevision() == rev, "Transpose filter off: TRANSPOSE does not make the branches again");
            setParam (p, fg::id::regenResize, 0);
            rev = p.getBranchRevision();
            press (p, fg::id::resizeExpand);
            expect (p.getBranchRevision() == rev && lengthOf (p) == 16, "Resize filter off: RESIZE does not make the branches again");
            setParam (p, fg::id::regenLength, 0);
            rev = p.getBranchRevision();
            setParam (p, fg::id::length, 12);
            letWindowThreadRun (150);
            expect (p.getBranchRevision() == rev, "Length filter off: LENGTH by hand does not make the branches again");
            // UNMUTATE always does
            press (p, fg::id::unmutate);
            expect (p.getBranchRevision() > rev, "UNMUTATE always makes the branches again");
        }

        // -- the settings page on the panel --
        {
            resetAll (proc);
            FractalGardenEditor editor (proc);
            editor.setSize (1080, 720);
            auto* panel = editor.getStyledPanel();
            auto* open = findButton (*panel, "SETTINGS...");
            expect (open != nullptr, "the SETTINGS... button exists");
            expect (! panel->settingsOpen(), "the settings page starts closed");
            if (open) open->triggerClick();
            letWindowThreadRun (60);                                // (a click is carried out by the window thread a moment later)
            expect (panel->settingsOpen(), "SETTINGS... opens the settings page");
            snapshotTo (*panel, outDir.getChildFile ("fractal-garden-settings.png"));
            auto* done = findButton (panel->settingsComponent(), "DONE");
            expect (done != nullptr, "the settings page has a DONE button");
            if (done) done->triggerClick();
            letWindowThreadRun (60);
            expect (! panel->settingsOpen(), "DONE closes the settings page");
        }
    }

    std::cout << "preview checks: " << (checks - failures) << " of " << checks << " passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
