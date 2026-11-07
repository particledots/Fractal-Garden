#include <fg/Clock.h>

#include <algorithm>
#include <cmath>

namespace fg
{
    namespace
    {
        // Quarter notes per step for each entry of kRateNames (the "1 bar" entry is worked out from the time signature).
        constexpr double kRateQuarters[kNumRates] = { 0.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

        // How many times faster than the rate each entry of kRatioNames runs (/8 ... x8).
        constexpr double kRatioSpeed[kNumRatios] = { 1.0 / 8.0, 1.0 / 4.0, 1.0 / 3.0, 1.0 / 2.0, 1.0, 2.0, 3.0, 4.0, 8.0 };

        long long floorMod (long long k, int length)
        {
            const long long m = k % length;
            return m < 0 ? m + length : m;
        }
    }

    double stepLengthQuarters (int rateIndex, int ratioIndex, int sigNum, int sigDen)
    {
        const int r = std::clamp (rateIndex, 0, kNumRates - 1);
        const int q = std::clamp (ratioIndex, 0, kNumRatios - 1);
        const int num = sigNum > 0 ? sigNum : 4;
        const int den = sigDen > 0 ? sigDen : 4;
        const double base = r == 0 ? 4.0 * (double) num / (double) den : kRateQuarters[r];
        return base / kRatioSpeed[q];
    }

    //==========================================================================
    void TrunkPlayer::reset()
    {
        wasRunning = false;
        expectedPpq = 0.0;
        lastBoundaryPpq = 0.0;
        gateOffPpq = -1.0;
        heldNote = 0;
        held.store (false);
        isRunning.store (false);
        shownStep.store (-1);
    }

    int TrunkPlayer::releaseHeld (NoteEvent* out, int maxOut)
    {
        int n = 0;
        if (held.load() && maxOut > 0)
        {
            out[n++] = NoteEvent { 0, false, heldNote, 0 };
        }
        held.store (false);
        gateOffPpq = -1.0;
        wasRunning = false;           // the next block starts over (it is treated like a start)
        isRunning.store (false);
        shownStep.store (-1);
        return n;
    }

    int TrunkPlayer::process (double sampleRate, int numSamples, const HostBlock& host, const PlayerSettings& settings,
                              const StepData& data, NoteEvent* out, int maxOut)
    {
        int n = 0;
        const int lastSample = numSamples > 0 ? numSamples - 1 : 0;

        auto emit = [&] (int offset, bool on, int note, int velocity)
        {
            if (n < maxOut)
                out[n++] = NoteEvent { std::clamp (offset, 0, lastSample), on, note, velocity };
        };
        auto release = [&] (int offset)
        {
            if (held.load())
            {
                emit (offset, false, heldNote, 0);
                held.store (false);
            }
            gateOffPpq = -1.0;
        };

        const bool running = host.playing && host.havePpq && sampleRate > 0.0 && numSamples > 0;
        isRunning.store (running);

        if (! running)
        {
            // Logic is stopped (or gave no position): the sounding note ends now and the next start begins afresh.
            release (0);
            wasRunning = false;
            shownStep.store (-1);
            return n;
        }

        const int length = std::clamp (settings.length, 1, kNumSteps);
        const double bpm = host.bpm > 0.0 ? host.bpm : 120.0;
        const double pps = bpm / 60.0 / sampleRate;                 // quarter notes per sample
        const double ppq0 = host.ppq;
        const double ppq1 = ppq0 + (double) numSamples * pps;
        const double stepLen = stepLengthQuarters (settings.rate, settings.ratio, host.sigNum, host.sigDen);
        const double eps = 1e-7;
        auto toOffset = [&] (double p) { return (int) std::lround ((p - ppq0) / pps); };

        // A jump (relocate, cycle, or the song position changing by more than a block's worth): end the note, count again from here.
        const bool relocated = wasRunning && std::fabs (ppq0 - expectedPpq) > kRelocateTolerance;
        if (relocated)
            release (0);
        const bool continuing = wasRunning && ! relocated;

        // Where the position is now, for the window (the steps that start inside this block update it below).
        if (! continuing)
        {
            shownStep.store ((int) floorMod ((long long) std::floor ((ppq0 + eps) / stepLen), length));
            // "the last boundary handled" is the one just before the first boundary this run will handle
            lastBoundaryPpq = (double) ((long long) std::ceil ((ppq0 - eps) / stepLen) - 1) * stepLen;
        }

        // The first step boundary this block has to deal with. In a continuous run it is the one right after the last boundary
        // handled, so a boundary is never played twice or lost when Logic's positions are a hair off; after a jump or a start it
        // is the first boundary at or after the block start.
        long long k = (long long) std::ceil ((ppq0 - eps) / stepLen);
        if (continuing)
            k = std::max (k, (long long) std::floor ((lastBoundaryPpq + eps) / stepLen) + 1);

        for (;;)
        {
            const double b = (double) k * stepLen;
            const bool haveBoundary = b < ppq1 - eps;
            const bool haveGateEnd = held.load() && gateOffPpq >= 0.0 && gateOffPpq < ppq1 - eps;
            if (! haveBoundary && ! haveGateEnd)
                break;

            if (haveGateEnd && (! haveBoundary || gateOffPpq < b))
            {
                emit (toOffset (gateOffPpq), false, heldNote, 0);
                held.store (false);
                gateOffPpq = -1.0;
                continue;
            }

            // A step starts here. Whatever was sounding ends exactly now (a gate of 100 % is legato up to this point).
            const int offset = toOffset (b);
            release (offset);

            const int index = (int) floorMod (k, length);
            const Step& step = data.steps[(std::size_t) index];
            shownStep.store (index);
            stepsStarted.fetch_add (1);
            lastBoundaryPpq = b;

            // A note-on is only written when there is also room left for its note-off.
            if (step.on && n + 2 <= maxOut)
            {
                const int note = std::clamp ((int) step.note, 0, 127);
                emit (offset, true, note, kPlayerVelocity);
                held.store (true);
                heldNote = note;
                notesStarted.fetch_add (1);
                const double gate = std::max (kMinGateFraction, (double) step.gate / (double) kGateMax);
                gateOffPpq = gate >= 0.999 ? -1.0 : b + gate * stepLen;
            }
            ++k;
        }

        expectedPpq = ppq1;
        wasRunning = true;
        return n;
    }
}
