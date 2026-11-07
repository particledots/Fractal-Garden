// The list of controls Logic can see and automate: 46 ids, exactly as in "Fractal-Garden control list (milestone 1, approved
// 7 Nov 2026)". Once a build with these ids ships, an id is NEVER renamed or removed, a menu's choices keep their order,
// and new controls are only appended at the end of kParams.
//
// This file is the single source for the plugin: the wrapper builds its parameters from kParams, the panel and the plain-slider
// window read the menu names from the tables below, and the engine self-test checks the whole list against the control list.
//
// Which milestone makes which control act is in the control list ("Works from"). Since milestone 3 Length, Rate and Ratio act (they set
// the loop and the speed of the trunk); the other controls move and are saved with the Logic project, and the momentary ones switch
// themselves back off, until their milestone.

#pragma once

#include <cstddef>

namespace fg
{
    inline constexpr int kNumSteps     = 64;
    inline constexpr int kStepsPerPage = 8;
    inline constexpr int kNumPages     = 8;
    inline constexpr int kNumParameters = 46;

    // ---- the ids (these strings are what Logic stores in projects and automation) ----
    namespace id
    {
        inline constexpr const char* branch         = "branch";
        inline constexpr const char* path           = "path";
        inline constexpr const char* mutate         = "mutate";
        inline constexpr const char* microRatchet   = "microRatchet";
        inline constexpr const char* microSlew      = "microSlew";
        inline constexpr const char* microMod       = "microMod";
        inline constexpr const char* perfRatchet    = "perfRatchet";
        inline constexpr const char* perfSlew       = "perfSlew";
        inline constexpr const char* perfMutate     = "perfMutate";
        inline constexpr const char* root           = "root";
        inline constexpr const char* knobMode       = "knobMode";
        inline constexpr const char* length         = "length";
        inline constexpr const char* scale          = "scale";
        inline constexpr const char* key            = "key";
        inline constexpr const char* order          = "order";
        inline constexpr const char* stepRate       = "stepRate";
        inline constexpr const char* ratio          = "ratio";
        inline constexpr const char* modMode        = "modMode";
        inline constexpr const char* patternSlot    = "patternSlot";
        inline constexpr const char* reseedScope    = "reseedScope";
        inline constexpr const char* reset          = "reset";
        inline constexpr const char* unmutate       = "unmutate";
        inline constexpr const char* reseedAll      = "reseedAll";
        inline constexpr const char* reseedBranches = "reseedBranches";
        inline constexpr const char* reseedNote     = "reseedNote";
        inline constexpr const char* reseedGate     = "reseedGate";
        inline constexpr const char* reseedSlew     = "reseedSlew";
        inline constexpr const char* reseedRatchet  = "reseedRatchet";
        inline constexpr const char* reseedMod      = "reseedMod";
        inline constexpr const char* resizeShrink   = "resizeShrink";
        inline constexpr const char* resizeExpand   = "resizeExpand";
        inline constexpr const char* rotateLeft     = "rotateLeft";
        inline constexpr const char* rotateRight    = "rotateRight";
        inline constexpr const char* transposeDown  = "transposeDown";
        inline constexpr const char* transposeUp    = "transposeUp";
        inline constexpr const char* patternLoad    = "patternLoad";
        inline constexpr const char* patternSave    = "patternSave";
        inline constexpr const char* pendRepeat     = "pendRepeat";
        inline constexpr const char* chanceForward  = "chanceForward";
        inline constexpr const char* chanceBackward = "chanceBackward";
        inline constexpr const char* chancePendulum = "chancePendulum";
        inline constexpr const char* chancePingPong = "chancePingPong";
        inline constexpr const char* chanceRandom   = "chanceRandom";
        inline constexpr const char* chanceDrunk    = "chanceDrunk";
        inline constexpr const char* chanceRunMin   = "chanceRunMin";
        inline constexpr const char* chanceRunMax   = "chanceRunMax";
    }

    // ---- the menus (the ORDER of each list is frozen) ----
    inline constexpr const char* kKnobModeNames[] = { "Default", "Micro Mutate", "Performance" };
    inline constexpr int kNumKnobModes = (int) (sizeof (kKnobModeNames) / sizeof (kKnobModeNames[0]));
    enum KnobMode { kModeDefault = 0, kModeMicro = 1, kModePerformance = 2 };

