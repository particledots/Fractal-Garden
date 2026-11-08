#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // The controls whose changes the wrapper watches: the momentary buttons that act, and LENGTH (a change by hand is an edit).
    constexpr const char* kListened[] = {
        fg::id::reset, fg::id::rotateLeft, fg::id::rotateRight, fg::id::transposeDown, fg::id::transposeUp, fg::id::resizeShrink, fg::id::resizeExpand,
        fg::id::unmutate, fg::id::reseedAll, fg::id::reseedBranches, fg::id::reseedNote, fg::id::reseedGate, fg::id::reseedSlew,
        fg::id::reseedRatchet, fg::id::reseedMod, fg::id::patternLoad, fg::id::patternSave, fg::id::patternShare, fg::id::patternGrab, fg::id::length };
}

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
    branchParam = apvts.getRawParameterValue (fg::id::branch);
    pathParam = apvts.getRawParameterValue (fg::id::path);
    mutateParam = apvts.getRawParameterValue (fg::id::mutate);
    microRatchetParam = apvts.getRawParameterValue (fg::id::microRatchet);
    microSlewParam = apvts.getRawParameterValue (fg::id::microSlew);
    microModParam = apvts.getRawParameterValue (fg::id::microMod);
    perfRatchetParam = apvts.getRawParameterValue (fg::id::perfRatchet);
    perfSlewParam = apvts.getRawParameterValue (fg::id::perfSlew);
    perfMutateParam = apvts.getRawParameterValue (fg::id::perfMutate);
    reseedScopeParam = apvts.getRawParameterValue (fg::id::reseedScope);
    pendRepeatParam = apvts.getRawParameterValue (fg::id::pendRepeat);
    chanceParams[fg::ChanceMix::Forward]  = apvts.getRawParameterValue (fg::id::chanceForward);
    chanceParams[fg::ChanceMix::Backward] = apvts.getRawParameterValue (fg::id::chanceBackward);
    chanceParams[fg::ChanceMix::Pendulum] = apvts.getRawParameterValue (fg::id::chancePendulum);
    chanceParams[fg::ChanceMix::PingPong] = apvts.getRawParameterValue (fg::id::chancePingPong);
    chanceParams[fg::ChanceMix::Random]   = apvts.getRawParameterValue (fg::id::chanceRandom);
    chanceParams[fg::ChanceMix::Drunk]    = apvts.getRawParameterValue (fg::id::chanceDrunk);
    chanceRunMinParam = apvts.getRawParameterValue (fg::id::chanceRunMin);
    chanceRunMaxParam = apvts.getRawParameterValue (fg::id::chanceRunMax);
    midiChannelParam = apvts.getRawParameterValue (fg::id::midiChannel);
    ccSlewParam = apvts.getRawParameterValue (fg::id::ccSlew);
    ccModParam = apvts.getRawParameterValue (fg::id::ccMod);
    baseNoteParam = apvts.getRawParameterValue (fg::id::baseNote);
    resizeMethodParam = apvts.getRawParameterValue (fg::id::resizeMethod);
    patternSlotParam = apvts.getRawParameterValue (fg::id::patternSlot);
    {
        static const char* const reseedIds[5] = { fg::id::reseedFilterNote, fg::id::reseedFilterGate, fg::id::reseedFilterSlew,
                                                  fg::id::reseedFilterRatchet, fg::id::reseedFilterMod };
        for (int i = 0; i < 5; ++i)
            reseedFilterParams[i] = apvts.getRawParameterValue (reseedIds[i]);
        static const char* const regenIds[kNumRegenEdits] = { fg::id::regenNote, fg::id::regenGate, fg::id::regenSlew, fg::id::regenRatchet,
                                                              fg::id::regenStepOnOff, fg::id::regenLength, fg::id::regenResize,
                                                              fg::id::regenRotate, fg::id::regenTranspose };
        for (int i = 0; i < kNumRegenEdits; ++i)
            regenParams[i] = apvts.getRawParameterValue (regenIds[i]);
    }

    // The seed of the random orders: chosen once for a new instance, then saved with the project (so the same bars play the same way).
    orderSeed.store (((std::uint32_t) juce::Random::getSystemRandom().nextInt() & 0x7fffffffu) | 1u);
    apvts.state.setProperty (FgParams::kSeedProperty, (int) orderSeed.load(), nullptr);

    // The branch seed and the random numbers of the Reseed buttons: chosen once for a new instance (the seed is then saved with the project).
    branchSeed.store (((std::uint32_t) juce::Random::getSystemRandom().nextInt() & 0x7fffffffu) | 1u);
    reseedRng = fg::Rng ((std::uint64_t) juce::Random::getSystemRandom().nextInt64());
    undoSteps = steps;
    undoLength = 8;
    branchSource = steps;
    apvts.state.setProperty (FgParams::kBranchSeedProperty, (int) branchSeed.load(), nullptr);

    audioSteps = steps;
    audioBranchSource = steps;
    outBuffer.ensureSize (16384);
    saveStepsToState();
    saveUndoToState();
    savePatternBankToState();

    for (const char* id : kListened)
        apvts.addParameterListener (id, this);
    startTimerHz (20);
}

