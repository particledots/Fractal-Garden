// The 64 steps of one channel, and how they are saved.
//
// Step data is saved with the Logic project but is NOT automatable (the control list says so). The fields below are the ones the
// editor tabs show: Note, Gate Length, Slew, Ratchet and Mod (a value, plus for the Shapes mode a shape and a rate), and on / off.
// The ranges are provisional until the milestone that makes each one work (4 for Note and on/off, 5 for Gate, Slew, Ratchet and Mod);
// the saved bytes start with a version number so later milestones can widen them without breaking saved projects.

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
    inline constexpr int kGateMax = 100;       // percent of the step
    inline constexpr int kSlewMax = 100;       // percent
    inline constexpr int kRatchetMax = 8;      // 1 = no ratchet
    inline constexpr int kModMax = 100;        // percent
    inline constexpr int kNumModShapes = 8;    // our own eight: sine, triangle, ramp up, ramp down, square, pulse, pluck, stairs
    inline constexpr int kNumModRates = 8;     // milestone 2 shows these eight captions; the full list comes with milestone 5

    inline constexpr const char* kModShapeNames[] = { "SINE", "TRI", "RAMP UP", "RAMP DN", "SQUARE", "PULSE", "PLUCK", "STAIRS" };
    inline constexpr const char* kModRateCaptions[] = { "x1", "x2", "x4", "x2 ratchet", "x4 ratchet", "1/2 first half", "x3 one-shot", "x1 one-shot" };

    struct Step
    {
        std::uint8_t note = 12;          // octave 1, degree 0: the key note at MIDI 60 (C3 in Logic) for Key C, Root 0
        std::uint8_t gate = 50;
        std::uint8_t slew = 0;
        std::uint8_t ratchet = 1;
        std::uint8_t mod = 0;
        std::uint8_t modShape = 0;
        std::uint8_t modRate = 0;
        bool on = true;

        void clamp();                                  // pull every field into its range
        bool operator== (const Step&) const;
    };

    struct StepData
    {
        std::array<Step, kNumSteps> steps;

        // The sample pattern the editor shows in milestone 2 (the pattern of pictures A and B for steps 1 to 24, then varied steps).
        static StepData makeSample();

        // Saved form: [version][count][count x 8 bytes]. Version 2 (0.4.0) stores each note as octave * 12 + degree; version 1 (0.2.0 and
        // 0.3.0) stored plain MIDI notes, which are converted to the nearest degree of C major when loaded. fromBytes returns false (and
        // changes nothing) for anything it does not understand.
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
}
