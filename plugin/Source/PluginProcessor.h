// The plugin wrapper: a MIDI effect (AU type aumi, no audio buses, MIDI in and MIDI out), set up like Dotriaconta-tone.
//
// Milestone 3: the trunk plays, locked to Logic. Each block the wrapper reads Logic's play state, tempo, time signature and song
// position and hands them, with RATE, RATIO, LENGTH and the 64 steps, to the engine's clock (fg::TrunkPlayer), which says which
// notes start and stop and at which sample. The notes go out as MIDI on channel 1; everything else that arrives from Logic passes
// through unchanged. The wrapper also holds the 46 controls, the 64 steps of data and the editor's tab / page / window choice, and
// saves all of them with the Logic project.
//
// Two temporary extras for this milestone's tests (they add no control and change nothing that is saved):
//  - requestTestNote(): clicking the STEP lamp in the header asks for one short note while Logic is STOPPED. It shows whether
//    Logic keeps calling a MIDI effect while stopped (which decides whether "click a step to hear it" can be built later).
//  - a few counters (getDiagnostics) shown in a small line under the header.
//
// Housekeeping: a few times a second, on the window thread, the momentary buttons that are on are switched off again (they act,
// then pop back). A project saved while one was on opens with all of them off.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <fg/Clock.h>
#include <fg/Steps.h>
#include "Parameters.h"

#include <atomic>

class FractalGardenProcessor : public juce::AudioProcessor, private juce::Timer
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

    // ---- temporary test aids for milestone 3 ----
    void requestTestNote();                     // from the window thread; only sounds while Logic is stopped, and only if asked just now
    struct Diagnostics
    {
        int stoppedBlocks = 0;                  // blocks the plugin was asked to process while Logic was stopped
        int noPositionBlocks = 0;               // blocks while Logic was playing but gave no song position
        int testRequests = 0, testNotesSent = 0;
    };
    Diagnostics getDiagnostics() const;

private:
    void timerCallback() override;
    void switchOff (const char* parameterId);
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
    fg::NoteEvent events[kMaxEvents];
    juce::MidiBuffer outBuffer;
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<bool> panicPending { false };                   // set by prepareToPlay: the first block sends all-notes-off

    // the temporary test note
    static constexpr int kTestNote = 72;                        // C4 in Logic's naming
    std::atomic<bool> testRequested { false };
    std::atomic<juce::uint32> testRequestedAtMs { 0 };
    bool testNoteSounding = false;
    long long testNoteSamplesLeft = 0;                          // audio thread only
    std::atomic<int> diagStopped { 0 }, diagNoPosition { 0 }, diagTestRequests { 0 }, diagTestSent { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalGardenProcessor)
};