FractalGardenProcessor::~FractalGardenProcessor()
{
    stopTimer();
    for (const char* id : kListened)
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
    if (parameterId == fg::id::length)
    {
        // LENGTH changed by hand or by automation is a manual edit (it sets the unmutate point and remakes the branches). A change the
        // plugin makes itself (Resize, Unmutate, loading a project) records the new value first, so it is not seen as a change.
        if (juce::roundToInt (newValue) != lastLengthSeen.exchange (juce::roundToInt (newValue)))
            lengthEditPending.store (true);
        return;
    }
    if (newValue <= 0.5f)
        return;                                     // the button popping back off is not a press
    if (parameterId == fg::id::reset)               ++resetCount;
    else if (parameterId == fg::id::rotateLeft)     ++pendingRotateLeft;
    else if (parameterId == fg::id::rotateRight)    ++pendingRotateRight;
    else if (parameterId == fg::id::transposeDown)  ++pendingTransposeDown;
    else if (parameterId == fg::id::transposeUp)    ++pendingTransposeUp;
    else if (parameterId == fg::id::resizeShrink)   ++pendingShrink;
    else if (parameterId == fg::id::resizeExpand)   ++pendingExpand;
    else if (parameterId == fg::id::patternLoad)    ++pendingPatternLoad;
    else if (parameterId == fg::id::patternSave)    ++pendingPatternSave;
    else if (parameterId == fg::id::patternShare)   ++pendingPatternShare;
    else if (parameterId == fg::id::patternGrab)    ++pendingPatternGrab;
    else if (parameterId == fg::id::unmutate)       ++pendingUnmutate;
    else if (parameterId == fg::id::reseedAll)      ++pendingReseedAll;
    else if (parameterId == fg::id::reseedBranches) ++pendingReseedBranches;
    else if (parameterId == fg::id::reseedNote)     ++pendingReseedNote;
    else if (parameterId == fg::id::reseedGate)     ++pendingReseedGate;
    else if (parameterId == fg::id::reseedSlew)     ++pendingReseedSlew;
    else if (parameterId == fg::id::reseedRatchet)  ++pendingReseedRatchet;
    else if (parameterId == fg::id::reseedMod)      ++pendingReseedMod;
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
    for (int n = pendingPatternSave.exchange (0); n > 0; --n)     savePatternNow (getPatternSlot());
    for (int n = pendingPatternLoad.exchange (0); n > 0; --n)     loadPatternNow (getPatternSlot());
    for (int n = pendingPatternShare.exchange (0); n > 0; --n)    sharePatternNow();
    for (int n = pendingPatternGrab.exchange (0); n > 0; --n)     grabPatternNow();
    for (int n = pendingUnmutate.exchange (0); n > 0; --n)        unmutate();
    for (int n = pendingReseedAll.exchange (0); n > 0; --n)       reseed (fg::ReseedKind::All);
    for (int n = pendingReseedNote.exchange (0); n > 0; --n)      reseed (fg::ReseedKind::Note);
    for (int n = pendingReseedGate.exchange (0); n > 0; --n)      reseed (fg::ReseedKind::Gate);
    for (int n = pendingReseedSlew.exchange (0); n > 0; --n)      reseed (fg::ReseedKind::Slew);
    for (int n = pendingReseedRatchet.exchange (0); n > 0; --n)   reseed (fg::ReseedKind::Ratchet);
    for (int n = pendingReseedMod.exchange (0); n > 0; --n)       reseed (fg::ReseedKind::Mod);
    for (int n = pendingReseedBranches.exchange (0); n > 0; --n)  reseedBranches();
}