    // The same 28 scales as Dotriaconta-tone and the Scripter scripts, same order and names.
    inline constexpr const char* kScaleNames[] = {
        "Major (Ionian)", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian (Natural Minor)", "Locrian",
        "Harmonic Minor", "Melodic Minor", "Harmonic Major", "Hungarian Minor", "Phrygian Dominant",
        "Double Harmonic (Byzantine)", "Neapolitan Minor", "Neapolitan Major", "Enigmatic",
        "Major Pentatonic", "Minor Pentatonic", "Blues", "Hirajoshi", "In Sen", "Kumoi", "Iwato",
        "Whole Tone", "Octatonic (Whole-Half)", "Octatonic (Half-Whole)", "Prometheus", "Chromatic" };
    inline constexpr int kNumScales = (int) (sizeof (kScaleNames) / sizeof (kScaleNames[0]));

    inline constexpr const char* kKeyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline constexpr int kNumKeys = (int) (sizeof (kKeyNames) / sizeof (kKeyNames[0]));

    // 1 to 8 are the Bloom v2 manual's eight orders; 9 to 11 come from Dotriaconta-tone.
    inline constexpr const char* kOrderNames[] = {
        "Forward", "Reverse", "Pendulum", "Random", "Converge", "Diverge", "Converge & Diverge", "Page Jump",
        "Drunk walk", "Random no-repeat", "Chance mix" };
    inline constexpr int kNumOrders = (int) (sizeof (kOrderNames) / sizeof (kOrderNames[0]));
    enum Order
    {
        kOrderForward = 0, kOrderReverse = 1, kOrderPendulum = 2, kOrderRandom = 3, kOrderConverge = 4, kOrderDiverge = 5,
        kOrderConvergeDiverge = 6, kOrderPageJump = 7, kOrderDrunk = 8, kOrderRandomNoRepeat = 9, kOrderChanceMix = 10
    };

