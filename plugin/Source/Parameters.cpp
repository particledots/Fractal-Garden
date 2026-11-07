#include "Parameters.h"

namespace FgParams
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& spec : fg::kParams)
    {
        const juce::ParameterID pid { spec.id, 1 };
        switch (spec.kind)
        {
            case fg::Kind::Knob:
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, spec.name, juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f),
                                                                         (float) spec.defaultValue));
                break;
            case fg::Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, spec.name, spec.minValue, spec.maxValue, spec.defaultValue));
                break;
            case fg::Kind::Choice:
            {
                juce::StringArray names;
                for (int i = 0; i < spec.numChoices; ++i)
                    names.add (spec.choices[i]);
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, spec.name, names, spec.defaultValue));
                break;
            }
            case fg::Kind::Toggle:
            case fg::Kind::Momentary:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, spec.name, spec.defaultValue != 0));
                break;
        }
    }
    return layout;
}

const std::vector<const char*>& momentaryIds()
{
    static const std::vector<const char*> ids = []
    {
        std::vector<const char*> v;
        for (const auto& spec : fg::kParams)
            if (spec.kind == fg::Kind::Momentary)
                v.push_back (spec.id);
        return v;
    }();
    return ids;
}

} // namespace FgParams
