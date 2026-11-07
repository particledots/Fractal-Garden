#include <fg/Mod.h>

#include <algorithm>
#include <cmath>

namespace fg
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kJustBeforeOne = 0.9999;

        double unit (double v) { return std::min (1.0, std::max (0.0, v)); }
    }

    ModRate modRateInfo (int rate)
    {
        // The manual's table, row by row: x1 one-shot, x2 one-shot, x2 ratchet, x3 one-shot, x3 ratchet, x4 one-shot, x4 ratchet, x8 ratchet,
        // /2 first and second half, /4 four quarters, fixed 0 V to 2.5 V, fixed 2.5 V to 5 V.
        static const ModRate table[kNumModRates] = {
            { kRateOneShot, 1, 0 }, { kRateOneShot, 2, 0 }, { kRateRatchet, 2, 0 }, { kRateOneShot, 3, 0 },
            { kRateRatchet, 3, 0 }, { kRateOneShot, 4, 0 }, { kRateRatchet, 4, 0 }, { kRateRatchet, 8, 0 },
            { kRatePart, 2, 0 }, { kRatePart, 2, 1 }, { kRatePart, 4, 0 }, { kRatePart, 4, 1 },
            { kRatePart, 4, 2 }, { kRatePart, 4, 3 }, { kRateFixed, 1, 0 }, { kRateFixed, 1, 1 } };
        return table[std::min (kNumModRates - 1, std::max (0, rate))];
    }

    double modShapeValue (int shape, double t)
    {
        t = std::min (kJustBeforeOne, std::max (0.0, t));
        switch (shape)
        {
            case 0:  return 0.5 - 0.5 * std::cos (2.0 * kPi * t);          // sine
            case 1:  return 1.0 - std::abs (2.0 * t - 1.0);                // triangle
            case 2:  return t;                                             // ramp up
            case 3:  return 1.0 - t;                                       // ramp down
            case 4:  return t < 0.5 ? 1.0 : 0.0;                           // square
            case 5:  return t < 0.25 ? 1.0 : 0.0;                          // pulse
            case 6:  return std::exp (-4.0 * t);                           // pluck
            default: return std::min (3.0, std::floor (t * 4.0)) / 3.0;    // stairs
        }
    }

    double shapesModValue (const Step& step, double t)
    {
        if (! step.on)
            return 0.0;
        t = unit (t);
        const ModRate r = modRateInfo (step.modRate);
        switch (r.kind)
        {
            case kRateOneShot:
            {
                const double u = t * (double) r.n;
                return u < 1.0 ? modShapeValue (step.modShape, u) : modShapeValue (step.modShape, kJustBeforeOne);
            }
            case kRateRatchet:
            {
                const double u = t * (double) r.n;
                return modShapeValue (step.modShape, t >= 1.0 ? kJustBeforeOne : u - std::floor (u));
            }
            case kRatePart:
                return modShapeValue (step.modShape, (t + (double) r.part) / (double) r.n);
            default:
                return 0.5 * (double) r.part + 0.5 * ((double) std::min (7, (int) step.modShape) / 7.0);
        }
    }

    bool shapesModMoves (const Step& step)
    {
        return step.on && modRateInfo (step.modRate).kind != kRateFixed;
    }

    double envelopeModValue (int modPercent, double t)
    {
        t = unit (t);
        const double peak = unit ((double) modPercent / (double) kModMax);
        if (peak <= 0.0)
            return 1.0 - t;                                // instant attack, long decay
        if (t < peak)
            return t / peak;
        if (peak >= 1.0)
            return 1.0;                                    // the peak is the very end of the step
        return (1.0 - t) / (1.0 - peak);
    }

    double smoothModValue (double from, double to, double t)
    {
        return from + (to - from) * unit (t);
    }

    double stepLevel (const Step& step)
    {
        return step.on ? unit ((double) step.mod / (double) kModMax) : 0.0;
    }

    int modToCc (double value)
    {
        return std::min (127, std::max (0, (int) std::lround (unit (value) * 127.0)));
    }
}
