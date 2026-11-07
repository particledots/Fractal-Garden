// The sequencer clock and the trunk player (milestone 3).
//
// Plain C++, no plugin code. Each audio block the wrapper tells it what Logic says (playing or not, the song position in quarter
// notes at the start of the block, tempo, time signature) and which RATE, RATIO and LENGTH are set, and it answers with the notes
// to start and stop, each with its sample position inside the block.
//
// The position in the pattern is worked out from Logic's song position, never from a counter of our own. Step number
// k = (song position) / (step length), and the step that plays is k modulo LENGTH. That is what keeps the plugin locked to Logic:
// start from anywhere, jump to another bar, cycle, and two instances on two tracks always agree, without talking to each other.
//
// Milestone 4: the trunk plays in any of the 11 orders (fg/Orders.h), each step's stored place in the scale becomes a MIDI note
// through Scale, Key and Root (fg/Pitch.h) as the note starts, and RESET restarts the order.
//
// Milestone 5: per-step gate length with ties, ratchets, slew (CC 84) and the Mod output (CC 1), see below.
//  - GATE: 5 % to 100 % of the step. A step at 100 % is TIED to the next one: the next note starts first and the old note ends right
//    after it (same moment, that order), so a mono synth plays them legato. If the next step plays the same pitch there is no new note
//    at all (no re-trigger). A tie into an OFF step ends at the start of that step. A ratcheted step is never tied.
//  - RATCHET: the step is divided into x1 x2 x3 x4 x6 x8 x16 x32 equal slots, each with a note of half its length (never shorter than
//    1 ms). The gate length does not apply to a ratcheted step.
//  - SLEW: at the start of each step that plays, CC 84 carries the step's slew (0 to 100 % as 0 to 127), before the note. A step whose
//    NEXT step has slew above 0 is also held to the next step (the same legato overlap as a tie), so a mono synth can glide into it.
//  - MOD: CC 1 follows the Mod mode (fg/Mod.h). Moving values are sampled every 10 ms; only changes are sent.
//  - Cap: at most 100 CC messages (CC 84 and CC 1 together) in any one second.
//  - In Velocity mode the step's Mod value is also the note's velocity (1 to 127); in the other modes the velocity is 100.
//  CC messages are only produced when PlayerSettings::sendCc is on (the plugin turns it on).
//
// RESET: the step that starts next is played as step 1 of the order and counting goes on from there. Because the order is worked out
// from (step counter - anchor), nothing else changes. The anchor is forgotten whenever Logic stops, jumps or cycles, so starting
// playback always locks the pattern back to the song's bars.

#pragma once

#include <fg/Mod.h>
#include <fg/Orders.h>
#include <fg/Pitch.h>
#include <fg/Steps.h>

#include <atomic>
#include <cstdint>

namespace fg
{
    // What Logic says at the start of an audio block.
    struct HostBlock
    {
        bool playing = false;
        bool havePpq = false;       // false: Logic gave no song position (the player then treats the block as "not running")
        double ppq = 0.0;           // song position in quarter notes at the start of the block (bar 1 beat 1 = 0)
        double bpm = 120.0;         // <= 0 means unknown; 120 is used
        int sigNum = 4, sigDen = 4; // time signature (only "1 bar" depends on it)
    };

    // The controls that set the speed, the loop, the order and the pitch.
    struct PlayerSettings
    {
        int rate = kDefaultRate;     // index into kRateNames
        int ratio = kDefaultRatio;   // index into kRatioNames
        int length = 8;              // 1 to 64

        int order = kOrderForward;   // index into kOrderNames
        bool pendRepeat = false;     // Pendulum: the two end steps play twice
        std::uint32_t seed = 1;      // for the random orders
        ChanceMix chance;            // for Chance mix

        int scale = 0;               // index into kScaleNames
        int key = 0;                 // 0 = C ... 11 = B
        int root = 0;                // scale steps, -24 to +24

        int modMode = 0;             // index into kModModeNames: Shapes, Velocity, Envelope, Smooth
        bool sendCc = false;         // produce the CC 84 (slew) and CC 1 (mod) messages as well as the notes

        std::uint32_t resetCount = 0;   // goes up by one each time RESET is pressed
    };

    // One thing to send. A note event (cc < 0) starts or stops a note; a CC event (cc >= 0) sends controller number cc with value 0 to 127.
    struct NoteEvent
    {
        int offset = 0;              // sample position inside the block, 0 to numSamples - 1
        bool on = false;
        int note = 60;
        int velocity = 100;
        int cc = -1;
        int value = 0;
    };

