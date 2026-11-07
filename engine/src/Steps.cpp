#include <fg/Steps.h>

#include <algorithm>
#include <cmath>

namespace fg
{

namespace
{
    constexpr std::uint8_t kFormatVersion = 3;       // 3: ratchet is an index, sixteen Mod rates; 2: notes are octave * 12 + degree; 1 (0.2.0, 0.3.0): MIDI notes
    constexpr std::size_t kBytesPerStep = 8;

    std::uint8_t clampTo (int v, int lo, int hi) { return (std::uint8_t) std::min (hi, std::max (lo, v)); }

    // Versions 1 and 2 stored the ratchet as a count 1 to 8. The nearest amount of the new list (a tie goes to the smaller amount).
    int ratchetIndexForCount (int count)
    {
        int best = 0, bestDistance = 1000;
        for (int i = 0; i < kNumRatchets; ++i)
        {
            const int d = std::abs (kRatchetAmounts[i] - count);
            if (d < bestDistance) { bestDistance = d; best = i; }
        }
        return best;
    }

    // Versions 1 and 2 had eight Mod rates: x1, x2, x4, x2 ratchet, x4 ratchet, 1/2 first half, x3 one-shot, x1 one-shot.
    int modRateFromOld (int old)
    {
        static const int map[8] = { 0, 1, 5, 2, 6, 8, 3, 0 };
        return map[std::min (7, std::max (0, old))];
    }
}

void Step::clamp()
{
    note     = clampTo (note, kNoteMin, kNoteMax);
    gate     = clampTo (gate, kGateMin, kGateMax);
    slew     = clampTo (slew, 0, kSlewMax);
    ratchet  = clampTo (ratchet, 0, kRatchetMax);
    mod      = clampTo (mod, 0, kModMax);
    modShape = clampTo (modShape, 0, kNumModShapes - 1);
    modRate  = clampTo (modRate, 0, kNumModRates - 1);
}

bool Step::operator== (const Step& o) const
{
    return note == o.note && gate == o.gate && slew == o.slew && ratchet == o.ratchet && mod == o.mod
        && modShape == o.modShape && modRate == o.modRate && on == o.on;
}

bool StepData::operator== (const StepData& o) const
{
    for (int i = 0; i < kNumSteps; ++i)
        if (! (steps[(std::size_t) i] == o.steps[(std::size_t) i]))
            return false;
    return true;
}

StepData StepData::makeSample()
{
    // Picture A: steps 1 to 16 (the notes of D Dorian, which are the notes of C major), step 6 and step 12 off. The notes below are MIDI
    // notes; each is turned into the nearest degree of C major, which is exact for these.
    static const int notesA[16] = { 74, 81, 86, 79, 76, 91, 98, 84, 74, 76, 81, 86, 93, 98, 100, 79 };
    static const int onA[16]    = { 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 };
    // Picture B: steps 17 to 24, the Mod tab in Shapes mode; step 22 off.
    static const int shapesB[8] = { 0, 2, 6, 4, 1, 3, 7, 5 };
    static const int ratesB[8]  = { 0, 2, 0, 6, 1, 8, 0, 3 };      // x1, x2 ratchet, x1, x4 ratchet, x2 one-shot, 1/2 first half, x1, x3 one-shot
    static const int onB[8]     = { 1, 1, 1, 1, 1, 0, 1, 1 };

    StepData d;
    for (int i = 0; i < kNumSteps; ++i)
    {
        Step s;
        if (i < 16)       { s.note = (std::uint8_t) nearestMajorStored (notesA[i]); s.on = onA[i] != 0; }
        else              { s.note = (std::uint8_t) nearestMajorStored (60 + (i * 5) % 24); s.on = (i % 7) != 3; }
        s.gate     = (std::uint8_t) (25 + (i * 11) % 76);
        s.slew     = (std::uint8_t) ((i % 3 == 0) ? (i * 13) % 60 : 0);
        s.ratchet  = (std::uint8_t) ((i % 5 == 0) ? 1 : 0);          // x2 on every fifth step
        s.mod      = (std::uint8_t) ((i * 17) % 101);
        s.modShape = (std::uint8_t) (i % kNumModShapes);
        s.modRate  = (std::uint8_t) ((i / 2) % kNumModRates);
        if (i >= 16 && i < 24)
        {
            s.modShape = (std::uint8_t) shapesB[i - 16];
            s.modRate  = (std::uint8_t) ratesB[i - 16];
            s.on = onB[i - 16] != 0;
        }
        s.clamp();
        d.steps[(std::size_t) i] = s;
    }
    return d;
}

std::vector<std::uint8_t> StepData::toBytes() const
{
    std::vector<std::uint8_t> out;
    out.reserve (2 + (std::size_t) kNumSteps * kBytesPerStep);
    out.push_back (kFormatVersion);
    out.push_back ((std::uint8_t) kNumSteps);
    for (const auto& s : steps)
    {
        out.push_back (s.note);
        out.push_back (s.gate);
        out.push_back (s.slew);
        out.push_back (s.ratchet);
        out.push_back (s.mod);
        out.push_back (s.modShape);
        out.push_back (s.modRate);
        out.push_back (s.on ? 1 : 0);
    }
    return out;
}

bool StepData::fromBytes (const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 2 || (data[0] != kFormatVersion && data[0] != 2 && data[0] != 1))
        return false;
    const int version = data[0];
    const bool oldMidiNotes = version == 1;
    const bool oldRatchetAndRates = version < 3;
    const std::size_t count = data[1];
    if (count == 0 || count > (std::size_t) kNumSteps || size != 2 + count * kBytesPerStep)
        return false;

