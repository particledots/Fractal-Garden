// The controls Logic can see and automate: all 46 ids of the control list, built from the engine's table (fg/Params.h), which is
// the single source for ids, ranges, defaults and menu orders. The ids are FROZEN once a build ships; new controls are only
// appended at the end of the table.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <fg/Params.h>

namespace FgParams
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // The ids of the 17 momentary buttons (they act, then switch themselves back off).
    const std::vector<const char*>& momentaryIds();

    // Names of the things saved next to the controls in the plugin state (not automatable).
    inline constexpr const char* kStepsProperty = "steps";         // the 64 steps, as text
    inline constexpr const char* kTabProperty   = "editorTab";     // 0 Note, 1 Gate Length, 2 Slew, 3 Ratchet, 4 Mod
    inline constexpr const char* kPageProperty  = "editorPage";    // 0 to 7
    inline constexpr const char* kViewProperty  = "editorView";    // "styled" or "plain"
    inline constexpr const char* kSeedProperty  = "orderSeed";     // seed of the random orders, chosen when an instance is first made
}
