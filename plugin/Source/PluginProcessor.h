// The plugin wrapper: a MIDI effect (AU type aumi, no audio buses, MIDI in and MIDI out), set up like Dotriaconta-tone.
//
// Milestone 2: nothing plays. MIDI arriving from Logic passes through unchanged and the plugin adds no notes of its own. The wrapper
// holds the 46 controls, the 64 steps of data and the editor's tab / page / window choice, and saves all of them with the Logic
// project. It also notes Logic's play state and tempo for the header of the window (read-only; the sequencer comes in milestone 3).
//
// Housekeeping: a few times a second, on the window thread, the momentary buttons that are on are switched off again (they act,
// then pop back). A project saved while one was on opens with all of them off.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
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
    struct HostSnapshot { bool known = false; bool playing = false; bool tempoKnown = false; double bpm = 120.0; };
    HostSnapshot getHostSnapshot() const;

private:
    void timerCallback() override;
    void switchOff (const char* parameterId);
    void saveStepsToState();
    void loadStepsFromState();

    juce::AudioProcessorValueTreeState apvts;

    mutable juce::CriticalSection stepLock;
    fg::StepData steps;
    std::atomic<int> stepsRevision { 0 };

    std::atomic<bool> switchTransientButtonsOff { false };      // set when a saved project is loaded

    std::atomic<bool> hostKnown { false }, hostPlaying { false }, hostTempoKnown { false };
    std::atomic<double> hostBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractalGardenProcessor)
};