    StepData fresh;                               // steps not in the saved data keep their default values
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint8_t* p = data + 2 + i * kBytesPerStep;
        Step s;
        s.note = oldMidiNotes ? (std::uint8_t) nearestMajorStored (p[0]) : p[0];
        s.gate = p[1]; s.slew = p[2];
        s.ratchet = oldRatchetAndRates ? (std::uint8_t) ratchetIndexForCount (p[3]) : p[3];
        s.mod = p[4]; s.modShape = p[5];
        s.modRate = oldRatchetAndRates ? (std::uint8_t) modRateFromOld (p[6]) : p[6];
        s.on = p[7] != 0;
        s.clamp();
        fresh.steps[i] = s;
    }
    *this = fresh;
    return true;
}

void rotateSteps (StepData& data, int length, int direction)
{
    const int L = std::clamp (length, 1, kNumSteps);
    if (L < 2 || direction == 0)
        return;
    const StepData before = data;
    for (int i = 0; i < L; ++i)
    {
        const int from = direction > 0 ? (i + 1) % L : (i + L - 1) % L;
        data.steps[(std::size_t) i] = before.steps[(std::size_t) from];
    }
}

void transposeSteps (StepData& data, int length, int scaleSteps, int scale)
{
    const int L = std::clamp (length, 1, kNumSteps);
    for (int i = 0; i < L; ++i)
    {
        auto& s = data.steps[(std::size_t) i];
        s.note = (std::uint8_t) degreeToStored (storedToDegree (s.note, scale) + scaleSteps, scale);
    }
}

bool resizeShrink (StepData& data, int& length)
{
    const int L = std::clamp (length, 1, kNumSteps);
    if (L < 2)
        return false;
    const int newLength = (L + 1) / 2;
    const StepData before = data;
    for (int i = 0; i < newLength; ++i)
        data.steps[(std::size_t) i] = before.steps[(std::size_t) (2 * i)];
    length = newLength;
    return true;
}

bool resizeExpand (StepData& data, int& length)
{
    const int L = std::clamp (length, 1, kNumSteps);
    if (L * 2 > kNumSteps)
        return false;
    const StepData before = data;
    for (int i = 0; i < L; ++i)
    {
        data.steps[(std::size_t) (2 * i)] = before.steps[(std::size_t) i];
        Step gap = before.steps[(std::size_t) i];
        gap.on = false;
        data.steps[(std::size_t) (2 * i + 1)] = gap;
    }
    length = L * 2;
    return true;
}

} // namespace fg
