// Mutation and reseed (milestones 6 and 7): the random changes made to steps.
//
// Plain C++. Everything here works on one Step (or a run of steps) and a random number generator that the caller owns, so the same
// code serves Default Mutate and Micro Mutate while the sequencer plays, the Mutate branches, and the Reseed buttons.
//
// DEFAULT MUTATE (the manual's two zones, knob 0 to 1):
//   Zone 1, knob 0 to 0.5: only the NOTE can change. The chance rises from 0 to 100 % across the zone.
//   Zone 2, knob 0.5 to 1: the note chance drops back to a fixed 75 %, and the GATE chance rises from 0 to 100 %. When a gate change is
//   triggered, 70 % of the time the gate LENGTH changes and 30 % of the time the step is switched on or off.
//   A note change moves the note by scale steps: mostly 1 to 3 steps (1: half the time, 2: 30 %, 3: 20 %) up or down; one time in five
//   by a whole octave (the number of notes in the scale). Never past the ends of the five octaves.
//
// MICRO MUTATE (the manual has no zones for these, only "none at the left, every step at the right"): each knob is the chance, 0 to 1,
//   that a step gets a new ratchet amount, a new slew amount, or a new Mod value. In Shapes mode the Mod change picks a new shape (the
//   Mod value is not used in Shapes mode); in the other modes it picks a new Mod value 0 to 100 %.
//
// RESEED (dice roll): ranges of the new values are listed in the comments at reseedSteps below.

#pragma once

#include <fg/Steps.h>

#include <cstdint>

namespace fg
{
    // A small seeded random number generator (splitmix64). The state is just a number, so it is cheap to keep and copy.
    struct Rng
    {
        std::uint64_t state = 0x9e3779b97f4a7c15ULL;

        Rng() = default;
        explicit Rng (std::uint64_t seed) : state (seed * 0x9e3779b97f4a7c15ULL + 0x2545f4914f6cdd1dULL) {}

        std::uint64_t next();
        double unit();               // 0 up to (not including) 1
        int below (int n);           // 0 to n - 1 (n < 1 gives 0)
    };

    // ---- Default Mutate ----
    double mutateNoteChance (double knob);       // 0 to 1: zone 1 rises to 100 %, zone 2 is fixed at 75 %
    double mutateGateChance (double knob);       // 0 to 1: zero in zone 1, rises to 100 % across zone 2

    bool mutateNote (Step&, int scale, Rng&);    // moves the note by scale steps (true if it changed)
    bool mutateGate (Step&, Rng&);               // 70 %: new gate length; 30 %: toggle on / off (true if it changed)

    // One step, one knob value: draws the note chance, then the gate chance. Draws nothing at knob 0. True if the step changed.
    bool defaultMutate (Step&, double knob, int scale, Rng&);

    // ---- Micro Mutate ----
    int randomRatchet (Rng&);                    // an index into kRatchetAmounts, x1 most likely, x32 rarest
    bool microMutate (Step&, double ratchetChance, double slewChance, double modChance, int modMode, Rng&);

    // ---- Reseed ----
    enum class ReseedKind { All, Note, Gate, Slew, Ratchet, Mod };

    // Gives the steps first .. first + count - 1 (cut off at 64) new random values.
    //  Note:    a place anywhere in the five octaves (every degree of the current scale equally likely).
    //  Gate:    about 20 % of steps OFF; gate length one of 10 25 40 50 50 60 75 90 100 %.
    //  Slew:    70 % none, otherwise 10 to 100 %.
    //  Ratchet: 75 % x1, otherwise one of the others (the same weights as Micro Mutate).
    //  Mod:     value 0 to 100 %, any of the eight shapes, rate x1 60 % of the time and otherwise any other of the 16 rates.
    //  All:     all of the above.
    //  For All, allMask says which of the five parts are included (the settings page's RESEED FILTER): bit 0 Note, 1 Gate, 2 Slew, 3 Ratchet,
    //  4 Mod. The default includes all five, which is the same as before. A part that is left out draws no random numbers.
    inline constexpr unsigned kReseedAllMask = 0x1fu;
    void reseedSteps (StepData&, int first, int count, ReseedKind, int scale, Rng&, unsigned allMask = kReseedAllMask);
}
