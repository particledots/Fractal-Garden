// Performance mode (milestone 8): the three temporary effects, Ratchet FX, Slew FX and Mutate FX (with ornaments).
//
// The manual calls these "non-destructive": they change what is PLAYED, never the stored steps. So the player makes a copy of each step as
// it starts, applies the effects to the copy, and plays the copy. Nothing here is saved, nothing goes into the mutation log, and
// Unmutate and the saved pattern never see it. Turn a knob back to 0 and the pattern is exactly as it was.
//
// Plain C++. Everything is a pure function of (the stored step, the step counter, the seed, the knobs): no running random stream, so the
// result for a step is the same whatever the block size, it stays locked to Logic's bars, and looking ahead at the next step (for the
// slew glide) gives the same answer as when that step plays.
//
// EACH KNOB HAS TWO ZONES, split at the middle of the knob (the manual: "the first half" and "the second half"):
//   RATCHET FX  zone 1 (0 to 0.5): a chance rising from 0 to 100 % that a step gets a random ratchet of x2, x3, x4 or x6 (the manual's
//               largest random amount is x6). Zone 2 (0.5 to 1): every step gets one fixed amount; the zone is cut into eight equal bands
//               for the eight amounts x1 x2 x3 x4 x6 x8 x16 x32 (the manual shows the amount on the eight trunk LEDs).
//   SLEW FX     zone 1: a chance rising from 0 to 100 % that a step gets a random slew of 10 to 100 %. Zone 2: every step gets the same
//               slew, 0 % rising to 100 % across the zone.
//   MUTATE FX   zone 1: a chance rising from 0 to 100 % that a step gets a temporary change: half the time its note moves (as Mutate,
//               fg/Mutate.h), half the time its gate changes (70 % a new length, 30 % on / off). Zone 2: ornaments. The chance of an
//               ornament rises from 0 to 100 % across the zone; if no ornament happens, a temporary note change happens with that same
//               chance. 0.5 to 0.67 offers the seven two-note ornaments, 0.67 to 0.84 adds the seven four-note ones, above 0.84 all of them
//               and the eight-note trill. One of the offered ones is picked at random.
//
// ORNAMENTS are made of eighth notes. A step counts as a quarter note, so an eighth is half a step: a two-note ornament lasts one step, a
// four-note one two steps, the trill four steps. An ornament starts at its step's start and takes over the following steps it covers (their
// own notes do not play). Each ornament note sounds for 80 % of its eighth.

#pragma once

#include <fg/Steps.h>

#include <cstdint>

namespace fg
{
    inline constexpr double kPerfZoneSplit = 0.5;            // zone 1 below this, zone 2 above
    inline constexpr double kPerfTwoNoteTop = 0.67;          // ornaments: up to here only the two-note ones
    inline constexpr double kPerfFourNoteTop = 0.84;         // up to here the four-note ones as well; above, all of them and the trill

    struct PerformKnobs
    {
        double ratchet = 0.0, slew = 0.0, mutate = 0.0;      // 0 to 1
        bool any() const { return ratchet > 0.0 || slew > 0.0 || mutate > 0.0; }
    };

    // ---- the zones, as plain numbers (the panel's read-outs use these too) ----
    double perfChance (double knob);                         // zone 1: 0 to 1 across the zone; 1 in zone 2
    bool perfInZone2 (double knob);
    int perfFixedRatchet (double knob);                      // zone 2: index into kRatchetAmounts (0 to 7); -1 in zone 1
    int perfFixedSlew (double knob);                         // zone 2: 0 to 100 (percent); -1 in zone 1
    double perfOrnamentChance (double knob);                 // zone 2: 0 to 1 across the zone; 0 in zone 1
    int perfOrnamentsOffered (double knob);                  // 0 in zone 1, 7, 14 or 15 in zone 2

    // ---- ornaments ----
    enum OrnamentKind
    {
        kOrnAnticipation = 0, kOrnSuspension, kOrnSyncopation, kOrnOctaveUp, kOrnFifthUp, kOrnHalfTurnToward, kOrnHalfTurnAway,   // two notes
        kOrnRunToward, kOrnRunAway, kOrnTurn, kOrnArpToward, kOrnArpAway, kOrnMordentUp, kOrnMordentDown,                          // four notes
        kOrnTrill                                                                                                                  // eight notes
    };
    inline constexpr int kNumOrnaments = 15;
    inline constexpr int kMaxOrnamentNotes = 8;
    inline constexpr int kOrnRest = -1000;                   // a slot with no note (Syncopation's first eighth)
    inline constexpr double kOrnamentGateFraction = 0.8;     // each ornament note sounds for this much of its eighth

    const char* ornamentName (int kind);
    int ornamentNotes (int kind);                            // 2, 4 or 8 (the number of eighth notes)
    int ornamentSteps (int kind);                            // how many steps it lasts: notes / 2

    // The notes of an ornament, as running degree numbers (0 to degreeCount - 1, the same scale of numbers Transpose and Root use) or kOrnRest.
    //   cur   the current step's degree; next / prev   the degrees of the next / previous step in play order, or -1 if that step is not there
    //   or is OFF. Returns the number of notes written to out (0 when the ornament cannot be made: Anticipation and Half Turn Toward need a
    //   next step, Suspension needs a previous one).
    int ornamentPlan (int kind, int cur, int next, int prev, int scale, int out[kMaxOrnamentNotes]);

    // ---- one step ----
    struct PerformResult
    {
        Step step;               // what plays (ratchet, slew and Mutate FX applied; no ornament)
        Step stepIfNoOrnament;   // what plays if the ornament chosen below cannot be made: the same, with the note change an ornament miss gives
        int ornament = -1;       // an OrnamentKind, or -1 (none this time)
    };

    // The effects for the step `stored`, which plays at step counter value k. Draws nothing and returns the step unchanged when the knobs
    // are all 0.
    PerformResult performStep (const Step& stored, long long k, std::uint32_t seed, int scale, const PerformKnobs&);
}
