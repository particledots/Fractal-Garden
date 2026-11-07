// The eleven play orders of the trunk (milestone 4).
//
// Every order is a pure function of the step counter k (how many steps since the pattern's start), the loop LENGTH and a few settings.
// There is no state to keep, so the answer for a given k is the same whatever block size Logic uses, whether Logic starts in the
// middle of the song, jumps or cycles, and on every instance with the same settings. That is what keeps the plugin locked to Logic.
//
// 1 to 8 are the Bloom v2 manual's eight orders (Forward, Reverse, Pendulum, Random, Converge, Diverge, Converge & Diverge, Page Jump);
// where the manual's pictures did not extract as text, the shapes below are our own reading of its words (see the README).
// 9 to 11 (Drunk walk, Random no-repeat, Chance mix) and the Pendulum with repeated ends, the Random order and the hash they use are
// the user's own code from Dotriaconta-tone (carried over; the Chance mix defaults are the same).
//
// The "random" orders are seeded: the same seed gives the same sequence every time the same bars are played. The seed is chosen when
// an instance is first made and saved with the Logic project.

#pragma once

#include <fg/Params.h>

#include <cstdint>

namespace fg
{
    // The Chance mix settings: six orders, each with a weight 0 to 100 (0 = never picked), and how long a run lasts (steps).
    struct ChanceMix
    {
        enum { Forward, Backward, Pendulum, PingPong, Random, Drunk, kNumModes };
        int weight[kNumModes] = { 100, 60, 40, 0, 20, 0 };
        int runMin = 4, runMax = 16;               // 1 to 32 steps
    };

    // Which step (0 to length - 1) plays on step counter k. k may be negative (a count-in before bar 1); the orders keep working.
    //   order      0 to 10, the index into kOrderNames
    //   pendRepeat Pendulum only: the two end steps play twice (1-2-3-3-2-1)
    int orderStepIndex (long long k, int length, int order, bool pendRepeat, std::uint32_t seed, const ChanceMix& chance);
}