    inline constexpr int kPlayerChannel = 1;       // the MIDI channel (a settings page comes in milestone 9)
    inline constexpr int kPlayerVelocity = 100;
    inline constexpr double kRelocateTolerance = 0.02;   // quarter notes: a bigger difference than this from where the block should have started counts as a jump
    inline constexpr double kMinGateFraction = 0.05;     // the gate is never shorter than 5 % of the step
    inline constexpr double kRatchetPulseFraction = 0.5; // a ratchet note is half of its slot (the manual's default "fixed pulse width")
    inline constexpr double kMinNoteSeconds = 0.001;     // no note is shorter than 1 ms (unless its slot is)
    inline constexpr double kModTickSeconds = 0.01;      // moving Mod values are sampled every 10 ms
    inline constexpr int kMaxCcPerSecond = 100;          // CC 84 and CC 1 together, in any one second

    // How long one step lasts, in quarter notes, for a RATE, a RATIO and Logic's time signature. "1 bar" is one bar of that signature
    // (4/4 = 4 quarters, 6/8 = 3 quarters). RATIO x2 makes the step half as long, /2 twice as long.
    double stepLengthQuarters (int rateIndex, int ratioIndex, int sigNum, int sigDen);

    class TrunkPlayer
    {
    public:
        // Starts over without sending anything (a fresh instance, or after the wrapper has dealt with sounding notes itself).
        void reset();

        // One audio block. Returns how many events were written to out (never more than maxOut; a note-off is never dropped for
        // lack of room: a note-on that does not fit is skipped together with its note-off).
        int process (double sampleRate, int numSamples, const HostBlock&, const PlayerSettings&, const StepData&, NoteEvent* out, int maxOut);

        // The note-off for a note that is sounding, if any (used when the plugin is bypassed). Returns 0 or 1.
        int releaseHeld (NoteEvent* out, int maxOut);

        // ---- what the window shows (safe to read from any thread) ----
        int currentStep() const        { return shownStep.load (std::memory_order_relaxed); }       // 0 to 63, or -1 when not running
        bool gateOpen() const          { return held.load (std::memory_order_relaxed); }            // a note is sounding
        std::uint32_t stepCount() const { return stepsStarted.load (std::memory_order_relaxed); }   // goes up at every step (on or off)
        std::uint32_t noteCount() const { return notesStarted.load (std::memory_order_relaxed); }   // goes up at every note-on
        bool running() const           { return isRunning.load (std::memory_order_relaxed); }

    private:
        bool wasRunning = false;
        bool haveResetCount = false;
        std::uint32_t seenResetCount = 0;
        long long anchorK = 0;               // RESET: the step counter value that counts as step 1 of the order (0 = locked to the bars)
        double expectedPpq = 0.0;            // where the next block should start if nothing jumped
        double lastBoundaryPpq = 0.0;        // song position of the last step start handled (so none is played twice or lost)
        double gateOffPpq = -1.0;            // song position of the sounding note's note-off, or -1 (none: it ends at the next step)
        int heldNote = 0;

        // The step that is playing now (all in quarter notes of the song).
        bool stepActive = false;
        Step curStep;
        double curStart = 0.0, curLen = 0.25;
        double curPrevLevel = 0.0;           // Smooth: where the glide starts
        int curPulses = 1, nextPulse = 1;    // ratchet: how many notes in the step, and which one starts next
        double curSlot = 0.25, curPulseWidth = 0.0;
        int curPulseNote = 60, curPulseVelocity = 100;

        // CC output
        int lastCc1 = -1, lastCc84 = -1;     // the last values sent (-1: send the next one whatever it is)
        long long sampleClock = 0;           // samples handled so far (this is what the 10 ms grid and the 100 per second cap count in)
        long long nextTickAbs = 0;           // the next 10 ms tick
        long long ccRing[kMaxCcPerSecond] = {};
        int ccRingPos = 0, ccRingCount = 0;

        bool modMoves (const PlayerSettings&) const;
        double modLevel (const PlayerSettings&, double t) const;

        std::atomic<bool> held { false };
        std::atomic<bool> isRunning { false };
        std::atomic<int> shownStep { -1 };
        std::atomic<std::uint32_t> stepsStarted { 0 }, notesStarted { 0 };
    };
}
