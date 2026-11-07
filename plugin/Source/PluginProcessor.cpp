#include "PluginProcessor.h"
#include "PluginEditor.h"

FractalGardenProcessor::FractalGardenProcessor()
    : AudioProcessor (BusesProperties()),                       // a MIDI effect: no audio buses
      apvts (*this, nullptr, "Parameters", FgParams::createLayout()),
      steps (fg::StepData::makeSample())
{
    rateParam   = apvts.getRawParameterValue (fg::id::stepRate);
    ratioParam  = apvts.getRawParameterValue (fg::id::ratio);
    lengthParam = apvts.getRawParameterValue (fg::id::length);
    audioSteps = steps;
    outBuffer.ensureSize (16384);
    saveStepsToState();
    startTimerHz (20);
}

FractalGardenProcessor::~FractalGardenProcessor()
{
    stopTimer();
}

//==============================================================================
void FractalGardenProcessor::switchOff (const char* parameterId)
{
    if (auto* p = apvts.getParameter (parameterId))
    {
        if (p->getValue() > 0.5f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (0.0f);
            p->endChangeGesture();
        }
    }
}

void FractalGardenProcessor::timerCallback()
{
    // The momentary buttons act the moment they go on (from milestone 4 on); then they go back off.
    for (const char* id : FgParams::momentaryIds())
        switchOff (id);
    switchTransientButtonsOff.store (false);
}

//==============================================================================
void FractalGardenProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate.store (newSampleRate > 0.0 ? newSampleRate : 44100.0);
    player.reset();
    testNoteSounding = false;
    testNoteSamplesLeft = 0;
    panicPending.store (true);          // if Logic restarted its audio while a note sounded, the first block sends all-notes-off
    outBuffer.ensureSize (16384);
}

bool FractalGardenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet().isDisabled() && layouts.getMainInputChannelSet().isDisabled();
}

void FractalGardenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    // 1. What Logic says: play state, tempo, time signature, song position (also shown in the window).
    fg::HostBlock host;
    if (auto* transport = getPlayHead())
    {
        if (auto position = transport->getPosition())
        {
            hostKnown.store (true);
            host.playing = position->getIsPlaying();
            hostPlaying.store (host.playing);
            if (auto bpm = position->getBpm())
            {
                host.bpm = *bpm;
                hostBpm.store (*bpm);
                hostTempoKnown.store (true);
            }
            if (auto ppq = position->getPpqPosition())
            {
                host.ppq = *ppq;
                host.havePpq = true;
                hostPpq.store (*ppq);
                hostPpqKnown.store (true);
            }
            if (auto signature = position->getTimeSignature())
            {
                host.sigNum = signature->numerator;
                host.sigDen = signature->denominator;
                hostSigNum.store (host.sigNum);
                hostSigDen.store (host.sigDen);
            }
        }
    }
    if (! host.playing)
        diagStopped.fetch_add (1);
    else if (! host.havePpq)
        diagNoPosition.fetch_add (1);

    // 2. The audio thread's copy of the steps (taken only when they changed, and never waiting for the window thread).
    const int revision = stepsRevision.load();
    if (revision != audioStepsRevision)
    {
        const juce::ScopedTryLock tryLock (stepLock);
        if (tryLock.isLocked())
        {
            audioSteps = steps;
            audioStepsRevision = revision;
        }
    }

    // 3. The clock decides which notes start and stop in this block.
    fg::PlayerSettings settings;
    settings.rate   = juce::jlimit (0, fg::kNumRates - 1,  juce::roundToInt (rateParam->load()));
    settings.ratio  = juce::jlimit (0, fg::kNumRatios - 1, juce::roundToInt (ratioParam->load()));
    settings.length = juce::jlimit (1, fg::kNumSteps,      juce::roundToInt (lengthParam->load()));
    const int numEvents = player.process (sampleRate.load(), numSamples, host, settings, audioSteps, events, kMaxEvents);

    // 4. Out: everything that arrived from Logic passes through unchanged, and the sequencer's notes are added at their sample positions.
    const int lastSample = numSamples - 1;
    outBuffer.clear();
    for (const auto meta : midi)
        outBuffer.addEvent (meta.getMessage(), juce::jlimit (0, lastSample, meta.samplePosition));

    if (panicPending.exchange (false))
        outBuffer.addEvent (juce::MidiMessage::allNotesOff (fg::kPlayerChannel), 0);

    for (int i = 0; i < numEvents; ++i)
    {
        const auto& e = events[i];
        outBuffer.addEvent (e.on ? juce::MidiMessage::noteOn (fg::kPlayerChannel, e.note, (juce::uint8) e.velocity)
                                 : juce::MidiMessage::noteOff (fg::kPlayerChannel, e.note),
                            juce::jlimit (0, lastSample, e.offset));
    }

    // 5. The temporary test note: only when Logic is stopped, and only if the click was made just now (a click that Logic never
    //    delivered a block for must not sound seconds later, when Logic starts).
    if (testRequested.exchange (false))
    {
        const juce::uint32 age = juce::Time::getMillisecondCounter() - testRequestedAtMs.load();
        if (! host.playing && age < 1500)
        {
            if (testNoteSounding)
                outBuffer.addEvent (juce::MidiMessage::noteOff (fg::kPlayerChannel, kTestNote), 0);
            outBuffer.addEvent (juce::MidiMessage::noteOn (fg::kPlayerChannel, kTestNote, (juce::uint8) fg::kPlayerVelocity), 0);
            testNoteSounding = true;
            testNoteSamplesLeft = (long long) (sampleRate.load() * 0.25);
            diagTestSent.fetch_add (1);
        }
    }
    if (testNoteSounding)
    {
        if (testNoteSamplesLeft < numSamples)
        {
            outBuffer.addEvent (juce::MidiMessage::noteOff (fg::kPlayerChannel, kTestNote), (int) juce::jmax ((long long) 0, testNoteSamplesLeft));
            testNoteSounding = false;
        }
        else
            testNoteSamplesLeft -= numSamples;
    }

    midi.swapWith (outBuffer);
}

