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
    scaleParam  = apvts.getRawParameterValue (fg::id::scale);
    keyParam    = apvts.getRawParameterValue (fg::id::key);
    rootParam   = apvts.getRawParameterValue (fg::id::root);
    orderParam  = apvts.getRawParameterValue (fg::id::order);
    modModeParam = apvts.getRawParameterValue (fg::id::modMode);
    pendRepeatParam = apvts.getRawParameterValue (fg::id::pendRepeat);
    chanceParams[fg::ChanceMix::Forward]  = apvts.getRawParameterValue (fg::id::chanceForward);
    chanceParams[fg::ChanceMix::Backward] = apvts.getRawParameterValue (fg::id::chanceBackward);
    chanceParams[fg::ChanceMix::Pendulum] = apvts.getRawParameterValue (fg::id::chancePendulum);
    chanceParams[fg::ChanceMix::PingPong] = apvts.getRawParameterValue (fg::id::chancePingPong);
    chanceParams[fg::ChanceMix::Random]   = apvts.getRawParameterValue (fg::id::chanceRandom);
    chanceParams[fg::ChanceMix::Drunk]    = apvts.getRawParameterValue (fg::id::chanceDrunk);
    chanceRunMinParam = apvts.getRawParameterValue (fg::id::chanceRunMin);
    chanceRunMaxParam = apvts.getRawParameterValue (fg::id::chanceRunMax);

    // The seed of the random orders: chosen once for a new instance, then saved with the project (so the same bars play the same way).
    orderSeed.store (((std::uint32_t) juce::Random::getSystemRandom().nextInt() & 0x7fffffffu) | 1u);
    apvts.state.setProperty (FgParams::kSeedProperty, (int) orderSeed.load(), nullptr);

    audioSteps = steps;
    outBuffer.ensureSize (16384);
    saveStepsToState();

    for (const char* id : { fg::id::reset, fg::id::rotateLeft, fg::id::rotateRight, fg::id::transposeDown, fg::id::transposeUp, fg::id::resizeShrink, fg::id::resizeExpand })
        apvts.addParameterListener (id, this);
    startTimerHz (20);
}

FractalGardenProcessor::~FractalGardenProcessor()
{
    stopTimer();
    for (const char* id : { fg::id::reset, fg::id::rotateLeft, fg::id::rotateRight, fg::id::transposeDown, fg::id::transposeUp, fg::id::resizeShrink, fg::id::resizeExpand })
        apvts.removeParameterListener (id, this);
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

// A press of a momentary button is counted here, on whatever thread changed the control (the window, or Logic's automation).
void FractalGardenProcessor::parameterChanged (const juce::String& parameterId, float newValue)
{
    if (newValue <= 0.5f)
        return;                                     // the button popping back off is not a press
    if (parameterId == fg::id::reset)               ++resetCount;
    else if (parameterId == fg::id::rotateLeft)     ++pendingRotateLeft;
    else if (parameterId == fg::id::rotateRight)    ++pendingRotateRight;
    else if (parameterId == fg::id::transposeDown)  ++pendingTransposeDown;
    else if (parameterId == fg::id::transposeUp)    ++pendingTransposeUp;
    else if (parameterId == fg::id::resizeShrink)   ++pendingShrink;
    else if (parameterId == fg::id::resizeExpand)   ++pendingExpand;
}

// Rotate and Transpose change the step data, so they are carried out here, on the window thread.
void FractalGardenProcessor::applyPendingButtons()
{
    for (int n = pendingRotateLeft.exchange (0); n > 0; --n)      rotateTrunk (-1);
    for (int n = pendingRotateRight.exchange (0); n > 0; --n)     rotateTrunk (+1);
    for (int n = pendingTransposeDown.exchange (0); n > 0; --n)   transposeTrunk (-1);
    for (int n = pendingTransposeUp.exchange (0); n > 0; --n)     transposeTrunk (+1);
    for (int n = pendingShrink.exchange (0); n > 0; --n)          resizeTrunk (-1);
    for (int n = pendingExpand.exchange (0); n > 0; --n)          resizeTrunk (+1);
}

void FractalGardenProcessor::rotateTrunk (int direction)
{
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    {
        const juce::ScopedLock sl (stepLock);
        fg::rotateSteps (steps, length, direction);
    }
    saveStepsToState();
}

void FractalGardenProcessor::transposeTrunk (int scaleSteps)
{
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    const int scale = juce::jlimit (0, fg::kNumScales - 1, juce::roundToInt (scaleParam->load()));
    {
        const juce::ScopedLock sl (stepLock);
        fg::transposeSteps (steps, length, scaleSteps, scale);
    }
    saveStepsToState();
}

// Resize: the trunk becomes half as long (every other step kept) or twice as long (each step followed by a copy that is OFF), and
// LENGTH is set to match. Nothing happens when it cannot (length 1 cannot shrink, more than 32 cannot expand).
void FractalGardenProcessor::resizeTrunk (int direction)
{
    int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    bool changed;
    {
        const juce::ScopedLock sl (stepLock);
        changed = direction < 0 ? fg::resizeShrink (steps, length) : fg::resizeExpand (steps, length);
    }
    if (! changed)
        return;
    saveStepsToState();
    if (auto* p = apvts.getParameter (fg::id::length))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) length));
        p->endChangeGesture();
    }
}

