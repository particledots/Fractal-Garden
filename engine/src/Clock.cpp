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

        constexpr double kNever = 1e300;
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
        haveResetCount = false;
        anchorK = 0;
        expectedPpq = 0.0;
        lastBoundaryPpq = 0.0;
        gateOffPpq = -1.0;
        heldNote = 0;
        stepActive = false;
        lastCc1 = lastCc84 = -1;
        sampleClock = 0;
        nextTickAbs = 0;
        ccRingPos = ccRingCount = 0;
        branchBuilt = false;
        rngSeeded = false;
        mutLogCount = 0;
        held.store (false);
        isRunning.store (false);
        shownStep.store (-1);
        shownSection.store (-1);
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
        anchorK = 0;
        stepActive = false;
        lastCc1 = lastCc84 = -1;
        isRunning.store (false);
        shownStep.store (-1);
        shownSection.store (-1);
        return n;
    }

    int TrunkPlayer::takeMutations (MutationEvent* out, int maxOut)
    {
        const int n = std::min (mutLogCount, maxOut);
        for (int i = 0; i < n; ++i)
            out[i] = mutLog[i];
        mutLogCount = 0;
        return n;
    }

    int TrunkPlayer::process (double sampleRate, int numSamples, const HostBlock& host, const PlayerSettings& settings,
                              const StepData& data, NoteEvent* out, int maxOut)
    {
        StepData copy = data;                         // nothing here may change it: no branches, no mutation (the settings are forced off)
        PlayerSettings plain = settings;
        plain.branches = 0;
        plain.mutate = plain.microRatchet = plain.microSlew = plain.microMod = 0.0;
        return process (sampleRate, numSamples, host, plain, copy, data, out, maxOut);
    }

    // Whether the Mod value changes during the current step (so it has to be sampled every 10 ms).
    bool TrunkPlayer::modMoves (const PlayerSettings& settings) const
    {
        switch (settings.modMode)
        {
            case 1:  return false;                                       // Velocity: one fixed level
            case 2:  return curStep.on;                                  // Envelope
            case 3:  return curPrevLevel != stepLevel (curStep);         // Smooth: gliding from the last level to this one
            default: return shapesModMoves (curStep);                    // Shapes
        }
    }

    // The Mod value (0 to 1) t of the way through the current step.
    double TrunkPlayer::modLevel (const PlayerSettings& settings, double t) const
    {
        switch (settings.modMode)
        {
            case 1:  return stepLevel (curStep);
            case 2:  return curStep.on ? envelopeModValue (curStep.mod, t) : 0.0;
            case 3:  return smoothModValue (curPrevLevel, stepLevel (curStep), t);
            default: return shapesModValue (curStep, t);
        }
    }

    int TrunkPlayer::process (double sampleRate, int numSamples, const HostBlock& host, const PlayerSettings& settings,
                              StepData& data, const StepData& branchSource, NoteEvent* out, int maxOut)
    {
        int n = 0;
        const int lastSample = numSamples > 0 ? numSamples - 1 : 0;
        const long long blockStartAbs = sampleClock;
        if (numSamples > 0)
            sampleClock += numSamples;

        auto emit = [&] (int offset, bool on, int note, int velocity)
        {
            if (n < maxOut)
                out[n++] = NoteEvent { std::clamp (offset, 0, lastSample), on, note, velocity };
        };
        auto emitCc = [&] (int offset, int cc, int value)
        {
            if (n + 4 <= maxOut)          // always leave room for note-offs
            {
                NoteEvent e;
                e.offset = std::clamp (offset, 0, lastSample);
                e.cc = cc;
                e.value = value;
                out[n++] = e;
            }
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
        // The cap: no more than kMaxCcPerSecond CC messages in any one second (counted in samples).
        auto allowCc = [&] (long long absSample)
        {
            const long long window = (long long) std::max (1.0, sampleRate);
            if (ccRingCount >= kMaxCcPerSecond && absSample - ccRing[ccRingPos] < window)
                return false;
            ccRing[ccRingPos] = absSample;
            ccRingPos = (ccRingPos + 1) % kMaxCcPerSecond;
            if (ccRingCount < kMaxCcPerSecond)
                ++ccRingCount;
            return true;
        };

        const bool running = host.playing && host.havePpq && sampleRate > 0.0 && numSamples > 0;
        isRunning.store (running);
        if (! haveResetCount)
        {
            seenResetCount = settings.resetCount;       // a RESET pressed before this point is not acted on later
            haveResetCount = true;
        }

        if (! running)
        {
            // Logic is stopped (or gave no position): the sounding note ends now and the next start begins afresh, locked to the bars.
            release (0);
            wasRunning = false;
            anchorK = 0;
            stepActive = false;
            lastCc1 = lastCc84 = -1;
            ccRingPos = ccRingCount = 0;
            seenResetCount = settings.resetCount;       // RESET while stopped has nothing to restart
            shownStep.store (-1);
            shownSection.store (-1);
            return n;
        }

        const int length = std::clamp (settings.length, 1, kNumSteps);
        const int branchCount = std::clamp (settings.branches, 0, kMaxBranches);
        if (branchCount == 0)
            branchBuilt = false;
        else
        {
            const int path = std::clamp (settings.path, 0, pathCountFor (branchCount) - 1);
            const int scale = std::clamp (settings.scale, 0, kNumScales - 1);
            if (! branchBuilt || builtRevision != settings.branchRevision || builtSeed != settings.branchSeed || builtBranches != branchCount
                || builtPath != path || builtLength != length || builtScale != scale)
            {
                buildBranches (branchSet, branchSource, length, scale, branchCount, path, settings.branchSeed);
                branchBuilt = true;
                builtRevision = settings.branchRevision; builtSeed = settings.branchSeed; builtBranches = branchCount;
                builtPath = path; builtLength = length; builtScale = scale;
            }
        }
        const bool mutating = settings.mutate > 0.0 || settings.microRatchet > 0.0 || settings.microSlew > 0.0 || settings.microMod > 0.0;

        // Where step counter value kk plays: which section (0 = trunk, 1.. = branch) and which step of it.
        auto locate = [&] (long long kk, int& section, int& index)
        {
            const long long local = kk - anchorK;
            index = orderStepIndex (local, length, settings.order, settings.pendRepeat, settings.seed, settings.chance);
            if (branchCount == 0)
                section = 0;
            else
            {
                const long long passes = (long long) std::floor ((double) local / (double) length);
                section = (int) (((passes % (branchCount + 1)) + (branchCount + 1)) % (branchCount + 1));
            }
        };
        auto stepRef = [&] (int section, int index) -> Step&
        {
            return section == 0 ? data.steps[(std::size_t) index] : branchSet.steps[(std::size_t) (section - 1)][(std::size_t) index];
        };
        const double bpm = host.bpm > 0.0 ? host.bpm : 120.0;
        const double pps = bpm / 60.0 / sampleRate;                 // quarter notes per sample
        const double ppq0 = host.ppq;
        const double ppq1 = ppq0 + (double) numSamples * pps;
        const double stepLen = stepLengthQuarters (settings.rate, settings.ratio, host.sigNum, host.sigDen);
        const double minNote = kMinNoteSeconds * bpm / 60.0;        // the shortest note, in quarter notes
        const long long tickSamples = std::max<long long> (1, std::llround (kModTickSeconds * sampleRate));
        const double eps = 1e-7;
        auto toOffset = [&] (double p) { return (int) std::lround ((p - ppq0) / pps); };

        // A jump (relocate, cycle, or the song position changing by more than a block's worth): end the note, count again from here.
        const bool relocated = wasRunning && std::fabs (ppq0 - expectedPpq) > kRelocateTolerance;
        if (relocated)
            release (0);
        const bool continuing = wasRunning && ! relocated;
        if (! continuing)
        {
            anchorK = 0;                                // a start, a jump or a cycle: back to Logic's bars
            stepActive = false;
            lastCc1 = lastCc84 = -1;                    // the next values are sent whatever they are
            if (! wasRunning)
                ccRingPos = ccRingCount = 0;
        }

        // Where the position is now, for the window (the steps that start inside this block update it below).
        if (! continuing)
        {
            int section0, index0;
            locate ((long long) std::floor ((ppq0 + eps) / stepLen), section0, index0);
            shownStep.store (index0);
            shownSection.store (section0);
            // "the last boundary handled" is the one just before the first boundary this run will handle
            lastBoundaryPpq = (double) ((long long) std::ceil ((ppq0 - eps) / stepLen) - 1) * stepLen;
        }

        // The first step boundary this block has to deal with. In a continuous run it is the one right after the last boundary
        // handled, so a boundary is never played twice or lost when Logic's positions are a hair off; after a jump or a start it
        // is the first boundary at or after the block start.
        long long k = (long long) std::ceil ((ppq0 - eps) / stepLen);
        if (continuing)
            k = std::max (k, (long long) std::floor ((lastBoundaryPpq + eps) / stepLen) + 1);

        // RESET: the next step to play is step 1 of the order.
        if (settings.resetCount != seenResetCount)
        {
            seenResetCount = settings.resetCount;
            anchorK = k;
        }

        for (;;)
        {
            // What could happen next, and when (in quarter notes of the song). The earliest goes first; at the same moment a note-off
            // goes before a new step, a new step before a ratchet note, and a ratchet note before a Mod tick.
            const double b = (double) k * stepLen;
            const double tBoundary = b < ppq1 - eps ? b : kNever;
            const double tOff = (held.load() && gateOffPpq >= 0.0 && gateOffPpq < ppq1 - eps) ? gateOffPpq : kNever;

            double tPulse = kNever;
            if (stepActive && nextPulse < curPulses)
            {
                const double pt = curStart + (double) nextPulse * curSlot;
                if (pt < ppq1 - eps)
                    tPulse = pt;
            }

            double tTick = kNever;
            long long tickAbs = 0;
            if (settings.sendCc && stepActive && modMoves (settings))
            {
                // A tick is nudged a hair after anything else at the same moment, and a tick that would land on (or after) the next
                // step's start is left to that step, so the output never depends on the block size.
                tickAbs = std::max (nextTickAbs, blockStartAbs);
                const double tp = ppq0 + (double) (tickAbs - blockStartAbs) * pps + 1e-9;
                if (tickAbs < blockStartAbs + numSamples && tp < b - 1e-8)
                    tTick = tp;
            }

            const double tNext = std::min (std::min (tBoundary, tOff), std::min (tPulse, tTick));
            if (tNext >= kNever)
                break;

            if (tOff <= tNext)
            {
                emit (toOffset (gateOffPpq), false, heldNote, 0);
                held.store (false);
                gateOffPpq = -1.0;
                continue;
            }

            if (tBoundary <= tNext)
            {
                // A step starts here.
                const int offset = toOffset (b);
                const long long boundaryAbs = blockStartAbs + std::clamp (offset, 0, lastSample);
                int section, index;
                locate (k, section, index);
                Step& stored = stepRef (section, index);
                if (mutating)
                {
                    // Mutate and Micro Mutate change the step as it starts, before it plays (and for good).
                    if (! rngSeeded)
                    {
                        mutRng = Rng ((std::uint64_t) settings.seed ^ 0xa5a5a5a5ULL);
                        rngSeeded = true;
                    }
                    bool changed = defaultMutate (stored, settings.mutate, settings.scale, mutRng);
                    changed = microMutate (stored, settings.microRatchet, settings.microSlew, settings.microMod, settings.modMode, mutRng) || changed;
                    if (changed)
                    {
                        stored.clamp();
                        if (section == 0 && mutLogCount < kMaxMutationLog)
                            mutLog[mutLogCount++] = MutationEvent { settings.epoch, index, stored };
                    }
                }
                const Step step = stored;                   // a copy: what plays is what the step was as it started
                shownStep.store (index);
                shownSection.store (section);
                stepsStarted.fetch_add (1);
                lastBoundaryPpq = b;

                // Where a Smooth glide starts: the level the step before ended on.
                double prevLevel;
                if (stepActive)
                    prevLevel = stepLevel (curStep);
                else
                {
                    int prevSection, prevIndex;
                    locate (k - 1, prevSection, prevIndex);
                    prevLevel = stepLevel (stepRef (prevSection, prevIndex));
                }

                const int pulses = kRatchetAmounts[std::min (kRatchetMax, (int) step.ratchet)];
                // A tie: the note that is sounding was left open (gate 100 %, or the next step has slew) and this step plays. A tie needs
                // room for one more event (the old note's note-off), and a note-on is only written when its own note-off has room too.
                const bool wantTie = held.load() && gateOffPpq < 0.0;
                const bool plays = step.on && n + 2 <= maxOut && (! wantTie || n + 3 <= maxOut);
                const bool tie = plays && wantTie;
                if (! tie)
                    release (offset);          // whatever was sounding ends exactly now

                stepActive = true;
                curStep = step;
                curStart = b;
                curLen = stepLen;
                curPrevLevel = prevLevel;
                curPulses = pulses;
                nextPulse = plays ? 1 : pulses;
                curSlot = stepLen / (double) pulses;
                curPulseWidth = std::min (std::max (kRatchetPulseFraction * curSlot, minNote), 0.9 * curSlot);

                if (settings.sendCc)
                {
                    if (plays)
                    {
                        const int slewValue = std::clamp ((int) std::lround ((double) step.slew * 127.0 / (double) kSlewMax), 0, 127);
                        if (slewValue != lastCc84 && allowCc (boundaryAbs))
                        {
                            emitCc (offset, kSlewCc, slewValue);
                            lastCc84 = slewValue;
                        }
                    }
                    const int modValue = modToCc (modLevel (settings, 0.0));
                    if (modValue != lastCc1 && allowCc (boundaryAbs))
                    {
                        emitCc (offset, kModCc, modValue);
                        lastCc1 = modValue;
                    }
                    nextTickAbs = boundaryAbs + tickSamples;
                }

                if (plays)
                {
                    const int note = pitchFor (step.note, settings.scale, settings.key, settings.root);
                    int velocity = kPlayerVelocity;
                    if (settings.modMode == 1)
                        velocity = std::clamp ((int) std::lround ((double) step.mod * 127.0 / (double) kModMax), 1, 127);
                    curPulseNote = note;
                    curPulseVelocity = velocity;

                    if (! (tie && heldNote == note))              // the same pitch tied: no new note, the old one just carries on
                    {
                        emit (offset, true, note, velocity);
                        if (tie)
                            emit (offset, false, heldNote, 0);     // the old note ends right after the new one starts (legato)
                        held.store (true);
                        heldNote = note;
                        notesStarted.fetch_add (1);
                    }

                    if (pulses > 1)
                    {
                        gateOffPpq = b + curPulseWidth;
                    }
                    else
                    {
                        const double fraction = std::max (kMinGateFraction, (double) step.gate / (double) kGateMax);
                        bool open = fraction >= 0.999;                       // 100 %: tied to the next step
                        if (! open)
                        {
                            // The next step has slew: hold this note up to it so a mono synth can glide.
                            int nextSection, nextIndex;
                            locate (k + 1, nextSection, nextIndex);
                            const Step& next = stepRef (nextSection, nextIndex);
                            open = next.on && next.slew > 0;
                        }
                        gateOffPpq = open ? -1.0 : b + std::min (std::max (fraction * stepLen, minNote), stepLen);
                    }
                }
                ++k;
                continue;
            }

            if (tPulse <= tNext)
            {
                // The next ratchet note of the current step.
                const double pt = curStart + (double) nextPulse * curSlot;
                const int offset = toOffset (pt);
                ++nextPulse;
                if (n + 2 <= maxOut)
                {
                    release (offset);
                    emit (offset, true, curPulseNote, curPulseVelocity);
                    held.store (true);
                    heldNote = curPulseNote;
                    notesStarted.fetch_add (1);
                    gateOffPpq = pt + curPulseWidth;
                }
                continue;
            }

            // A Mod tick: sample the moving Mod value.
            {
                const int offset = (int) (tickAbs - blockStartAbs);
                const double t = std::clamp ((tTick - curStart) / curLen, 0.0, 1.0);
                const int modValue = modToCc (modLevel (settings, t));
                if (modValue != lastCc1 && allowCc (tickAbs))
                {
                    emitCc (offset, kModCc, modValue);
                    lastCc1 = modValue;
                }
                nextTickAbs = tickAbs + tickSamples;
            }
        }

        expectedPpq = ppq1;
        wasRunning = true;
        return n;
    }
}