// The window thread's side of Mutate: write the changes the audio thread made to trunk steps into the saved steps.
void FractalGardenProcessor::drainMutations()
{
    int start1, size1, start2, size2;
    mutationFifo.prepareToRead (mutationFifo.getNumReady(), start1, size1, start2, size2);
    bool any = false;
    const auto epochNow = (std::uint32_t) stepsRevision.load();
    {
        const juce::ScopedLock sl (stepLock);
        auto apply = [&] (int start, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                const auto& e = mutationBuffer[start + i];
                if (e.epoch != epochNow || e.index < 0 || e.index >= fg::kNumSteps)
                    continue;                                        // made before an edit: dropped
                steps.steps[(std::size_t) e.index] = e.step;
                any = true;
            }
        };
        apply (start1, size1);
        apply (start2, size2);
    }
    mutationFifo.finishedRead (size1 + size2);
    if (any)
        saveStepsQuietly();
}

// A manual edit: this is the state UNMUTATE goes back to, and the branches are made again from it.
// remakeBranches is false when the settings page's branch-regeneration filter says this kind of edit leaves the branches alone.
void FractalGardenProcessor::markEdit (bool remakeBranches)
{
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    {
        const juce::ScopedLock sl (stepLock);
        undoSteps = steps;
        undoLength = length;
        if (remakeBranches)
            branchSource = steps;
    }
    if (remakeBranches)
        ++branchRevision;
    saveUndoToState();
}

bool FractalGardenProcessor::regenOn (RegenEdit e) const
{
    return regenParams[(int) e]->load() > 0.5f;
}

void FractalGardenProcessor::renewBranches()
{
    {
        const juce::ScopedLock sl (stepLock);
        branchSource = steps;
    }
    ++branchRevision;
    saveUndoToState();
}

void FractalGardenProcessor::setLengthQuietly (int length)
{
    lastLengthSeen.store (length);
    if (auto* p = apvts.getParameter (fg::id::length))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) length));
        p->endChangeGesture();
    }
}

void FractalGardenProcessor::unmutate()
{
    int length;
    {
        const juce::ScopedLock sl (stepLock);
        steps = undoSteps;
        branchSource = steps;
        length = undoLength;
    }
    ++branchRevision;
    saveStepsToState();
    saveUndoToState();
    if (juce::roundToInt (lengthParam->load()) != length)
        setLengthQuietly (length);
}

// RESEED: new random values for the page the editor is showing, or for all 64 steps. The trunk changes for good (UNMUTATE rolls it back);
// the branches are left as they were (RESEED: BRANCHES makes new ones).
void FractalGardenProcessor::reseed (fg::ReseedKind kind)
{
    const int scale = juce::jlimit (0, fg::kNumScales - 1, juce::roundToInt (scaleParam->load()));
    const bool page = juce::roundToInt (reseedScopeParam->load()) == 0;
    const int first = page ? getEditorPage() * fg::kStepsPerPage : 0;
    const int count = page ? fg::kStepsPerPage : fg::kNumSteps;
    {
        const juce::ScopedLock sl (stepLock);
        unsigned mask = 0;
        for (int i = 0; i < 5; ++i)
            if (reseedFilterParams[i]->load() > 0.5f)
                mask |= 1u << i;
        fg::reseedSteps (steps, first, count, kind, scale, reseedRng, mask);
    }
    saveStepsToState();
}

void FractalGardenProcessor::reseedBranches()
{
    branchSeed.store (((std::uint32_t) juce::Random::getSystemRandom().nextInt() & 0x7fffffffu) | 1u);
    apvts.state.setProperty (FgParams::kBranchSeedProperty, (int) branchSeed.load(), nullptr);
    renewBranches();
}

FractalGardenProcessor::BranchView FractalGardenProcessor::getBranchView() const
{
    BranchView v;
    v.count = fg::branchCountFromKnob (branchParam->load());
    v.path = fg::pathFromKnob (pathParam->load(), v.count);
    v.seed = branchSeed.load();
    return v;
}

void FractalGardenProcessor::rotateTrunk (int direction)
{
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    {
        const juce::ScopedLock sl (stepLock);
        fg::rotateSteps (steps, length, direction);
    }
    saveStepsToState();
    if (regenOn (kRegenRotate))
        renewBranches();
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
    if (regenOn (kRegenTranspose))
        renewBranches();
}

