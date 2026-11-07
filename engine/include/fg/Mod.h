// The Mod output (milestone 5): what each of the four Mod modes puts out for a step, as a number from 0 to 1 (the manual's 0 V to 5 V).
// The plugin sends it as MIDI CC 1 (0 to 127); the clock decides when. Plain C++, no plugin code.
//
// The manual describes the four modes but not their exact curves; where it is vague these are our own approximations (see the README
// and the gaps ledger):
//   Shapes   one of our eight shapes, played at one of the manual's sixteen rates. A "one-shot" plays the shape once in the first
//            1/n of the step and then holds the shape's last value; a "ratchet" repeats the whole shape n times in the step; "1/2" and
//            "1/4" stretch the shape over 2 or 4 steps and this step plays the named part; the two "fixed" rates hold one level (the
//            shape picks one of eight levels, 0 to 2.5 V or 2.5 V to 5 V). An OFF step is 0.
//   Velocity a fixed level (the step's Mod value) for the whole step. An OFF step is 0.
//   Envelope an attack / decay triangle over the step whose peak (full scale) sits at the step's Mod value: 0 % is an instant attack with
//            a long decay (plucky), 50 % is equal parts, 100 % is a long attack and an instant drop (reverse plucky). OFF is 0.
//   Smooth   a straight glide over the whole step from the previous step's level to this step's level. An OFF step glides to 0.

#pragma once

#include <fg/Steps.h>

namespace fg
{
    inline constexpr int kModCc = 1;       // the manual's default Mod CC number
    inline constexpr int kSlewCc = 84;     // the manual's default Slew CC number

    enum ModRateKind { kRateOneShot = 0, kRateRatchet = 1, kRatePart = 2, kRateFixed = 3 };

    struct ModRate
    {
        ModRateKind kind = kRateOneShot;
        int n = 1;          // OneShot / Ratchet: the speed multiple; Part: the number of parts (2 or 4)
        int part = 0;       // Part: which part (0 = first); Fixed: 0 = 0 to 2.5 V, 1 = 2.5 V to 5 V
    };

    ModRate modRateInfo (int rate);                          // rate 0 to 15, the manual's order

    double modShapeValue (int shape, double t);              // our eight shapes: t 0 to 1 in, 0 to 1 out
    double shapesModValue (const Step&, double t);           // Shapes mode, t = how far through the step (0 to 1)
    bool shapesModMoves (const Step&);                       // false when the value is constant over the step (OFF, fixed levels)
    double envelopeModValue (int modPercent, double t);
    double smoothModValue (double from, double to, double t);
    double stepLevel (const Step&);                          // the step's Mod value as 0 to 1, 0 when the step is OFF

    int modToCc (double value);                              // 0 to 1 becomes 0 to 127
}
