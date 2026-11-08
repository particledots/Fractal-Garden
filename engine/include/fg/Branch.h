// Branches and Path (milestone 6).
//
// The trunk is the 64-step pattern (the first LENGTH steps play). BRANCH sets how many branches (0 to 7) play after the trunk. Every branch
// is as long as the trunk and is made from the branch before it (branch 1 from the trunk) by one of five changes:
//   R Reverse   the steps play backwards
//   I Inverse   every note is mirrored around the trunk's first note (up becomes down)
//   T Transpose every note moves up by 1 to a whole octave of scale steps
//   M Mutate    about 60 % of the notes and 25 % of the gates are mutated (the same changes as the Mutate knob)
//   ? Randomize a new random note for every step, about 20 % of steps OFF, a random gate length; slew, ratchet and mod are copied
//
// PATH. The manual says only that there are 2^branches paths. We read them as a binary tree: at every branch level there are two variants,
// Path picks one variant per level (the bits of the path number, first level first), and every variant gets its own change and its own
// random numbers drawn from the branch seed. So the same Branch, Path and seed always give the same branches, and a different path
// changes the branches from the first level where its bits differ. The two variants at one level always get different changes.
//
// Nothing here keeps state: the branches are a pure function of (trunk snapshot, LENGTH, SCALE, branch count, path, seed).

#pragma once

#include <fg/Mutate.h>
#include <fg/Steps.h>

#include <array>
#include <cstdint>

namespace fg
{
    inline constexpr int kMaxBranches = 7;

    enum BranchKind { kBranchReverse = 0, kBranchInverse = 1, kBranchTranspose = 2, kBranchMutate = 3, kBranchRandomize = 4 };
    inline constexpr int kNumBranchKinds = 5;
    inline constexpr char kBranchLetters[kNumBranchKinds] = { 'R', 'I', 'T', 'M', '?' };

    int branchCountFromKnob (double knob);               // 0 to 7 (the knob 0..1, rounded)
    int pathCountFor (int branches);                     // 2 ^ branches (1 when there are no branches)
    int pathFromKnob (double knob, int branches);        // 0 to pathCountFor - 1

    // What branch `level` (1 to branches) does on this path. Used by the panel's TREE letters as well as by buildBranches.
    int branchKindFor (int level, int path, int branches, std::uint32_t seed);

    struct BranchSet
    {
        int count = 0;                                                      // branches built (0 to 7)
        std::array<int, kMaxBranches> kind {};                              // BranchKind of each
        std::array<std::array<Step, kNumSteps>, kMaxBranches> steps {};     // branch b (0-based) is steps[b]; the first LENGTH steps of each are used
    };

    // Builds the branches of the path from the trunk snapshot `source`. rangeOctaves (1.2.0, see fg/Mutate.h) limits the notes the Randomize
    // and Mutate branches make by themselves; Reverse, Inverse and Transpose work from the notes that are there, as before. A step's fine
    // pitch goes with it (Inverse turns it round, Randomize puts it back to 0).
    void buildBranches (BranchSet&, const StepData& source, int length, int scale, int branches, int path, std::uint32_t seed,
                        int rangeOctaves = kNumOctaves);
}