// Bypassed in Logic (if Logic calls a bypassed MIDI effect at all): MIDI from Logic keeps passing through, the plugin adds nothing,
// and any note it has started is ended.
void FractalGardenProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    releaseEverything (midi);
}

void FractalGardenProcessor::releaseEverything (juce::MidiBuffer& out)
{
    const int count = player.releaseHeld (events, kMaxEvents);
    for (int i = 0; i < count; ++i)
        out.addEvent (juce::MidiMessage::noteOff (fg::kPlayerChannel, events[i].note), 0);
    if (testNoteSounding)
    {
        out.addEvent (juce::MidiMessage::noteOff (fg::kPlayerChannel, kTestNote), 0);
        testNoteSounding = false;
    }
    testRequested.store (false);
}

FractalGardenProcessor::HostSnapshot FractalGardenProcessor::getHostSnapshot() const
{
    HostSnapshot s;
    s.known = hostKnown.load();
    s.playing = hostPlaying.load();
    s.tempoKnown = hostTempoKnown.load();
    s.bpm = hostBpm.load();
    s.ppqKnown = hostPpqKnown.load();
    s.ppq = hostPpq.load();
    s.sigNum = hostSigNum.load();
    s.sigDen = hostSigDen.load();
    return s;
}

FractalGardenProcessor::PlayerView FractalGardenProcessor::getPlayerView() const
{
    PlayerView v;
    v.running = player.running();
    v.step = player.currentStep();
    v.gate = player.gateOpen();
    v.stepCount = player.stepCount();
    v.noteCount = player.noteCount();
    return v;
}

void FractalGardenProcessor::requestTestNote()
{
    diagTestRequests.fetch_add (1);
    testRequestedAtMs.store (juce::Time::getMillisecondCounter());
    testRequested.store (true);
}

FractalGardenProcessor::Diagnostics FractalGardenProcessor::getDiagnostics() const
{
    Diagnostics d;
    d.stoppedBlocks = diagStopped.load();
    d.noPositionBlocks = diagNoPosition.load();
    d.testRequests = diagTestRequests.load();
    d.testNotesSent = diagTestSent.load();
    return d;
}

//==============================================================================
void FractalGardenProcessor::saveStepsToState()
{
    std::vector<std::uint8_t> bytes;
    {
        const juce::ScopedLock sl (stepLock);
        bytes = steps.toBytes();
    }
    apvts.state.setProperty (FgParams::kStepsProperty, juce::Base64::toBase64 (bytes.data(), bytes.size()), nullptr);
    ++stepsRevision;
}

void FractalGardenProcessor::loadStepsFromState()
{
    const auto text = apvts.state.getProperty (FgParams::kStepsProperty).toString();
    if (text.isNotEmpty())
    {
        juce::MemoryOutputStream out;
        if (juce::Base64::convertFromBase64 (out, text))
        {
            fg::StepData loaded;
            if (loaded.fromBytes (static_cast<const std::uint8_t*> (out.getData()), out.getDataSize()))
            {
                const juce::ScopedLock sl (stepLock);
                steps = loaded;
                ++stepsRevision;
                return;
            }
        }
    }
    // Nothing usable saved (a new project, or a damaged one): the sample pattern.
    {
        const juce::ScopedLock sl (stepLock);
        steps = fg::StepData::makeSample();
    }
    saveStepsToState();
}

fg::StepData FractalGardenProcessor::getSteps() const
{
    const juce::ScopedLock sl (stepLock);
    return steps;
}

void FractalGardenProcessor::setStep (int index, const fg::Step& step)
{
    if (index < 0 || index >= fg::kNumSteps)
        return;
    {
        const juce::ScopedLock sl (stepLock);
        fg::Step s = step;
        s.clamp();
        if (steps.steps[(std::size_t) index] == s)
            return;
        steps.steps[(std::size_t) index] = s;
    }
    saveStepsToState();
}

int FractalGardenProcessor::getEditorTab() const
{
    return juce::jlimit (0, 4, (int) apvts.state.getProperty (FgParams::kTabProperty, 0));
}

int FractalGardenProcessor::getEditorPage() const
{
    return juce::jlimit (0, fg::kNumPages - 1, (int) apvts.state.getProperty (FgParams::kPageProperty, 0));
}

void FractalGardenProcessor::setEditorTab (int tab)
{
    apvts.state.setProperty (FgParams::kTabProperty, juce::jlimit (0, 4, tab), nullptr);
}

void FractalGardenProcessor::setEditorPage (int page)
{
    apvts.state.setProperty (FgParams::kPageProperty, juce::jlimit (0, fg::kNumPages - 1, page), nullptr);
}

//==============================================================================
juce::AudioProcessorEditor* FractalGardenProcessor::createEditor()
{
    return new FractalGardenEditor (*this);
}

void FractalGardenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FractalGardenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadStepsFromState();
            switchTransientButtonsOff.store (true);
        }
}

// This is what Logic calls to make the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FractalGardenProcessor();
}
