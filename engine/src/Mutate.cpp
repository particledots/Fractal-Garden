#include <fg/Mutate.h>

#include <algorithm>

namespace fg
{

std::uint64_t Rng::next()
{
    state += 0x9e3779b97f4a7c15ULL;
    std::uint64_t x = state;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

double Rng::unit()
{
    return (double) (next() >> 11) * (1.0 / 9007199254740992.0);
}

int Rng::below (int n)
{
    if (n < 2)
        return 0;
    return std::min (n - 1, (int) (unit() * (double) n));
}

namespace
{
    // How likely each ratchet amount is when one is drawn at random (x1 x2 x3 x4 x6 x8 x16 x32), in percent. Triplet amounts (x3, x6) are included.
    constexpr int kRatchetWeights[kNumRatchets] = { 30, 20, 14, 14, 8, 8, 4, 2 };

    constexpr int kReseedGates[9] = { 10, 25, 40, 50, 50, 60, 75, 90, 100 };

    int weightedRatchet (Rng& rng, bool allowOne)
    {
        int total = 0;
        for (int i = allowOne ? 0 : 1; i < kNumRatchets; ++i)
            total += kRatchetWeights[i];
        int pick = rng.below (total);
        for (int i = allowOne ? 0 : 1; i < kNumRatchets; ++i)
        {
            if (pick < kRatchetWeights[i])
                return i;
            pick -= kRatchetWeights[i];
        }
        return 0;
    }

    int newSlew (Rng& rng, double zeroChance)
    {
        return rng.unit() < zeroChance ? 0 : 10 + rng.below (91);
    }
}

double mutateNoteChance (double knob)
{
    if (knob <= 0.0)
        return 0.0;
    if (knob <= 0.5)
        return knob * 2.0;
    return 0.75;
}

double mutateGateChance (double knob)
{
    if (knob <= 0.5)
        return 0.0;
    return std::min (1.0, (knob - 0.5) * 2.0);
}

bool mutateNote (Step& s, int scale, Rng& rng)
{
    const int degree = storedToDegree (s.note, scale);
    int amount;
    if (rng.unit() < 0.2)
        amount = scaleSize (scale);                  // an octave
    else
    {
        const double r = rng.unit();
        amount = r < 0.5 ? 1 : (r < 0.8 ? 2 : 3);
    }
    const int direction = rng.unit() < 0.5 ? -1 : 1;
    int stored = degreeToStored (degree + direction * amount, scale);
    if (stored == s.note)
        stored = degreeToStored (degree - direction * amount, scale);      // stuck against an end: go the other way
    if (stored == s.note)
        return false;
    s.note = (std::uint8_t) stored;
    return true;
}

bool mutateGate (Step& s, Rng& rng)
{
    if (rng.unit() < 0.7)
    {
        int gate = s.gate;
        for (int tries = 0; tries < 6 && gate == s.gate; ++tries)
            gate = kGateMin + rng.below (kGateMax - kGateMin + 1);
        if (gate == s.gate)
            return false;
        s.gate = (std::uint8_t) gate;
        return true;
    }
    s.on = ! s.on;
    return true;
}

bool defaultMutate (Step& s, double knob, int scale, Rng& rng)
{
    bool changed = false;
    const double noteChance = mutateNoteChance (knob), gateChance = mutateGateChance (knob);
    if (noteChance > 0.0 && rng.unit() < noteChance)
        changed = mutateNote (s, scale, rng) || changed;
    if (gateChance > 0.0 && rng.unit() < gateChance)
        changed = mutateGate (s, rng) || changed;
    return changed;
}

int randomRatchet (Rng& rng)
{
    return weightedRatchet (rng, true);
}

bool microMutate (Step& s, double ratchetChance, double slewChance, double modChance, int modMode, Rng& rng)
{
    bool changed = false;
    if (ratchetChance > 0.0 && rng.unit() < ratchetChance)
    {
        const int r = weightedRatchet (rng, true);
        changed = changed || r != s.ratchet;
        s.ratchet = (std::uint8_t) r;
    }
    if (slewChance > 0.0 && rng.unit() < slewChance)
    {
        const int v = newSlew (rng, 0.4);
        changed = changed || v != s.slew;
        s.slew = (std::uint8_t) v;
    }
    if (modChance > 0.0 && rng.unit() < modChance)
    {
        if (modMode == 0)
        {
            const int shape = rng.below (kNumModShapes);
            changed = changed || shape != s.modShape;
            s.modShape = (std::uint8_t) shape;
        }
        else
        {
            const int v = rng.below (kModMax + 1);
            changed = changed || v != s.mod;
            s.mod = (std::uint8_t) v;
        }
    }
    return changed;
}

void reseedSteps (StepData& data, int first, int count, ReseedKind kind, int scale, Rng& rng, unsigned allMask)
{
    const int lo = std::clamp (first, 0, kNumSteps);
    const int hi = std::clamp (first + count, lo, kNumSteps);
    const bool all = kind == ReseedKind::All;
    for (int i = lo; i < hi; ++i)
    {
        Step& s = data.steps[(std::size_t) i];
        if ((all && (allMask & 1u)) || kind == ReseedKind::Note)
            s.note = (std::uint8_t) degreeToStored (rng.below (degreeCount (scale)), scale);
        if ((all && (allMask & 2u)) || kind == ReseedKind::Gate)
        {
            s.on = rng.unit() >= 0.2;
            s.gate = (std::uint8_t) kReseedGates[rng.below (9)];
        }
        if ((all && (allMask & 4u)) || kind == ReseedKind::Slew)
            s.slew = (std::uint8_t) newSlew (rng, 0.7);
        if ((all && (allMask & 8u)) || kind == ReseedKind::Ratchet)
            s.ratchet = (std::uint8_t) (rng.unit() < 0.75 ? 0 : weightedRatchet (rng, false));
        if ((all && (allMask & 16u)) || kind == ReseedKind::Mod)
        {
            s.mod = (std::uint8_t) rng.below (kModMax + 1);
            s.modShape = (std::uint8_t) rng.below (kNumModShapes);
            s.modRate = (std::uint8_t) (rng.unit() < 0.6 ? 0 : 1 + rng.below (kNumModRates - 1));
        }
        s.clamp();
    }
}

} // namespace fg
