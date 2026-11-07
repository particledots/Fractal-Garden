// The plugin wrapper: a MIDI effect (AU type aumi, no audio buses, MIDI in and MIDI out), set up like Dotriaconta-tone.
//
// Milestone 3: the trunk plays, locked to Logic. Each block the wrapper reads Logic's play state, tempo, time signature and song
// position and hands them, with the controls and the 64 steps, to the engine's clock (fg::TrunkPlayer), which says which notes
// start and stop and at which sample. The notes go out as MIDI on channel 1; everything else that arrives from Logic passes
// through unchanged. The wrapper also holds the 46 controls, the 64 steps of data and the editor's tab / page / window choice, and
// saves all of them with the Logic project.
//
// Milestone 4: RATE, RATIO, LENGTH, SCALE, KEY, ROOT and ORDER (with Repeat Ends and the Chance mix numbers) are handed to the clock
// every block. The momentary buttons act like this:
//  - RESET: counted on whatever thread changes it (it can be Logic's automation); the audio thread sees the count change and
//    restarts the order at the next step.
//  - ROTATE and TRANSPOSE: counted the same way, then carried out on the window thread (they change the step data for good).
//
// Milestone 5: the clock also gets the Mod mode and is told to send CC 84 (slew) and CC 1 (mod) next to the notes; the CC messages
// are added to the output in the clock's order (CC before the note it belongs to). SHRINK and EXPAND (Resize) are counted and carried
// out like ROTATE, and they also set LENGTH.
// Then the button is switched off again a few times a second ("act, then pop back"). A project saved while one was on opens with all
// of them off and none of them acts on loading.
//
// Logic does not call a MIDI effect while its transport is stopped (milestone 3 test, both Macs), so everything here only sounds
// while Logic plays. Edits made while stopped are heard the next time it plays.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <fg/Clock.h>
#include <fg/Steps.h>
#include "Parameters.h"

#include <atomic>

class FractalGardenProcessor : public juce::AudioProcessor, private juce::Timer, private juce::AudioProcessorValueTreeState::Listener
{
public:
    FractalGardenProcessor();
    ~FractalGardenProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    // ---- the 64 steps (saved in the plugin state) ----
    fg::StepData getSteps() const;                          // a copy
    void setStep (int index, const fg::Step& step);         // changes one step and saves it into the state
    void rotateTrunk (int direction);                       // +1 RIGHT / -1 LEFT: the first LENGTH steps move one place, for good
    void transposeTrunk (int scaleSteps);                   // +1 UP / -1 DOWN: the first LENGTH steps move one scale step, for good
    void resizeTrunk (int direction);                       // -1 SHRINK / +1 EXPAND: the trunk becomes half / twice as long (LENGTH follows), for good
    int getStepsRevision() const { return stepsRevision.load(); }   // goes up whenever the steps change (the window repaints on it)

    // ---- editor choices (saved in the plugin state) ----
    int getEditorTab() const;
    int getEditorPage() const;
    void setEditorTab (int tab);
    void setEditorPage (int page);

    // ---- what Logic says (read-only, for the header of the window) ----
    struct HostSnapshot
    {
        bool known = false, playing = false, tempoKnown = false;
        double bpm = 120.0;
        bool ppqKnown = false;                  // Logic has given a song position (kept after a stop)
        double ppq = 0.0;                       // in quarter notes, bar 1 beat 1 = 0
        int sigNum = 4, sigDen = 4;
    };
    HostSnapshot getHostSnapshot() const;

    // ---- what the sequencer is doing (read-only, for the playhead, the lights and the STEP read-out) ----
    struct PlayerView
    {
        bool running = false;                   // Logic is playing and gave a song position
        int step = -1;                          // 0 to 63 while running, else -1
        bool gate = false;                      // a note is sounding
        std::uint32_t stepCount = 0, noteCount = 0;     // go up at every step / every note (the window flashes the lights on a change)
    };
    PlayerView getPlayerView() const;

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterId, float newValue) override;     // counts presses of the momentary buttons (any thread)
    void switchOff (const char* parameterId);
    void applyPendingButtons();
    void saveStepsToState();
    void loadStepsFromState();
    void releaseEverything (juce::MidiBuffer& out);     // note-offs for whatever this plugin has started (bypass)

    juce::AudioProcessorValueTreeState apvts;

    mutable juce::CriticalSection stepLock;
    fg::StepData steps;
    std::atomic<int> stepsRevision { 0 };

    std::atomic<bool> switchTransientButtonsOff { false };      // set when a saved project is loaded

    std::atomic<bool> hostKnown { false }, hostPlaying { false }, hostTempoKnown { false }, hostPpqKnown { false };
    std::atomic<double> hostBpm { 120.0 }, hostPpq { 0.0 };
    std::atomic<int> hostSigNum { 4 }, hostSigDen { 4 };

    // ---- the audio thread's own things ----
    static constexpr int kMaxEvents = 4096;                     // notes per block (a 1/32 at x8 needs a few dozen even in a huge block)
    fg::TrunkPlayer player;
    fg::StepData audioSteps;                                    // the audio thread's copy of the steps
    int audioStepsRevision = -1;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* lengthParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* rootParam = nullptr;
    std::atomic<float>* orderParam = nullptr;
    std::atomic<float>* modModeParam = nullptr;
    std::atomic<float>* pendRepeatParam = nullptr;
    std::atomic<float>* chanceParams[fg::ChanceMix::kNumModes] = {};
    std::atomic<float>* chanceRunMinParam = nullptr;
    std::atomic<float>* chanceRunMaxParam = nullptr;

    std::atomic<std::uint32_t> orderSeed { 1 };                 // seed of the random orders (saved in the state)
    std::atomic<std::uint32_t> resetCount { 0 };                // goes up at every press of RESET
    std::atomic<int> pendingRotateLeft { 0 }, pendingRotateRight { 0 }, pendingTransposeUp { 0 }, pendingTransposeDown { 0 };
    std::atomic<int> pendingShrink { 0 }, pendingExpand { 0 };
    fg::NoteEvent events[kMaxEvents];
    juce::MidiBuffer outBuffer;
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<bool> panicPending { false };                   // set by prepareToPlay: the first block sends all-notes-off

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalGardenProcessor)
};
