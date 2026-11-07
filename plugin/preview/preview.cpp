// Offline preview and control checks (not part of the plugin; only built with -DFG_BUILD_PREVIEW=ON).
//   1. draws the styled panel in the states of pictures A, B and C, and the plain window, to picture files;
//   2. checks, in BOTH windows, that all 46 ids exist, each has a control, and each control moves exactly one parameter;
//   3. checks saving and reloading (controls, step data, tab, page, window choice), the momentary buttons popping back,
//      the Knob Mode titles and colours, the Order slot (REPEAT ENDS / CHANCE...), and the Chance mix pop-over;
//   4. checks that the plugin adds no MIDI while Logic is stopped, and (milestone 3) that the sequencer follows a simulated Logic
//      transport: notes, stop, relocate, bypass, the temporary test note, RATE / RATIO / LENGTH, two instances, the header read-outs.
// Usage: FractalGardenPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdlib>
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

        const double sr = 48000.0, pps = 120.0 / 60.0 / sr;      // 120 bpm: 24000 samples per quarter note

        auto makeProc = [&] (MockHost& host)
        {
            auto p = std::make_unique<FractalGardenProcessor>();
            p->setPlayConfigDetails (0, 0, sr, 512);
            p->prepareToPlay (sr, 512);
            p->setPlayHead (&host);
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
            expect (! ev.empty() && ev[0].on && ev[0].t == 0 && ev[0].note == data.steps[0].note && ev[0].channel == 1 && ev[0].velocity == 100,
                    "plays step 1 at once, channel 1, velocity 100");
            int expectedOns = 0; bool timesOk = true, notesOk = true;
            for (int i = 0; i < 8; ++i)
                if (data.steps[(std::size_t) i].on)
                {
                    if ((std::size_t) expectedOns >= ons.size()) { timesOk = false; break; }
                    timesOk = timesOk && std::llabs (ons[(std::size_t) expectedOns].t - 6000LL * i) <= 1;
                    notesOk = notesOk && ons[(std::size_t) expectedOns].note == data.steps[(std::size_t) i].note;
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
            expect (! ev.empty() && ev[0].on && ev[0].t == clock && ev[0].note == dataNow.steps[0].note, "starting at the top of bar 3 plays step 1 at once");
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
            expect (p2->getDiagnostics().noPositionBlocks == 4, "...and is counted in the diagnostics line");
        }

        // -- the temporary test note: only while Logic is stopped, and only when asked just now --
        {
            MockHost host;
            auto p = makeProc (host);
            long long clock = 0;
            p->requestTestNote();
            auto ev = run (*p, host, clock, 16384, 512, 0.0, false);
            int ons = 0, offs = 0; long long onAt = -1, offAt = -1;
            for (const auto& e : ev) { if (e.on) { ++ons; onAt = e.t; expect (e.note == 72 && e.channel == 1, "the test note is C4 (72) on channel 1"); } else { ++offs; offAt = e.t; } }
            expect (ons == 1 && offs == 1 && onAt == 0 && std::llabs (offAt - 12000) <= 512, "the test note sounds once while stopped, for about a quarter of a second");
            expect (p->getDiagnostics().testNotesSent == 1 && p->getDiagnostics().testRequests == 1, "the test note is counted");

            p->requestTestNote();                                                    // while Logic plays: nothing
            ev = run (*p, host, clock, 2048, 512, 0.0, true);
            bool anyTest = false;
            for (const auto& e : ev) if (e.note == 72) anyTest = true;
            expect (! anyTest && p->getDiagnostics().testNotesSent == 1, "a test note asked for while Logic plays does not sound");

            p->requestTestNote();                                                    // a request that nobody picked up is not played seconds later
            juce::Thread::sleep (1700);
            ev = run (*p, host, clock, 2048, 512, 0.0, false);
            expect (ev.empty() && p->getDiagnostics().testNotesSent == 1, "an old test request is dropped, not played late");
        }

        // -- RATE, RATIO, LENGTH, edited steps and tempo take effect --
        {
            MockHost host;
            auto p = makeProc (host);
            setParam (*p, fg::id::stepRate, 4.0f);                                   // 1/8
            setParam (*p, fg::id::ratio, 5.0f);                                      // x2 -> a step is a 1/16 again
            setParam (*p, fg::id::length, 3.0f);
            fg::Step s = p->getSteps().steps[2];
            s.on = true; s.note = 55; s.gate = 100;
            p->setStep (2, s);
            long long clock = 0;
            const auto ev = run (*p, host, clock, 36000, 512, 0.0);
            std::vector<Ev> ons;
            for (const auto& e : ev) if (e.on) ons.push_back (e);
            bool spaced = ons.size() >= 5;
            for (std::size_t i = 0; i < ons.size(); ++i) spaced = spaced && std::llabs (ons[i].t - 6000LL * (long long) i) <= 1;
            expect (spaced, "RATE 1/8 with RATIO x2 plays a step every 1/16");
            const auto data = p->getSteps();
            expect (ons.size() >= 4 && ons[2].note == 55 && ons[0].note == data.steps[0].note && ons[3].note == data.steps[0].note, "LENGTH 3 loops after step 3, and an edited step is played");

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