// Resize: the trunk becomes half as long (every other step kept) or twice as long (each step followed by a copy that is OFF), and
// LENGTH is set to match. Nothing happens when it cannot (length 1 cannot shrink, more than 32 cannot expand).
void FractalGardenProcessor::resizeTrunk (int direction)
{
    int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    bool changed;
    {
        const juce::ScopedLock sl (stepLock);
        const auto method = (fg::ResizeMethod) juce::jlimit (0, fg::kNumResizeMethodChoices - 1, juce::roundToInt (resizeMethodParam->load()));
        changed = direction < 0 ? fg::resizeShrink (steps, length) : fg::resizeExpand (steps, length, method);
    }
    if (! changed)
        return;
    saveStepsToState();
    if (regenOn (kRegenResize))
        renewBranches();
    setLengthQuietly (length);
}

void FractalGardenProcessor::timerCallback()
{
    drainMutations();
    applyPendingButtons();
    if (++sharedTick >= 20)
    {
        sharedTick = 0;
        refreshSharedInfo();
    }
    if (lengthEditPending.exchange (false))
        markEdit (regenOn (kRegenLength));

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

    // The branch snapshot, the same way.
    const int branchRev = branchRevision.load();
    if (branchRev != audioBranchRevision)
    {
        const juce::ScopedTryLock tryLock (stepLock);
        if (tryLock.isLocked())
        {
            audioBranchSource = branchSource;
            audioBranchRevision = branchRev;
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
    settings.branches = fg::branchCountFromKnob (branchParam->load());
    settings.path = fg::pathFromKnob (pathParam->load(), settings.branches);
    settings.branchSeed = branchSeed.load();
    settings.branchRevision = (std::uint32_t) audioBranchRevision;
    settings.epoch = (std::uint32_t) audioStepsRevision;
    settings.mutate = mutateParam->load();
    settings.microRatchet = microRatchetParam->load();
    settings.microSlew = microSlewParam->load();
    settings.microMod = microModParam->load();
    settings.perfRatchet = perfRatchetParam->load();       // Performance mode: temporary effects played over the steps (never stored)
    settings.perfSlew = perfSlewParam->load();
    settings.perfMutate = perfMutateParam->load();
    settings.slewCc = juce::jlimit (0, 127, juce::roundToInt (ccSlewParam->load()));         // the settings page
    settings.modCc = juce::jlimit (0, 127, juce::roundToInt (ccModParam->load()));
    settings.baseNote = juce::jlimit (0, fg::kMaxBaseNote, juce::roundToInt (baseNoteParam->load()));

    // The MIDI channel (settings page). If it changed, the note that is sounding is ended on the old channel first.
    const int channel = juce::jlimit (1, 16, juce::roundToInt (midiChannelParam->load()));
    int oldChannel = 0, releasedNote = -1;
    if (audioChannel == 0)
        audioChannel = channel;
    else if (channel != audioChannel)
    {
        fg::NoteEvent released[2];
        if (player.releaseHeld (released, 2) > 0)
            releasedNote = released[0].note;
        oldChannel = audioChannel;
        audioChannel = channel;
    }
    const int numEvents = player.process (sampleRate.load(), numSamples, host, settings, audioSteps, audioBranchSource, events, kMaxEvents);

    // Changes Mutate made to trunk steps go to the window thread, which saves them.
    {
        fg::MutationEvent mutated[64];
        for (;;)
        {
            const int m = player.takeMutations (mutated, 64);
            if (m <= 0)
                break;
            int start1, size1, start2, size2;
            mutationFifo.prepareToWrite (m, start1, size1, start2, size2);
            for (int i = 0; i < size1; ++i) mutationBuffer[start1 + i] = mutated[i];
            for (int i = 0; i < size2; ++i) mutationBuffer[start2 + i] = mutated[size1 + i];
            mutationFifo.finishedWrite (size1 + size2);
            if (m < 64)
                break;
        }
    }

    // 4. Out: everything that arrived from Logic passes through unchanged, and the sequencer's notes are added at their sample positions.
    const int lastSample = numSamples - 1;
    outBuffer.clear();
    for (const auto meta : midi)
        outBuffer.addEvent (meta.getMessage(), juce::jlimit (0, lastSample, meta.samplePosition));

    if (oldChannel != 0)
    {
        if (releasedNote >= 0)
            outBuffer.addEvent (juce::MidiMessage::noteOff (oldChannel, releasedNote), 0);
        outBuffer.addEvent (juce::MidiMessage::allNotesOff (oldChannel), 0);
    }
    if (panicPending.exchange (false))
        outBuffer.addEvent (juce::MidiMessage::allNotesOff (audioChannel), 0);

    for (int i = 0; i < numEvents; ++i)
    {
        const auto& e = events[i];
        if (e.cc >= 0)
        {
            outBuffer.addEvent (juce::MidiMessage::controllerEvent (audioChannel, e.cc, e.value), juce::jlimit (0, lastSample, e.offset));
            continue;
        }
        outBuffer.addEvent (e.on ? juce::MidiMessage::noteOn (audioChannel, e.note, (juce::uint8) e.velocity)
                                 : juce::MidiMessage::noteOff (audioChannel, e.note),
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
        out.addEvent (juce::MidiMessage::noteOff (audioChannel > 0 ? audioChannel : fg::kPlayerChannel, events[i].note), 0);
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
    v.section = player.currentSection();
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

// Mutation changes arrive many times a second: they are saved with the project but do not make the audio thread copy the steps again
// (it already has them).
void FractalGardenProcessor::saveStepsQuietly()
{
    std::vector<std::uint8_t> bytes;
    {
        const juce::ScopedLock sl (stepLock);
        bytes = steps.toBytes();
    }
    apvts.state.setProperty (FgParams::kStepsProperty, juce::Base64::toBase64 (bytes.data(), bytes.size()), nullptr);
    ++mutationRevision;
}

void FractalGardenProcessor::saveUndoToState()
{
    std::vector<std::uint8_t> undoBytes, sourceBytes;
    int length;
    {
        const juce::ScopedLock sl (stepLock);
        undoBytes = undoSteps.toBytes();
        sourceBytes = branchSource.toBytes();
        length = undoLength;
    }
    apvts.state.setProperty (FgParams::kUndoStepsProperty, juce::Base64::toBase64 (undoBytes.data(), undoBytes.size()), nullptr);
    apvts.state.setProperty (FgParams::kUndoLengthProperty, length, nullptr);
    apvts.state.setProperty (FgParams::kBranchSourceProperty, juce::Base64::toBase64 (sourceBytes.data(), sourceBytes.size()), nullptr);
}

// The unmutate point and the branch snapshot from a saved project. A project saved before 0.6.0 has neither: both start as the steps as they are.
void FractalGardenProcessor::loadUndoFromState()
{
    auto read = [this] (const char* property, fg::StepData& into)
    {
        const auto text = apvts.state.getProperty (property).toString();
        if (text.isEmpty())
            return false;
        juce::MemoryOutputStream out;
        if (! juce::Base64::convertFromBase64 (out, text))
            return false;
        fg::StepData loaded;
        if (! loaded.fromBytes (static_cast<const std::uint8_t*> (out.getData()), out.getDataSize()))
            return false;
        into = loaded;
        return true;
    };
    fg::StepData undo, source;
    const bool haveUndo = read (FgParams::kUndoStepsProperty, undo);
    const bool haveSource = read (FgParams::kBranchSourceProperty, source);
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    {
        const juce::ScopedLock sl (stepLock);
        undoSteps = haveUndo ? undo : steps;
        branchSource = haveSource ? source : steps;
        undoLength = haveUndo && apvts.state.hasProperty (FgParams::kUndoLengthProperty)
                       ? juce::jlimit (1, fg::kNumSteps, (int) apvts.state.getProperty (FgParams::kUndoLengthProperty)) : length;
    }
    ++branchRevision;
    saveUndoToState();
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
    bool remake = true;
    {
        const juce::ScopedLock sl (stepLock);
        fg::Step s = step;
        s.clamp();
        const fg::Step old = steps.steps[(std::size_t) index];
        if (old == s)
            return;
        steps.steps[(std::size_t) index] = s;
        // Which fields did the edit change? The branches are made again if one of them is on in the branch-regeneration filter. The Mod
        // fields are not in the manual's list of edits, so a Mod edit always makes them again (as in 0.7.0).
        remake = false;
        if (old.note != s.note)      remake = remake || regenOn (kRegenNote);
        if (old.gate != s.gate)      remake = remake || regenOn (kRegenGate);
        if (old.slew != s.slew)      remake = remake || regenOn (kRegenSlew);
        if (old.ratchet != s.ratchet) remake = remake || regenOn (kRegenRatchet);
        if (old.on != s.on)          remake = remake || regenOn (kRegenOnOff);
        if (old.mod != s.mod || old.modShape != s.modShape || old.modRate != s.modRate)
            remake = true;
    }
    saveStepsToState();
    markEdit (remake);          // a step edited by hand is the new UNMUTATE point; it remakes the branches unless the filter says no
}

//==============================================================================
// The pattern bank (milestone 9)
int FractalGardenProcessor::getPatternSlot() const
{
    return juce::jlimit (0, fg::kNumPatternSlots - 1, juce::roundToInt (patternSlotParam->load()));
}

FractalGardenProcessor::PatternInfo FractalGardenProcessor::getPatternInfo (int slot) const
{
    PatternInfo info;
    const juce::ScopedLock sl (stepLock);
    if (bank.isUsed (slot))
    {
        info.used = true;
        info.length = bank.slots[(std::size_t) slot].length;
    }
    return info;
}

// SAVE: the steps as they are now (mutations included, Performance effects never) and LENGTH go into the slot.
void FractalGardenProcessor::savePatternNow (int slot)
{
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    {
        const juce::ScopedLock sl (stepLock);
        if (! bank.save (slot, steps, length))
            return;
    }
    savePatternBankToState();
}

// LOAD: a manual edit. The steps and LENGTH come from the slot, it is the new UNMUTATE point, and the branches are made again.
void FractalGardenProcessor::loadPatternNow (int slot)
{
    fg::StepData loaded;
    int length = 8;
    {
        const juce::ScopedLock sl (stepLock);
        if (! bank.load (slot, loaded, length))
            return;                                     // an empty slot: nothing happens
    }
    applyLoadedPattern (loaded, length);
}

// What LOAD and GRAB do with a pattern: a manual edit.
void FractalGardenProcessor::applyLoadedPattern (const fg::StepData& loaded, int length)
{
    {
        const juce::ScopedLock sl (stepLock);
        steps = loaded;
    }
    saveStepsToState();
    if (juce::roundToInt (lengthParam->load()) != length)
        setLengthQuietly (length);
    else
        lastLengthSeen.store (length);
    markEdit (true);
}

//==============================================================================
// Sharing one pattern between instances (1.1.0). A plug-in cannot see another instance, so SHARE and GRAB go through one small file.
juce::File FractalGardenProcessor::getSharedPatternFile() const
{
    auto folder = sharedFolder;
    if (folder == juce::File())
        folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Fractal-Garden");
    return folder.getChildFile ("shared-pattern.fgpattern");
}

void FractalGardenProcessor::setSharedPatternFolder (const juce::File& folder)
{
    sharedFolder = folder;
    sharedStamp = juce::Time();
    sharedFileSize = -1;
    refreshSharedInfo();
}

// SHARE: the steps as they are now (mutations included, Performance effects never) and LENGTH go into the file. A file already there is replaced.
void FractalGardenProcessor::sharePatternNow()
{
    const int length = juce::jlimit (1, fg::kNumSteps, juce::roundToInt (lengthParam->load()));
    std::vector<std::uint8_t> bytes;
    {
        const juce::ScopedLock sl (stepLock);
        bytes = fg::sharedPatternToBytes (steps, length);
    }
    const auto file = getSharedPatternFile();
    const bool ok = file.getParentDirectory().createDirectory().wasOk() && file.replaceWithData (bytes.data(), bytes.size());
    sharedResult.store (ok ? 1 : 2);
    sharedStamp = juce::Time();
    sharedFileSize = -1;
    refreshSharedInfo();
    ++patternRevision;
}

// GRAB: a manual edit, exactly like LOAD. A missing or damaged file changes nothing.
void FractalGardenProcessor::grabPatternNow()
{
    const auto file = getSharedPatternFile();
    if (! file.existsAsFile())
    {
        sharedResult.store (4);
        ++patternRevision;
        return;
    }
    juce::MemoryBlock block;
    fg::StepData loaded;
    int length = 8;
    if (! file.loadFileAsData (block)
        || ! fg::sharedPatternFromBytes (static_cast<const std::uint8_t*> (block.getData()), block.getSize(), loaded, length))
    {
        sharedResult.store (5);
        ++patternRevision;
        return;
    }
    applyLoadedPattern (loaded, length);
    sharedResult.store (3);
    refreshSharedInfo();
    ++patternRevision;
}

void FractalGardenProcessor::refreshSharedInfo()
{
    const auto file = getSharedPatternFile();
    int found = -1;
    juce::Time stamp;
    juce::int64 size = -1;
    if (file.existsAsFile())
    {
        stamp = file.getLastModificationTime();
        size = file.getSize();
        if (stamp == sharedStamp && size == sharedFileSize)
            return;                                     // the file has not changed since it was last read
        juce::MemoryBlock block;
        fg::StepData tmp;
        int len = 8;
        if (file.loadFileAsData (block)
            && fg::sharedPatternFromBytes (static_cast<const std::uint8_t*> (block.getData()), block.getSize(), tmp, len))
            found = len;
    }
    sharedStamp = stamp;
    sharedFileSize = size;
    if (sharedLength.exchange (found) != found)
        ++patternRevision;
}

juce::String FractalGardenProcessor::getSharedText() const
{
    const int len = sharedLength.load();
    switch (sharedResult.load())
    {
        case 2:  return "SHARE FAILED: THE FILE COULD NOT BE WRITTEN";
        case 4:  return "GRAB: NOTHING HAS BEEN SHARED YET";
        case 5:  return "GRAB FAILED: THE SHARED FILE IS NOT READABLE";
        case 3:  if (len > 0) return "GRABBED THE SHARED PATTERN (" + juce::String (len) + " STEPS)"; break;
        default: break;
    }
    return len > 0 ? "SHARED FILE: " + juce::String (len) + " STEPS" : juce::String ("SHARED FILE: NONE");
}

void FractalGardenProcessor::savePatternBankToState()
{
    std::vector<std::uint8_t> bytes;
    {
        const juce::ScopedLock sl (stepLock);
        bytes = bank.toBytes();
    }
    apvts.state.setProperty (FgParams::kPatternBankProperty, juce::Base64::toBase64 (bytes.data(), bytes.size()), nullptr);
    ++patternRevision;
}

// The bank from a saved project. A project from before 0.8.0 has none: all 16 slots are empty. Loading never acts on anything.
void FractalGardenProcessor::loadPatternBankFromState()
{
    fg::PatternBank loaded;
    const auto text = apvts.state.getProperty (FgParams::kPatternBankProperty).toString();
    if (text.isNotEmpty())
    {
        juce::MemoryOutputStream out;
        if (juce::Base64::convertFromBase64 (out, text))
            loaded.fromBytes (static_cast<const std::uint8_t*> (out.getData()), out.getDataSize());     // damaged data leaves the bank empty
    }
    {
        const juce::ScopedLock sl (stepLock);
        bank = loaded;
    }
    savePatternBankToState();
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
            pendingPatternLoad.store (0); pendingPatternSave.store (0);
            pendingPatternShare.store (0); pendingPatternGrab.store (0);
            pendingUnmutate.store (0); pendingReseedAll.store (0); pendingReseedBranches.store (0); pendingReseedNote.store (0);
            pendingReseedGate.store (0); pendingReseedSlew.store (0); pendingReseedRatchet.store (0); pendingReseedMod.store (0);

            if (apvts.state.hasProperty (FgParams::kSeedProperty))
                orderSeed.store ((std::uint32_t) (int) apvts.state.getProperty (FgParams::kSeedProperty) | 1u);
            else
                apvts.state.setProperty (FgParams::kSeedProperty, (int) orderSeed.load(), nullptr);      // a project from 0.3.0 or earlier

            if (apvts.state.hasProperty (FgParams::kBranchSeedProperty))
                branchSeed.store ((std::uint32_t) (int) apvts.state.getProperty (FgParams::kBranchSeedProperty) | 1u);
            else
                apvts.state.setProperty (FgParams::kBranchSeedProperty, (int) branchSeed.load(), nullptr);      // a project from 0.5.0 or earlier

            loadStepsFromState();
            // loading a project is not a change of LENGTH by hand: forget what replaceState reported
            lastLengthSeen.store (juce::roundToInt (lengthParam->load()));
            lengthEditPending.store (false);
            loadUndoFromState();
            loadPatternBankFromState();
            switchTransientButtonsOff.store (true);
        }
}

// This is what Logic calls to make the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FractalGardenProcessor();
}
