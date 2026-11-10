// The 64 steps of one channel, and how they are saved.
//
// Step data is saved with the Logic project but is NOT automatable (the control list says so). The fields below are the ones the
// editor tabs show: Note, Gate Length, Slew, Ratchet and Mod (a value, plus for the Shapes mode a shape and a rate), and on / off.
//
// Milestone 5 fixes the ranges: Gate 5 to 100 % of the step (100 % is a tie), Slew 0 to 100 %, Ratchet an index into the manual's
// eight amounts (x1 x2 x3 x4 x6 x8 x16 x32), Mod 0 to 100 %, and sixteen Mod rates (the manual's table). The saved bytes start with
// a version number so older projects are converted when they are opened.

#pragma once

#include <fg/Params.h>
#include <fg/Pitch.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fg
{
    // A step's note is a place in the scale, not a MIDI note (see fg/Pitch.h): octave * 12 + degree, 0 to 59.
    inline constexpr int kNoteMin = 0;
    inline constexpr int kNoteMax = kStoredNoteMax;
    inline constexpr int kGateMin = 5;         // percent of the step (the manual: the minimum gate is 5 % of the clock rate)
    inline constexpr int kGateMax = 100;       // percent of the step; 100 % ties the note to the next step
    inline constexpr int kSlewMax = 100;       // percent
    inline constexpr int kModMax = 100;        // percent

    // Ratchet: the stored value is an index into this list (the manual's Ratchet Amounts). Index 0 is "x1", no ratchet.
    inline constexpr int kNumRatchets = 8;
    inline constexpr int kRatchetMax = kNumRatchets - 1;
    inline constexpr int kRatchetAmounts[kNumRatchets] = { 1, 2, 3, 4, 6, 8, 16, 32 };

    inline constexpr int kNumModShapes = 8;    // our own eight: sine, triangle, ramp up, ramp down, square, pulse, pluck, stairs
    inline constexpr const char* kModShapeNames[] = { "SINE", "TRI", "RAMP UP", "RAMP DN", "SQUARE", "PULSE", "PLUCK", "STAIRS" };

    // The sixteen Mod rates of the manual's Shapes table, in the manual's order (see fg/Mod.h for what each does).
    inline constexpr int kNumModRates = 16;
    inline constexpr const char* kModRateCaptions[kNumModRates] = {
        "x1", "x2 one-shot", "x2 ratchet", "x3 one-shot", "x3 ratchet", "x4 one-shot", "x4 ratchet", "x8 ratchet",
        "1/2 first half", "1/2 second half", "1/4 first qtr", "1/4 second qtr", "1/4 third qtr", "1/4 fourth qtr",
        "fixed 0-2.5V", "fixed 2.5-5V" };

    struct Step
    {
        std::uint8_t note = 12;          // octave 1, degree 0: the key note at MIDI 60 (C3 in Logic) for Key C, Root 0
        std::uint8_t gate = 50;
        std::uint8_t slew = 0;
        std::uint8_t ratchet = 0;        // index into kRatchetAmounts (0 = x1)
        std::uint8_t mod = 0;
        std::uint8_t modShape = 0;
        std::uint8_t modRate = 0;
        bool on = true;
        std::int16_t fine = 0;           // 1.2.0: fine pitch in cents (100 = a semitone) added to the scale note; only Pitch Snap SEMITONES / FREE use it

        void clamp();                                  // pull every field into its range
        bool operator== (const Step&) const;
    };

    struct StepData
    {
        std::array<Step, kNumSteps> steps;

        // The sample pattern the editor shows (the pattern of pictures A and B for steps 1 to 24, then varied steps).
        // Since 1.3.0 this is no longer what a new instance starts with (see makeDefault); it is kept for the automatic checks.
        static StepData makeSample();

        // What a new instance starts with since 1.3.0 (Bryn's choice, 9 Nov 2026): every step ON, the same note (octave 1, degree 0: C3 for
        // Key C, Root 0, Base note 48), gate 100 % (tied to the next step), no ratchet, no slew, Mod 0. The first RESEED is the starting point.
        static StepData makeDefault();

        // Saved form: [version][count][count x 10 bytes]. Version 4 (1.2.0) adds the fine pitch (two bytes, signed, little end first, after the
        // on / off byte); versions 1 to 3 have 8 bytes a step and open with a fine pitch of 0. Version 3 (0.5.0) stores the ratchet as an index and uses the sixteen Mod rates;
        // version 2 (0.4.0) stored the ratchet as a count 1 to 8 and eight Mod rates, and stored notes as octave * 12 + degree; version 1
        // (0.2.0 and 0.3.0) also stored plain MIDI notes, which are converted to the nearest degree of C major when loaded. Older data is
        // converted when loaded. fromBytes returns false (and changes nothing) for anything it does not understand.
        std::vector<std::uint8_t> toBytes() const;
        bool fromBytes (const std::uint8_t* data, std::size_t size);

        bool operator== (const StepData&) const;
    };

    // ---- editing the trunk (the ROTATE and TRANSPOSE buttons). Both change the step data for good, and only the first `length` steps. ----

    // direction +1 (RIGHT): the start point moves one step forward, so step 2's data becomes step 1 and step 1's goes to the end.
    // direction -1 (LEFT): the other way. Whole steps move (note, on/off, gate, slew, ratchet, mod).
    void rotateSteps (StepData&, int length, int direction);

    // Moves every note of the first `length` steps by a number of scale steps (+1 up, -1 down) in the given scale. A note that would go
    // past either end of the 5-octave range stops at the end.
    void transposeSteps (StepData&, int length, int scaleSteps, int scale);

    // ---- Resize (the RESIZE buttons, milestone 5). Both change the step data and `length` for good. They return true if something changed. ----

    // Shrink: the new length is half the old one, rounded up (8 to 4, 7 to 4, 2 to 1). The new step i is the old step 2i, so steps 1, 3, 5 ...
    // are kept. Nothing happens at length 1. Steps past the new length are left as they were.
    bool resizeShrink (StepData&, int& length);

    // The three ways Resize can expand (the manual's Resize Algorithm setting; the settings page's RESIZE METHOD). Spread is the default.
    enum class ResizeMethod { Spread = 0, Stretch = 1, Clone = 2 };

    // Expand ("Spread", the manual's default): the new length is twice the old one. The old step i moves to step 2i, and the step after it
    // (2i+1) becomes a copy of it that is switched OFF, so the gaps are empty. Nothing happens if twice the length would pass 64.
    // Stretch: the old step i moves to 2i and 2i+1 is an exact copy of it (the gap is filled with a repeat of the step).
    // Clone: the whole old sequence is repeated once after itself (new step L+i is a copy of step i).
    bool resizeExpand (StepData&, int& length, ResizeMethod method = ResizeMethod::Spread);

    // ---- the one-click buttons of 1.3.0. Both change `count` steps from `first` for good (steps outside 0 to 63 are skipped), the same span
    //      the RESEED buttons use (the page shown, or all 64). They return true if something changed. ----

    // ALL GATES 100 %: every step's gate length becomes 100 % (a tie into the next step). ON / OFF is not touched, nor anything else.
    bool setGatesFull (StepData&, int first, int count);

    // CLEAR RATCHETS: every step's ratchet becomes x1 (no ratchet). Nothing else is touched.
    bool clearRatchets (StepData&, int first, int count);
}
