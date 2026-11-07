// Scales, scale degrees and the pitch a stored note plays (milestone 4).
//
// A step does not store a MIDI note. It stores a place in the scale: an octave (0 to 4, five octaves) and a degree inside that octave.
// The stored byte is  octave * 12 + degree  (12 slots per octave, so the 12-note Chromatic scale fits). The pitch that is played is
// worked out at the moment the note starts, from the stored place and the current Scale, Key and Root. So changing Scale, Key or Root
// re-pitches the pattern at once and never changes the stored steps: switch from Major to Minor Pentatonic and back and nothing is lost.
//
// A step whose degree is higher than the number of notes in the current scale (degree 6 in a five-note scale) plays the scale's top
// degree; the stored degree is left alone, so it comes back when the scale does.
//
// The 28 scales are the same, in the same order and with the same notes, as Dotriaconta-tone and the Scripter scripts. Their note
// lists are the user's own table (carried over, nothing added).

#pragma once

#include <fg/Params.h>

#include <cstdint>

namespace fg
{
    inline constexpr int kNumOctaves = 5;
    inline constexpr int kBaseMidiNote = 48;                                  // C2 in Logic's naming (60 = C3): the lowest note at Key C, Root 0
    inline constexpr int kDegreeSlots = 12;                                   // stored note = octave * 12 + degree
    inline constexpr int kStoredNoteMax = kNumOctaves * kDegreeSlots - 1;     // 59

    // A scale as 12 bits, one per semitone above the key note (bit 0 = the key note itself).
    unsigned scaleMask (int scale);

    int scaleSize (int scale);                       // how many notes per octave (5 to 12)
    int scaleInterval (int scale, int degree);       // semitones above the key note for degree 0 .. scaleSize - 1
    int degreeCount (int scale);                     // degrees in the whole 5-octave range: 5 * scaleSize

    // Stored byte <-> a single running degree number 0 .. degreeCount - 1 (what Transpose and Root add to).
    int storedToDegree (int stored, int scale);
    int degreeToStored (int degree, int scale);      // clamps into the 5-octave range

    // The MIDI note (0 to 127) that a stored note plays for a Scale, a Key (0 = C ... 11 = B) and a Root (scale steps, may be negative).
    // baseNote is the MIDI note of the lowest place at Key C, Root 0 (the settings page's BASE NOTE; the default is kBaseMidiNote).
    int pitchFor (int stored, int scale, int key, int root, int baseNote = kBaseMidiNote);

    // For converting notes saved by versions 0.2.0 / 0.3.0 (which stored plain MIDI notes): the stored place of the nearest degree of C major.
    int nearestMajorStored (int midiNote);
}
