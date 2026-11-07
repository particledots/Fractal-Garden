#include <fg/Pitch.h>

#include <algorithm>
#include <cstdlib>
#include <initializer_list>

namespace fg
{

namespace
{
    constexpr unsigned M (std::initializer_list<int> semitones)
    {
        unsigned m = 0;
        for (int s : semitones)
            m |= 1u << s;
        return m;
    }

    // Same 28 scales, same order, as kScaleNames in Params.h (the engine test checks the count and a few notes).
    constexpr unsigned kMasks[kNumScales] = {
        M ({ 0, 2, 4, 5, 7, 9, 11 }),                  // Major (Ionian)
        M ({ 0, 2, 3, 5, 7, 9, 10 }),                  // Dorian
        M ({ 0, 1, 3, 5, 7, 8, 10 }),                  // Phrygian
        M ({ 0, 2, 4, 6, 7, 9, 11 }),                  // Lydian
        M ({ 0, 2, 4, 5, 7, 9, 10 }),                  // Mixolydian
        M ({ 0, 2, 3, 5, 7, 8, 10 }),                  // Aeolian (Natural Minor)
        M ({ 0, 1, 3, 5, 6, 8, 10 }),                  // Locrian
        M ({ 0, 2, 3, 5, 7, 8, 11 }),                  // Harmonic Minor
        M ({ 0, 2, 3, 5, 7, 9, 11 }),                  // Melodic Minor
        M ({ 0, 2, 4, 5, 7, 8, 11 }),                  // Harmonic Major
        M ({ 0, 2, 3, 6, 7, 8, 11 }),                  // Hungarian Minor
        M ({ 0, 1, 4, 5, 7, 8, 10 }),                  // Phrygian Dominant
        M ({ 0, 1, 4, 5, 7, 8, 11 }),                  // Double Harmonic (Byzantine)
        M ({ 0, 1, 3, 5, 7, 8, 11 }),                  // Neapolitan Minor
        M ({ 0, 1, 3, 5, 7, 9, 11 }),                  // Neapolitan Major
        M ({ 0, 1, 4, 6, 8, 10, 11 }),                 // Enigmatic
        M ({ 0, 2, 4, 7, 9 }),                         // Major Pentatonic
        M ({ 0, 3, 5, 7, 10 }),                        // Minor Pentatonic
        M ({ 0, 3, 5, 6, 7, 10 }),                     // Blues
        M ({ 0, 2, 3, 7, 8 }),                         // Hirajoshi
        M ({ 0, 1, 5, 7, 10 }),                        // In Sen
        M ({ 0, 2, 3, 7, 9 }),                         // Kumoi
        M ({ 0, 1, 5, 6, 10 }),                        // Iwato
        M ({ 0, 2, 4, 6, 8, 10 }),                     // Whole Tone
        M ({ 0, 2, 3, 5, 6, 8, 9, 11 }),               // Octatonic (Whole-Half)
        M ({ 0, 1, 3, 4, 6, 7, 9, 10 }),               // Octatonic (Half-Whole)
        M ({ 0, 2, 4, 6, 9, 10 }),                     // Prometheus
        M ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }),  // Chromatic
    };

    int clampScale (int scale) { return std::clamp (scale, 0, kNumScales - 1); }

    int floorDiv (int a, int b)
    {
        int q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0)))
            --q;
        return q;
    }
    int floorMod (int a, int b) { return a - floorDiv (a, b) * b; }
}

unsigned scaleMask (int scale) { return kMasks[clampScale (scale)]; }

int scaleSize (int scale)
{
    const unsigned m = scaleMask (scale);
    int n = 0;
    for (int i = 0; i < 12; ++i)
        n += (int) ((m >> i) & 1u);
    return n;
}

int scaleInterval (int scale, int degree)
{
    const unsigned m = scaleMask (scale);
    const int n = scaleSize (scale);
    const int want = std::clamp (degree, 0, n - 1);
    int seen = 0;
    for (int i = 0; i < 12; ++i)
        if ((m >> i) & 1u)
        {
            if (seen == want)
                return i;
            ++seen;
        }
    return 0;
}

int degreeCount (int scale) { return kNumOctaves * scaleSize (scale); }

int storedToDegree (int stored, int scale)
{
    const int s = std::clamp (stored, 0, kStoredNoteMax);
    const int n = scaleSize (scale);
    const int octave = s / kDegreeSlots;
    const int degree = std::min (s % kDegreeSlots, n - 1);
    return octave * n + degree;
}

int degreeToStored (int degree, int scale)
{
    const int n = scaleSize (scale);
    const int d = std::clamp (degree, 0, kNumOctaves * n - 1);
    return (d / n) * kDegreeSlots + d % n;
}

int pitchFor (int stored, int scale, int key, int root)
{
    const int n = scaleSize (scale);
    const int running = storedToDegree (stored, scale) + root;
    const int octave = floorDiv (running, n);
    const int degree = floorMod (running, n);
    const int midi = kBaseMidiNote + std::clamp (key, 0, 11) + 12 * octave + scaleInterval (scale, degree);
    return std::clamp (midi, 0, 127);
}

int nearestMajorStored (int midiNote)
{
    static constexpr int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 };
    const int rel = std::clamp (midiNote - kBaseMidiNote, 0, kNumOctaves * 12 - 1);
    const int octave = rel / 12, semitone = rel % 12;
    int best = 0, bestDistance = 99;
    for (int d = 0; d < 7; ++d)
    {
        const int distance = std::abs (kMajor[d] - semitone);
        if (distance < bestDistance)            // a tie keeps the lower degree
        {
            bestDistance = distance;
            best = d;
        }
    }
    return octave * kDegreeSlots + best;
}

} // namespace fg