    inline constexpr const char* kRateNames[] = { "1 bar", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline constexpr int kNumRates = (int) (sizeof (kRateNames) / sizeof (kRateNames[0]));
    inline constexpr int kDefaultRate = 6;      // 1/16

    inline constexpr const char* kRatioNames[] = { "/8", "/4", "/3", "/2", "x1", "x2", "x3", "x4", "x8" };
    inline constexpr int kNumRatios = (int) (sizeof (kRatioNames) / sizeof (kRatioNames[0]));
    inline constexpr int kDefaultRatio = 4;     // x1

    inline constexpr const char* kModModeNames[] = { "Shapes", "Velocity", "Envelope", "Smooth" };
    inline constexpr int kNumModModes = (int) (sizeof (kModModeNames) / sizeof (kModModeNames[0]));

    inline constexpr const char* kReseedScopeNames[] = { "Page", "All 64" };
    inline constexpr int kNumReseedScopes = 2;

    inline constexpr int kNumPatternSlots = 16;
    inline constexpr const char* kPatternSlotNames[] = {
        "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6", "Slot 7", "Slot 8",
        "Slot 9", "Slot 10", "Slot 11", "Slot 12", "Slot 13", "Slot 14", "Slot 15", "Slot 16" };

    // ---- the 46 controls, in the order of the control list ----
    enum class Kind { Knob, Int, Choice, Toggle, Momentary };

    struct ParamSpec
    {
        const char* id;
        const char* name;                 // what Logic shows in its automation and Controls views
        Kind kind;
        int minValue, maxValue;           // Knob: 0 and 1 (the knob is 0.0 to 1.0); Int: the whole-number range; Choice: 0 to n-1; Toggle / Momentary: 0 and 1
        int defaultValue;                 // Knob: 0 means the knob's left end (every knob starts there); Toggle / Momentary: 0 = off
        const char* const* choices;       // Choice only
        int numChoices;
    };

    inline constexpr ParamSpec kParams[kNumParameters] = {
        // The nine Branch / Path / Mutate knobs (all stay in effect whichever Knob Mode is showing)
        { id::branch,         "Branch",                  Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::path,           "Path",                    Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::mutate,         "Mutate",                  Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::microRatchet,   "Micro Ratchet",           Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::microSlew,      "Micro Slew",              Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::microMod,       "Micro Mod",               Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::perfRatchet,    "Ratchet FX",              Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::perfSlew,       "Slew FX",                 Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::perfMutate,     "Mutate FX",               Kind::Knob,      0, 1,  0, nullptr, 0 },
        { id::root,           "Root",                    Kind::Int,     -24, 24, 0, nullptr, 0 },
        { id::knobMode,       "Knob Mode",               Kind::Choice,    0, 2,  0, kKnobModeNames, kNumKnobModes },
        { id::length,         "Length",                  Kind::Int,       1, 64, 8, nullptr, 0 },
        { id::scale,          "Scale",                   Kind::Choice,    0, kNumScales - 1, 0, kScaleNames, kNumScales },
        { id::key,            "Key",                     Kind::Choice,    0, kNumKeys - 1,   0, kKeyNames, kNumKeys },
        { id::order,          "Order",                   Kind::Choice,    0, kNumOrders - 1, 0, kOrderNames, kNumOrders },
        { id::stepRate,       "Rate",                    Kind::Choice,    0, kNumRates - 1,  kDefaultRate,  kRateNames,  kNumRates },
        { id::ratio,          "Ratio",                   Kind::Choice,    0, kNumRatios - 1, kDefaultRatio, kRatioNames, kNumRatios },
        { id::modMode,        "Mod Out Mode",            Kind::Choice,    0, kNumModModes - 1, 0, kModModeNames, kNumModModes },
        { id::patternSlot,    "Pattern Slot",            Kind::Choice,    0, kNumPatternSlots - 1, 0, kPatternSlotNames, kNumPatternSlots },
        { id::reseedScope,    "Reseed Scope",            Kind::Choice,    0, 1,  0, kReseedScopeNames, kNumReseedScopes },
        // Momentary buttons: act, then pop back off
        { id::reset,          "Reset",                   Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::unmutate,       "Unmutate",                Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedAll,      "Reseed All",              Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedBranches, "Reseed Branches",         Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedNote,     "Reseed Note",             Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedGate,     "Reseed Gate",             Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedSlew,     "Reseed Slew",             Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedRatchet,  "Reseed Ratchet",          Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::reseedMod,      "Reseed Mod",              Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::resizeShrink,   "Resize Shrink",           Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::resizeExpand,   "Resize Expand",           Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::rotateLeft,     "Rotate Left",             Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::rotateRight,    "Rotate Right",            Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::transposeDown,  "Transpose Down",          Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::transposeUp,    "Transpose Up",            Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::patternLoad,    "Pattern Load",            Kind::Momentary, 0, 1,  0, nullptr, 0 },
        { id::patternSave,    "Pattern Save",            Kind::Momentary, 0, 1,  0, nullptr, 0 },
        // Pendulum's repeat-ends switch and the Chance mix numbers (the ones Dotriaconta-tone uses)
        { id::pendRepeat,     "Pendulum Repeat Ends",    Kind::Toggle,    0, 1,  0, nullptr, 0 },
        { id::chanceForward,  "Chance Mix Forward",      Kind::Int,       0, 100, 100, nullptr, 0 },
        { id::chanceBackward, "Chance Mix Backward",     Kind::Int,       0, 100, 60,  nullptr, 0 },
        { id::chancePendulum, "Chance Mix Pendulum",     Kind::Int,       0, 100, 40,  nullptr, 0 },
        { id::chancePingPong, "Chance Mix Ping-Pong",    Kind::Int,       0, 100, 0,   nullptr, 0 },
        { id::chanceRandom,   "Chance Mix Random",       Kind::Int,       0, 100, 20,  nullptr, 0 },
        { id::chanceDrunk,    "Chance Mix Drunk",        Kind::Int,       0, 100, 0,   nullptr, 0 },
        { id::chanceRunMin,   "Chance Mix Shortest Run", Kind::Int,       1, 32,  4,   nullptr, 0 },
        { id::chanceRunMax,   "Chance Mix Longest Run",  Kind::Int,       1, 32,  16,  nullptr, 0 },
    };

    // The spec for an id, or nullptr if there is no such control.
    const ParamSpec* findParam (const char* paramId);

    // A MIDI note as Logic names it (60 = C3): "D4", "C#2".
    void noteName (int midiNote, char* out, std::size_t outSize);
}
