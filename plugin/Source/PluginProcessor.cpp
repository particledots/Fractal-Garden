#include "PluginProcessor.h"
#include "PluginEditor.h"

FractalGardenProcessor::FractalGardenProcessor()
    : AudioProcessor (BusesProperties()),                       // a MIDI effect: no audio buses
      apvts (*this, nullptr, "Parameters", FgParams::createLayout()),
      steps (fg::StepData::makeSample())
{
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
void FractalGardenProcessor::prepareToPlay (double, int) {}

bool FractalGardenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet().isDisabled() && layouts.getMainInputChannelSet().isDisabled();
}

void FractalGardenProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Only look at what Logic says about play state and tempo (shown in the header). The MIDI buffer is left exactly as it came in:
    // this version adds no notes of its own.
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            hostKnown.store (true);
            hostPlaying.store (position->getIsPlaying());
            if (auto bpm = position->getBpm())
            {
                hostBpm.store (*bpm);
                hostTempoKnown.store (true);
            }
        }
    }
}

FractalGardenProcessor::HostSnapshot FractalGardenProcessor::getHostSnapshot() const
{
    HostSnapshot s;
    s.known = hostKnown.load();
    s.playing = hostPlaying.load();
    s.tempoKnown = hostTempoKnown.load();
    s.bpm = hostBpm.load();
    return s;
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