void FractalGardenProcessor::timerCallback()
{
    applyPendingButtons();

    // The editor's page stays on a page that LENGTH reaches (when LENGTH shrinks, the page follows it down).
    const int lastPage = (juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load())) - 1) / fg::kStepsPerPage;
    if ((int) apvts.state.getProperty (FgParams::kPageProperty, 0) > lastPage)
        setEditorPage (lastPage);

    // The momentary buttons act the moment they go on; then they go back off.
    for (const char* id : FgParams::momentaryIds())
        switchOff (id);
    switchTransientButtonsOff.store (false);
}

//==============================================================================
void FractalGardenProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate.store (newSampleRate > 0.0 ? newSampleRate : 44100.0);
    player.reset();
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
    settings.order  = juce::jlimit (0, fg::kNumOrders - 1, juce::roundToInt (orderParam->load()));
    settings.modMode = juce::jlimit (0, fg::kNumModModes - 1, juce::roundToInt (modModeParam->load()));
    settings.sendCc = true;
    settings.pendRepeat = pendRepeatParam->load() > 0.5f;
    settings.seed   = orderSeed.load();
    for (int i = 0; i < fg::ChanceMix::kNumModes; ++i)
        settings.chance.weight[i] = juce::jlimit (0, 100, juce::roundToInt (chanceParams[i]->load()));
    settings.chance.runMin = juce::jlimit (1, 32, juce::roundToInt (chanceRunMinParam->load()));
    settings.chance.runMax = juce::jlimit (1, 32, juce::roundToInt (chanceRunMaxParam->load()));
    settings.scale  = juce::jlimit (0, fg::kNumScales - 1, juce::roundToInt (scaleParam->load()));
    settings.key    = juce::jlimit (0, fg::kNumKeys - 1,   juce::roundToInt (keyParam->load()));
    settings.root   = juce::jlimit (-24, 24,               juce::roundToInt (rootParam->load()));
    settings.resetCount = resetCount.load();
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
        if (e.cc >= 0)
        {
            outBuffer.addEvent (juce::MidiMessage::controllerEvent (fg::kPlayerChannel, e.cc, e.value), juce::jlimit (0, lastSample, e.offset));
            continue;
        }
        outBuffer.addEvent (e.on ? juce::MidiMessage::noteOn (fg::kPlayerChannel, e.note, (juce::uint8) e.velocity)
                                 : juce::MidiMessage::noteOff (fg::kPlayerChannel, e.note),
                            juce::jlimit (0, lastSample, e.offset));
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
            // Loading a project is not a press of any button, even if a button was on when it was saved.
            const auto resetBefore = resetCount.load();
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            resetCount.store (resetBefore);
            pendingRotateLeft.store (0); pendingRotateRight.store (0);
            pendingTransposeDown.store (0); pendingTransposeUp.store (0);
            pendingShrink.store (0); pendingExpand.store (0);

            if (apvts.state.hasProperty (FgParams::kSeedProperty))
                orderSeed.store ((std::uint32_t) (int) apvts.state.getProperty (FgParams::kSeedProperty) | 1u);
            else
                apvts.state.setProperty (FgParams::kSeedProperty, (int) orderSeed.load(), nullptr);      // a project from 0.3.0 or earlier

            loadStepsFromState();
            switchTransientButtonsOff.store (true);
        }
}

// This is what Logic calls to make the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FractalGardenProcessor();
}
