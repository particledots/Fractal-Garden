#include <fg/Perform.h>

#include <fg/Mutate.h>
#include <fg/Pitch.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace fg
{
    namespace
    {
        constexpr const char* kOrnamentNames[kNumOrnaments] = {
            "Anticipation", "Suspension", "Syncopation", "Octave Up", "Fifth Up", "Half Turn Toward", "Half Turn Away",
            "Run Toward", "Run Away", "Turn", "Arp Toward", "Arp Away", "Mordent Up", "Mordent Down", "Trill" };

        // Semitones above the scale's lowest note for a running degree number.
        int semitonesOf (int degree, int scale)
        {
            const int size = scaleSize (scale);
            return (degree / size) * 12 + scaleInterval (scale, degree % size);
        }
    }

    //==========================================================================
    double perfChance (double knob)
    {
        if (knob <= 0.0)
            return 0.0;
        if (knob <= kPerfZoneSplit)
            return knob / kPerfZoneSplit;
        return 1.0;
    }

    bool perfInZone2 (double knob)
    {
        return knob > kPerfZoneSplit;
    }

    int perfFixedRatchet (double knob)
    {
        if (! perfInZone2 (knob))
            return -1;
        const int band = (int) std::floor ((knob - kPerfZoneSplit) / (1.0 - kPerfZoneSplit) * (double) kNumRatchets);
        return std::clamp (band, 0, kRatchetMax);
    }

    int perfFixedSlew (double knob)
    {
        if (! perfInZone2 (knob))
            return -1;
        return std::clamp ((int) std::lround ((knob - kPerfZoneSplit) / (1.0 - kPerfZoneSplit) * (double) kSlewMax), 0, kSlewMax);
    }

    double perfOrnamentChance (double knob)
    {
        if (! perfInZone2 (knob))
            return 0.0;
        return std::min (1.0, (knob - kPerfZoneSplit) / (1.0 - kPerfZoneSplit));
    }

    int perfOrnamentsOffered (double knob)
    {
        if (! perfInZone2 (knob))
            return 0;
        if (knob <= kPerfTwoNoteTop)
            return 7;
        if (knob <= kPerfFourNoteTop)
            return 14;
        return kNumOrnaments;
    }

    //==========================================================================
    const char* ornamentName (int kind)
    {
        return kOrnamentNames[std::clamp (kind, 0, kNumOrnaments - 1)];
    }

    int ornamentNotes (int kind)
    {
        if (kind <= kOrnHalfTurnAway)
            return 2;
        if (kind <= kOrnMordentDown)
            return 4;
        return 8;
    }

    int ornamentSteps (int kind)
    {
        return ornamentNotes (kind) / 2;
    }

    int ornamentPlan (int kind, int cur, int next, int prev, int scale, int out[kMaxOrnamentNotes])
    {
        const int size = scaleSize (scale);
        const int top = degreeCount (scale) - 1;
        const int c = std::clamp (cur, 0, top);
        const int dir = (next >= 0 && next != c) ? (next > c ? 1 : -1) : 1;      // toward the next step (up when there is none, or it is the same)
        int notes[kMaxOrnamentNotes];
        int count = 0;

        auto put = [&] (int v) { notes[count++] = v; };
        switch (kind)
        {
            case kOrnAnticipation:    if (next < 0) return 0; put (c); put (next); break;
            case kOrnSuspension:      if (prev < 0) return 0; put (prev); put (c); break;
            case kOrnSyncopation:     put (kOrnRest); put (c); break;
            case kOrnOctaveUp:        put (c); put (c + size); break;
            case kOrnFifthUp:
            {
                // the degree above this one that is nearest to seven semitones higher
                const int target = semitonesOf (c, scale) + 7;
                int best = std::min (top, c + 1), bestDistance = 1 << 20;
                for (int d = c + 1; d <= std::min (top, c + size + 1); ++d)
                {
                    const int distance = std::abs (semitonesOf (d, scale) - target);
                    if (distance < bestDistance) { bestDistance = distance; best = d; }
                }
                put (c); put (best);
                break;
            }
            case kOrnHalfTurnToward:  if (next < 0) return 0; put (c); put (next + dir); break;
            case kOrnHalfTurnAway:    put (c); put (c - dir); break;
            case kOrnRunToward:       for (int i = 0; i < 4; ++i) put (c + i * dir); break;
            case kOrnRunAway:         for (int i = 0; i < 4; ++i) put (c - i * dir); break;
            case kOrnTurn:            put (c); put (c + 1); put (c - 1); put (c); break;
            case kOrnArpToward:       for (int i = 0; i < 4; ++i) put (c + 2 * i * dir); break;
            case kOrnArpAway:         for (int i = 0; i < 4; ++i) put (c - 2 * i * dir); break;
            case kOrnMordentUp:       put (c); put (c + 1); put (c); put (c + 1); break;
            case kOrnMordentDown:     put (c); put (c - 1); put (c); put (c - 1); break;
            case kOrnTrill:           for (int i = 0; i < 8; ++i) put (i % 2 == 0 ? c : c + 1); break;
            default:                  return 0;
        }
        for (int i = 0; i < count; ++i)
            out[i] = notes[i] == kOrnRest ? kOrnRest : std::clamp (notes[i], 0, top);
        return count;
    }

    //==========================================================================
    PerformResult performStep (const Step& stored, long long k, std::uint32_t seed, int scale, const PerformKnobs& knobs)
    {
        PerformResult r;
        r.step = stored;
        r.stepIfNoOrnament = stored;
        if (! knobs.any())
            return r;

        // The dice for this step: always the same draws in the same order, so what a step gets depends only on (seed, k).
        Rng rng ((std::uint64_t) seed * 0x100000001b3ULL ^ ((std::uint64_t) k * 0xc2b2ae3d27d4eb4fULL) ^ 0x7e57ULL);
        rng.next();
        const double ratchetRoll = rng.unit();
        const int ratchetPick = rng.below (4);                 // x2 x3 x4 x6
        const double slewRoll = rng.unit();
        const int slewValue = 10 + rng.below (91);
        const double mutateRoll = rng.unit();
        const double kindRoll = rng.unit();
        const double ornamentRoll = rng.unit();
        const double ornamentPick = rng.unit();
        const double fallbackRoll = rng.unit();

        Step plain = stored, fallback = stored;
        const double mk = knobs.mutate;
        if (mk > 0.0)
        {
            if (! perfInZone2 (mk))
            {
                if (mutateRoll < perfChance (mk))
                {
                    if (kindRoll < 0.5) mutateNote (plain, scale, rng);
                    else                mutateGate (plain, rng);
                }
                fallback = plain;
            }
            else
            {
                const double p = perfOrnamentChance (mk);
                if (fallbackRoll < p)
                    mutateNote (fallback, scale, rng);
                if (ornamentRoll < p)
                {
                    const int offered = perfOrnamentsOffered (mk);
                    r.ornament = std::min (offered - 1, (int) (ornamentPick * (double) offered));   // one of the offered ornaments (the first `offered` kinds)
                }
                else
                    plain = fallback;
            }
        }
        plain.clamp();
        fallback.clamp();

        // Ratchet and slew apply on top, to both versions.
        auto applyRatchetAndSlew = [&] (Step& s)
        {
            const double rk = knobs.ratchet;
            if (rk > 0.0)
            {
                if (perfInZone2 (rk))
                    s.ratchet = (std::uint8_t) perfFixedRatchet (rk);
                else if (ratchetRoll < perfChance (rk))
                    s.ratchet = (std::uint8_t) (1 + ratchetPick);
            }
            const double sk = knobs.slew;
            if (sk > 0.0)
            {
                if (perfInZone2 (sk))
                    s.slew = (std::uint8_t) perfFixedSlew (sk);
                else if (slewRoll < perfChance (sk))
                    s.slew = (std::uint8_t) slewValue;
            }
            s.clamp();
        };
        applyRatchetAndSlew (plain);
        applyRatchetAndSlew (fallback);

        r.step = plain;
        r.stepIfNoOrnament = fallback;
        return r;
    }
}
