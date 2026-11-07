// Clock self-test (milestone 3): runs the trunk player the way Logic would, block by block, and checks when every note starts and stops.
// Needs no plugin and no Logic. The simulated host gives a song position for each block; the checks cover timing to the sample, every
// RATE and RATIO, block sizes from 1 to 8192, start / stop / relocate / cycle, tempo and rate changes, two instances staying in step,
// and that no note is ever left sounding. Milestones 6 and 7 add: the trunk-then-branches loop, branches made from the snapshot, Mutate and
// Micro Mutate while playing (same result at every block size), and that with no branches and the knobs at 0 nothing changed from 0.5.0.
// Prints "clock tests: N checks passed" and exits 0, or lists every failure and exits 1.

#include <fg/Clock.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

static int checks = 0, failures = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

namespace
{
    constexpr int kChromatic = fg::kNumScales - 1;
    constexpr double kSr = 48000.0;
    constexpr double kBpm = 120.0;
    constexpr double kPps = kBpm / 60.0 / kSr;       // quarter notes per sample at 120 bpm, 48 kHz (1 quarter = 24000 samples)

    struct Ev
    {
        long long t = 0;          // absolute sample
        bool on = false;
        int note = 0;
        int velocity = 0;
        int cc = -1;              // >= 0: a CC event (controller number)
        int value = 0;            // CC value
        double ppq = 0.0;         // song position of the event
        int length = 8;           // LENGTH in effect for the block
        double stepLen = 0.25;    // step length in quarters in effect for the block
    };

    struct Run
    {
        double sr = kSr;
        int block = 512;
        long long total = 0;
        int maxOut = 4096;
        std::function<fg::HostBlock (long long)> host;
        std::function<fg::PlayerSettings (long long)> settings;
        fg::StepData data;
        std::vector<int>* shownAtBlockEnd = nullptr;     // currentStep() after each block
        std::vector<bool>* gateAtBlockEnd = nullptr;     // gateOpen() after each block
    };

    fg::StepData makeData (int gatePercent = 50)
    {
        fg::StepData d;
        for (int i = 0; i < fg::kNumSteps; ++i)
        {
            d.steps[(std::size_t) i].note = (std::uint8_t) std::min (i, fg::kStoredNoteMax);     // every step has its own note (Chromatic, key C: step i plays 48 + i), so a wrong step is seen
            d.steps[(std::size_t) i].gate = (std::uint8_t) gatePercent;
            d.steps[(std::size_t) i].on = true;
        }
        return d;
    }

    std::function<fg::HostBlock (long long)> linearHost (double startPpq, double bpm = kBpm, double sr = kSr, int num = 4, int den = 4)
    {
        return [=] (long long t)
        {
            fg::HostBlock h;
            h.playing = true; h.havePpq = true; h.bpm = bpm; h.sigNum = num; h.sigDen = den;
            h.ppq = startPpq + (double) t * (bpm / 60.0 / sr);
            return h;
        };
    }

    // The test steps hold "place i" in each step; in the Chromatic scale with key C and Root 0, place i plays MIDI note 48 + i.
    fg::PlayerSettings plainSettings()
    {
        fg::PlayerSettings s;
        s.scale = kChromatic;
        return s;
    }

    std::function<fg::PlayerSettings (long long)> fixedSettings (int rate = fg::kDefaultRate, int ratio = fg::kDefaultRatio, int length = 8)
    {
        return [=] (long long) { fg::PlayerSettings s = plainSettings(); s.rate = rate; s.ratio = ratio; s.length = length; return s; };
    }

    Run makeRun (long long total)
    {
        Run r;
        r.total = total;
        r.data = makeData();
        r.host = linearHost (0.0);
        r.settings = fixedSettings();
        return r;
    }

    std::vector<Ev> simulate (fg::TrunkPlayer& player, const Run& r)
    {
        std::vector<Ev> events;
        std::vector<fg::NoteEvent> buf ((std::size_t) r.maxOut);
        for (long long t = 0; t < r.total; t += r.block)
        {
            const int n = (int) std::min<long long> (r.block, r.total - t);
            const fg::HostBlock h = r.host (t);
            const fg::PlayerSettings s = r.settings (t);
            const double pps = (h.bpm > 0.0 ? h.bpm : 120.0) / 60.0 / r.sr;
            const double stepLen = fg::stepLengthQuarters (s.rate, s.ratio, h.sigNum, h.sigDen);
            const int count = player.process (r.sr, n, h, s, r.data, buf.data(), r.maxOut);
            CHECK (count >= 0 && count <= r.maxOut);
            int lastOffset = 0;
            for (int i = 0; i < count; ++i)
            {
                CHECK (buf[(std::size_t) i].offset >= 0 && buf[(std::size_t) i].offset < n);
                CHECK (buf[(std::size_t) i].offset >= lastOffset);                   // in time order inside a block
                lastOffset = buf[(std::size_t) i].offset;
                Ev e;
                e.t = t + buf[(std::size_t) i].offset;
                e.on = buf[(std::size_t) i].on;
                e.note = buf[(std::size_t) i].note;
                e.velocity = buf[(std::size_t) i].velocity;
                e.cc = buf[(std::size_t) i].cc;
                e.value = buf[(std::size_t) i].value;
                e.ppq = h.ppq + (double) buf[(std::size_t) i].offset * pps;
                e.length = s.length;
                e.stepLen = stepLen;
                events.push_back (e);
            }
            if (r.shownAtBlockEnd) r.shownAtBlockEnd->push_back (player.currentStep());
            if (r.gateAtBlockEnd) r.gateAtBlockEnd->push_back (player.gateOpen());
        }
        return events;
    }

    struct Analysis { bool ok = true; int sounding = 0; int ons = 0, offs = 0; int maxSounding = 0; };

    // One note at a time, every note-off matches the note that is sounding, and (sounding at the end) tells whether anything is left on.
    // allowTies: a note-on while another note sounds is fine when the old note's note-off follows at the same sample (a tie, legato).
    Analysis analyse (const std::vector<Ev>& events, bool allowTies = false)
    {
        Analysis a;
        std::vector<int> sounding;
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            const auto& e = events[i];
            if (e.cc >= 0)
                continue;                                              // CC events are not notes
            if (e.on)
            {
                ++a.ons;
                if (! sounding.empty())
                {
                    bool tied = false;
                    for (std::size_t j = i + 1; allowTies && j < events.size() && events[j].t == e.t; ++j)
                        if (events[j].cc < 0 && ! events[j].on && events[j].note == sounding.front()) tied = true;
                    if (! tied) a.ok = false;                          // overlapping notes: the player is monophonic
                }
                sounding.push_back (e.note);
            }
            else
            {
                ++a.offs;
                auto it = std::find (sounding.begin(), sounding.end(), e.note);
                if (it == sounding.end()) a.ok = false;                // a note-off for something that is not sounding
                else sounding.erase (it);
            }
            a.maxSounding = std::max (a.maxSounding, (int) sounding.size());
        }
        a.sounding = (int) sounding.size();
        return a;
    }

    std::vector<Ev> onsOf (const std::vector<Ev>& events)
    {
        std::vector<Ev> r;
        for (const auto& e : events) if (e.on) r.push_back (e);
        return r;
    }

    long long floorMod (long long k, int n) { const long long m = k % n; return m < 0 ? m + n : m; }

    // Every note-on sits on a step boundary and plays the step that belongs to that boundary.
    bool onsOnBoundaries (const std::vector<Ev>& events, double tolerance = 1e-4)
    {
        for (const auto& e : events)
        {
            if (! e.on) continue;
            const double k = std::round (e.ppq / e.stepLen);
            if (std::fabs (e.ppq - k * e.stepLen) > tolerance) return false;
            if (e.note != 48 + (int) floorMod ((long long) k, e.length)) return false;
        }
        return true;
    }

    bool sameEvents (const std::vector<Ev>& a, const std::vector<Ev>& b, long long tolerance = 1)
    {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].on != b[i].on || a[i].note != b[i].note || a[i].cc != b[i].cc || a[i].value != b[i].value || std::llabs (a[i].t - b[i].t) > tolerance) return false;
        return true;
    }
}

int main()
{
    // ---- the step length table ----
    CHECK (fg::stepLengthQuarters (6, 4, 4, 4) == 0.25);                                                  // 1/16 x1
    CHECK (fg::stepLengthQuarters (2, 4, 4, 4) == 1.0);                                                   // 1/4
    CHECK (fg::stepLengthQuarters (0, 4, 4, 4) == 4.0 && fg::stepLengthQuarters (0, 4, 3, 4) == 3.0);     // 1 bar in 4/4 and 3/4
    CHECK (fg::stepLengthQuarters (0, 4, 6, 8) == 3.0 && fg::stepLengthQuarters (0, 4, 7, 8) == 3.5 && fg::stepLengthQuarters (0, 4, 12, 8) == 6.0);
    CHECK (fg::stepLengthQuarters (0, 4, 0, 0) == 4.0);                                                   // unknown signature: 4/4
    CHECK (std::fabs (fg::stepLengthQuarters (3, 4, 4, 4) - 2.0 / 3.0) < 1e-12);                          // 1/4T
    CHECK (std::fabs (fg::stepLengthQuarters (5, 4, 4, 4) - 1.0 / 3.0) < 1e-12);                          // 1/8T
    CHECK (std::fabs (fg::stepLengthQuarters (7, 4, 4, 4) - 1.0 / 6.0) < 1e-12);                          // 1/16T
    CHECK (fg::stepLengthQuarters (8, 4, 4, 4) == 0.125);                                                 // 1/32
    CHECK (fg::stepLengthQuarters (6, 5, 4, 4) == 0.125 && fg::stepLengthQuarters (6, 3, 4, 4) == 0.5);   // x2 halves the step, /2 doubles it
    CHECK (fg::stepLengthQuarters (6, 8, 4, 4) == 0.03125 && fg::stepLengthQuarters (6, 0, 4, 4) == 2.0); // x8 and /8
    CHECK (std::fabs (fg::stepLengthQuarters (6, 6, 4, 4) - 0.25 / 3.0) < 1e-12);                         // x3
    CHECK (std::fabs (fg::stepLengthQuarters (6, 2, 4, 4) - 0.75) < 1e-12);                               // /3
    CHECK (fg::stepLengthQuarters (-5, 99, 4, 4) == fg::stepLengthQuarters (0, 8, 4, 4));                 // out of range is pulled in

    // ---- the basic run: 120 bpm, 1/16, 2 bars, 32 steps, length 8, gate 50 % ----
    std::vector<Ev> reference;
    {
        fg::TrunkPlayer p;
        Run r = makeRun (192000);
        std::vector<int> shown; std::vector<bool> gate;
        r.shownAtBlockEnd = &shown; r.gateAtBlockEnd = &gate;
        const auto ev = simulate (p, r);
        reference = ev;
        const auto a = analyse (ev);
        CHECK (a.ok && a.ons == 32 && a.offs == 32 && a.sounding == 0 && a.maxSounding == 1);
        bool exact = ev.size() == 64;
        for (int k = 0; k < 32 && exact; ++k)
        {
            const auto& on = ev[(std::size_t) (2 * k)]; const auto& off = ev[(std::size_t) (2 * k + 1)];
            exact = on.on && ! off.on && std::llabs (on.t - 6000LL * k) <= 1 && std::llabs (off.t - (6000LL * k + 3000)) <= 1
                    && on.note == 48 + k % 8 && off.note == on.note && on.velocity == fg::kPlayerVelocity;
        }
        CHECK (exact);
        CHECK (onsOnBoundaries (ev));
        // what the window is shown after each block: the step that started last, and whether a note sounds
        bool shownOk = shown.size() == gate.size() && ! shown.empty();
        std::vector<Ev> sofar;
        for (std::size_t b = 0; b < shown.size() && shownOk; ++b)
        {
            const long long blockEnd = (long long) (b + 1) * 512;
            const double ppqEnd = (double) std::min<long long> (blockEnd, 192000) * kPps;
            const long long lastStart = (long long) std::ceil ((ppqEnd - 1e-7) / 0.25) - 1;
            if (shown[b] != (int) floorMod (lastStart, 8)) shownOk = false;
            bool sounding = false;
            for (const auto& e : ev) if (e.t < blockEnd) sounding = e.on;        // the last event before the end of the block
            if (gate[b] != sounding) shownOk = false;
        }
        CHECK (shownOk);
        CHECK (p.stepCount() == 32 && p.noteCount() == 32 && p.running());
    }

    // ---- the same notes at the same samples whatever the block size ----
    for (int block : { 1, 7, 64, 100, 480, 1000, 1024, 3000, 4096, 8192 })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (192000);
        r.block = block;
        r.maxOut = 20000;
        const auto ev = simulate (p, r);
        CHECK (sameEvents (ev, reference));
        const auto a = analyse (ev);
        CHECK (a.ok && a.sounding == 0);
    }

    // ---- other sample rates and tempos: note-ons sit on the step boundaries, to the sample ----
    for (double sr : { 44100.0, 88200.0, 96000.0 })
        for (double bpm : { 60.0, 93.7, 120.0, 187.0, 300.0 })
        {
            fg::TrunkPlayer p;
            Run r = makeRun ((long long) (sr * 10.0));
            r.sr = sr; r.block = 441;
            r.host = linearHost (0.0, bpm, sr);
            const auto ev = simulate (p, r);
            const double pps = bpm / 60.0 / sr;
            bool exact = analyse (ev).ok && analyse (ev).sounding <= 1;
            int k = 0;
            for (const auto& e : ev)
                if (e.on)
                {
                    exact = exact && std::llabs (e.t - (long long) std::llround (k * 0.25 / pps)) <= 1 && e.note == 48 + k % 8;
                    ++k;
                }
            CHECK (exact && k > 10);
        }

    // ---- Logic's positions are a hair off from block to block: no step is played twice or lost ----
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<double> jitter (-5e-7, 5e-7);
        fg::TrunkPlayer p;
        Run r = makeRun (192000);
        const auto clean = linearHost (0.0);
        r.host = [&] (long long t) { auto h = clean (t); if (t > 0) h.ppq += jitter (rng); return h; };
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        CHECK (a.ok && a.ons == 32 && a.offs == 32 && a.sounding == 0);
        CHECK (sameEvents (ev, reference));
    }

    // ---- unknown tempo counts as 120 ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (192000);
        r.host = [] (long long t) { fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = 0.0; h.ppq = (double) t * kPps; return h; };
        CHECK (sameEvents (simulate (p, r), reference));
    }

    // ---- every RATE with every RATIO: onsets on the step boundaries, nothing left sounding ----
    for (int rate = 0; rate < fg::kNumRates; ++rate)
        for (int ratio = 0; ratio < fg::kNumRatios; ++ratio)
        {
            fg::TrunkPlayer p;
            Run r = makeRun (192000);                       // 8 quarters
            r.settings = fixedSettings (rate, ratio, 8);
            const auto ev = simulate (p, r);
            const double stepLen = fg::stepLengthQuarters (rate, ratio, 4, 4);
            const int expectedOns = (int) std::ceil (8.0 / stepLen - 1e-6);
            const auto a = analyse (ev);
            bool exact = a.ok && a.ons == expectedOns && a.sounding <= 1 && onsOnBoundaries (ev);
            int k = 0;
            for (const auto& e : ev)
                if (e.on) { exact = exact && std::llabs (e.t - (long long) std::llround ((double) k * stepLen / kPps)) <= 1; ++k; }
            CHECK (exact);
        }

    // ---- rests, ties (gate 100 %) and gate lengths ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (48000);
        r.data = makeData (100);
        r.data.steps[2].on = false;
        const auto ev = simulate (p, r);                    // 8 steps
        const auto a = analyse (ev, true);
        CHECK (a.ok && a.ons == 7 && a.offs == 6 && a.sounding == 1);                 // the last note is still on: its step has not ended when the run does
        // a tie into an OFF step ends at the start of that step: step 2's note (49) ends exactly where step 3 would start
        bool endsAtRest = false, restHeard = false;
        for (const auto& e : ev)
        {
            if (e.t == 12000 && ! e.on && e.note == 49) endsAtRest = true;
            if (e.note == 50) restHeard = true;
        }
        CHECK (endsAtRest && ! restHeard);
        // a tie: the new note-on first, then the old note's note-off at the same sample (legato); five ties in this run
        int tiePairs = 0;
        bool pairsRight = true;
        for (std::size_t i = 0; i + 1 < ev.size(); ++i)
            if (ev[i].t == ev[i + 1].t)
            {
                ++tiePairs;
                pairsRight = pairsRight && ev[i].on && ! ev[i + 1].on && ev[i + 1].note == ev[i].note - 1;
            }
        CHECK (pairsRight && tiePairs == 5);
    }
    for (int gate : { 0, 1, 5, 25, 50, 75, 99 })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (12000);
        r.data = makeData (gate);
        const auto ev = simulate (p, r);
        const long long expectOff = (long long) std::llround (std::max (0.05, gate / 100.0) * 6000.0);
        CHECK (ev.size() >= 2 && ev[0].on && ! ev[1].on && std::llabs (ev[1].t - expectOff) <= 1);
    }

    // ---- start in the middle of a step: the first note is the next boundary; exactly on a boundary: at once ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (24000);
        r.host = linearHost (0.1);
        const auto ev = simulate (p, r);
        CHECK (! ev.empty() && ev[0].on && std::llabs (ev[0].t - 3600) <= 1 && ev[0].note == 48 + 1);      // boundary 0.25 = step index 1
    }
    for (double start : { 8.0, 7.999999999, 8.0000001 })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (12000);
        r.host = linearHost (start);
        const auto ev = simulate (p, r);
        CHECK (! ev.empty() && ev[0].on && ev[0].t <= 1 && ev[0].note == 48 + 32 % 8);
    }
    {   // before bar 1 (a count-in): the step index still counts correctly
        fg::TrunkPlayer p;
        Run r = makeRun (24000);
        r.host = linearHost (-1.0);
        const auto ev = simulate (p, r);
        CHECK (! ev.empty() && ev[0].on && ev[0].t <= 1 && ev[0].note == 48 + (int) floorMod (-4, 8));
        CHECK (onsOnBoundaries (ev));
    }

    // ---- stop in the middle of a note, then start again ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (60000);
        const auto lin = linearHost (0.0);
        r.host = [&] (long long t) { auto h = lin (t); if (t >= 7168 && t < 30208) h.playing = false; return h; };
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        CHECK (a.ok && a.sounding == 0);
        // note 1 starts at 6000, would end at 9000; Logic stops in block 7168: the note-off is exactly there
        bool offAtStop = false, quiet = true;
        long long firstAfter = -1;
        for (const auto& e : ev)
        {
            if (! e.on && e.note == 49 && e.t == 7168) offAtStop = true;
            if (e.t > 7168 && e.t < 30208) quiet = false;
            if (e.t >= 30208 && firstAfter < 0 && e.on) firstAfter = e.t;
        }
        CHECK (offAtStop && quiet);
        CHECK (firstAfter >= 0 && std::llabs (firstAfter - 36000) <= 1);              // next boundary after the restart
    }
    {   // stop right as a note starts, and stop with nothing sounding: nothing odd is sent
        fg::TrunkPlayer p;
        Run r = makeRun (24000);
        const auto lin = linearHost (0.0);
        r.host = [&] (long long t) { auto h = lin (t); if (t >= 9216) h.playing = false; return h; };   // note 1 ended at 9000
        const auto ev = simulate (p, r);
        CHECK (analyse (ev).ok && analyse (ev).sounding == 0 && ev.size() == 4);                          // on, off, on, off (steps 0 and 1)
    }
    {   // Logic stops giving a song position while playing: treated like a stop
        fg::TrunkPlayer p;
        Run r = makeRun (30000);
        const auto lin = linearHost (0.0);
        r.host = [&] (long long t) { auto h = lin (t); if (t >= 7168) h.havePpq = false; return h; };
        const auto ev = simulate (p, r);
        bool offAtLoss = false;
        for (const auto& e : ev) if (! e.on && e.t == 7168) offAtLoss = true;
        CHECK (analyse (ev).ok && analyse (ev).sounding == 0 && offAtLoss && ev.back().t <= 9000);
        CHECK (! p.running() && p.currentStep() == -1);
    }

    // ---- relocate in the middle of a note ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (48000);
        r.host = [] (long long t) { fg::HostBlock h; h.playing = true; h.havePpq = true; h.ppq = (double) t * kPps + (t >= 7168 ? 10.3 - 7168 * kPps : 0.0); return h; };
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        bool offAtJump = false;
        for (const auto& e : ev) if (! e.on && e.note == 49 && e.t == 7168) offAtJump = true;
        CHECK (a.ok && a.sounding <= 1 && offAtJump);
        long long firstOn = -1; int firstNote = 0;
        for (const auto& e : ev) if (e.on && e.t >= 7168) { firstOn = e.t; firstNote = e.note; break; }
        CHECK (std::llabs (firstOn - 11968) <= 1 && firstNote == 48 + 42 % 8);
        CHECK (onsOnBoundaries (ev));
    }
    {   // a small difference (0.01 quarter) is not a jump: the note goes on; 0.05 is a jump
        for (double jump : { 0.01, 0.05 })
        {
            fg::TrunkPlayer p;
            Run r = makeRun (24000);
            r.host = [jump] (long long t) { fg::HostBlock h; h.playing = true; h.havePpq = true; h.ppq = (double) t * kPps + (t >= 7168 ? jump : 0.0); return h; };
            const auto ev = simulate (p, r);
            long long offTime = -1;
            for (const auto& e : ev) if (! e.on && e.note == 49) { offTime = e.t; break; }
            CHECK (analyse (ev).ok);
            if (jump < 0.02) CHECK (std::llabs (offTime - (9000 - (long long) std::llround (jump / kPps))) <= 1);
            else CHECK (offTime == 7168);
        }
    }

    // ---- Logic's cycle: the position jumps back every 4 quarters; with a gate of 50 % and of 100 % ----
    for (int gate : { 50, 100 })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (480000);                           // 5 passes of 96000 samples
        r.block = 500;
        r.data = makeData (gate);
        r.host = [] (long long t) { fg::HostBlock h; h.playing = true; h.havePpq = true; h.ppq = (double) (t % 96000) * kPps; return h; };
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        const auto aTie = analyse (ev, gate == 100);
        CHECK (aTie.ok && (aTie.sounding == 0 || (gate == 100 && aTie.sounding <= 1)));
        CHECK (a.ons == 5 * 16);
        bool samePasses = true;
        for (int pass = 1; pass < 5; ++pass)
            for (int i = 0; i < 16; ++i)
            {
                const auto ons = onsOf (ev);
                if (ons[(std::size_t) (pass * 16 + i)].note != ons[(std::size_t) i].note) samePasses = false;
                if (std::llabs (ons[(std::size_t) (pass * 16 + i)].t - ons[(std::size_t) i].t - 96000LL * pass) > 1) samePasses = false;
            }
        CHECK (samePasses);
        // with legato the last note of a pass is cut at the jump, with the note-off ahead of the next pass's first note-on
        if (gate == 100)
        {
            bool cutAtJump = false;
            for (std::size_t i = 0; i + 1 < ev.size(); ++i)
                if (ev[i].t == 96000 && ! ev[i].on && ev[i + 1].on && ev[i + 1].t == 96000) cutAtJump = true;
            CHECK (cutAtJump);
        }
    }

    // ---- tempo changes while playing: the position stays continuous, steps stay on the beat ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (240000);
        std::vector<double> ppqAt; std::vector<double> bpmAt;
        double ppq = 0.0;
        for (long long t = 0; t < 240000; t += 512)
        {
            const double bpm = t < 48000 ? 120.0 : (t < 120000 ? 90.0 : 150.0 + (double) ((t / 512) % 7));       // a jump and a wobble
            ppqAt.push_back (ppq); bpmAt.push_back (bpm);
            ppq += 512 * bpm / 60.0 / kSr;
        }
        r.host = [&] (long long t) { fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = bpmAt[(std::size_t) (t / 512)]; h.ppq = ppqAt[(std::size_t) (t / 512)]; return h; };
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        CHECK (a.ok && a.sounding <= 1);
        CHECK (onsOnBoundaries (ev, 6e-5));
        const int expected = (int) std::ceil (ppq / 0.25 - 1e-6);
        CHECK (std::abs (a.ons - expected) <= 1);
    }

    // ---- RATE changed in the middle of a note, and from a slow rate to a fast one ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (60000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.rate = t >= 7168 ? 4 : 6; return s; };    // 1/16 -> 1/8 inside note 1 (6000 to 9000)
        const auto ev = simulate (p, r);
        CHECK (analyse (ev).ok && analyse (ev).sounding == 0);
        long long afterFirst = -1;
        for (const auto& e : ev) if (e.on && e.t > 6000) { afterFirst = e.t; break; }
        CHECK (std::llabs (afterFirst - 12000) <= 1);                                         // next 1/8 boundary is 0.5 quarter
        CHECK (onsOnBoundaries (ev));
    }
    {
        fg::TrunkPlayer p;
        Run r = makeRun (200000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.rate = t >= 100352 ? 6 : 0; return s; };  // 1 bar -> 1/16 at about 4.18 quarters
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        CHECK (a.ok && a.sounding <= 1);
        bool noBurst = true; long long previous = -1;
        for (const auto& e : ev)
            if (e.on && e.t >= 100352)
            {
                if (previous >= 0 && std::llabs (e.t - previous - 6000) > 1) noBurst = false;
                previous = e.t;
            }
        CHECK (noBurst && previous > 0);
        CHECK (onsOnBoundaries (ev));
    }

    // ---- RATIO changed while playing ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (96000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.ratio = t >= 48128 ? 5 : 4; return s; };   // x1 -> x2
        const auto ev = simulate (p, r);
        CHECK (analyse (ev).ok && analyse (ev).sounding <= 1);
        CHECK (onsOnBoundaries (ev));
    }

    // ---- LENGTH changed while playing ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (96000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.length = t >= 30208 ? 3 : 8; return s; };
        const auto ev = simulate (p, r);
        CHECK (analyse (ev).ok && analyse (ev).sounding <= 1);
        CHECK (onsOnBoundaries (ev));                                                          // each note is step (k mod LENGTH in effect)
    }
    for (int length : { 1, 2, 5, 16, 63, 64 })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (192000);
        r.settings = fixedSettings (fg::kDefaultRate, fg::kDefaultRatio, length);
        const auto ev = simulate (p, r);
        CHECK (analyse (ev).ok && analyse (ev).ons == 32 && onsOnBoundaries (ev));
    }
    {   // length 0 and 200 are pulled into 1 to 64
        fg::TrunkPlayer p;
        Run r = makeRun (24000);
        r.settings = fixedSettings (fg::kDefaultRate, fg::kDefaultRatio, 0);
        const auto ev = simulate (p, r);
        bool allFirst = ! ev.empty();
        for (const auto& e : ev) if (e.on && e.note != 48) allFirst = false;
        CHECK (allFirst);
    }

    // ---- time signature: "1 bar" follows it ----
    for (auto sig : { std::pair<int, int> { 3, 4 }, { 6, 8 }, { 7, 8 }, { 5, 4 } })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (480000);
        r.host = linearHost (0.0, kBpm, kSr, sig.first, sig.second);
        r.settings = fixedSettings (0, 4, 8);
        const auto ev = simulate (p, r);
        const double bar = 4.0 * sig.first / sig.second;
        const auto ons = onsOf (ev);
        bool spaced = ons.size() >= 3;
        for (std::size_t i = 0; i < ons.size(); ++i)
            spaced = spaced && std::llabs (ons[i].t - (long long) std::llround ((double) i * bar / kPps)) <= 1;
        CHECK (spaced && analyse (ev).ok);
    }

    // ---- two instances (same song, same settings) are in step, even when one starts later ----
    {
        fg::TrunkPlayer a, b;
        Run ra = makeRun (192000), rb = makeRun (192000);
        rb.host = [lin = linearHost (0.0)] (long long t) { auto h = lin (t); if (t < 37 * 512) h.playing = false; return h; };
        rb.block = 333;                                      // even with another block size
        const auto ea = onsOf (simulate (a, ra)), eb = onsOf (simulate (b, rb));
        std::vector<Ev> tail;
        for (const auto& e : ea) if (e.t >= eb.front().t) tail.push_back (e);
        bool locked = ! eb.empty() && tail.size() == eb.size();
        for (std::size_t i = 0; locked && i < eb.size(); ++i)
            locked = eb[i].note == tail[i].note && std::llabs (eb[i].t - tail[i].t) <= 1;
        CHECK (locked);
    }

    // ---- a full output list never loses a note-off: a note-on that has no room is skipped with it ----
    for (int room : { 2, 3, 4 })
    {
        fg::TrunkPlayer p;
        Run r = makeRun (96000);
        r.maxOut = room;
        r.block = 4096;
        r.settings = fixedSettings (8, 8, 8);                // 1/32 at x8: a step every 93.75 samples
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        CHECK (a.ok && a.sounding <= 1 && a.ons > 0);
    }

    // ---- bypass and panic: releaseHeld ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (7168);                              // stops inside note 1 (6000 to 9000)
        simulate (p, r);
        CHECK (p.gateOpen());
        fg::NoteEvent out[4];
        CHECK (p.releaseHeld (out, 4) == 1 && ! out[0].on && out[0].note == 49 && out[0].offset == 0);
        CHECK (! p.gateOpen() && p.currentStep() == -1 && ! p.running());
        CHECK (p.releaseHeld (out, 4) == 0);                 // nothing left to release
        // playing again from the middle of a step plays nothing until the next boundary
        fg::HostBlock h; h.playing = true; h.havePpq = true; h.ppq = 7168 * kPps;
        fg::PlayerSettings s = plainSettings(); s.length = 8;
        const auto data = makeData();
        fg::NoteEvent blk[8];
        const int n = p.process (kSr, 512, h, s, data, blk, 8);
        CHECK (n == 0 && p.currentStep() == 1);
    }

    // ---- the counters and the shown step when Logic is stopped ----
    {
        fg::TrunkPlayer p;
        CHECK (p.currentStep() == -1 && ! p.gateOpen() && ! p.running() && p.stepCount() == 0 && p.noteCount() == 0);
        fg::HostBlock h; h.playing = false; h.havePpq = true; h.ppq = 0.0;
        fg::PlayerSettings s = plainSettings();
        const auto data = makeData();
        fg::NoteEvent blk[8];
        CHECK (p.process (kSr, 512, h, s, data, blk, 8) == 0 && p.currentStep() == -1 && ! p.running());
        h.playing = true;
        p.process (kSr, 512, h, s, data, blk, 8);
        CHECK (p.running() && p.currentStep() == 0 && p.stepCount() == 1 && p.noteCount() == 1);
        // relocating into the middle of step 5 shows step 5 at once, before its boundary
        h.ppq = 5 * 0.25 + 0.1;
        p.process (kSr, 64, h, s, data, blk, 8);
        CHECK (p.currentStep() == 5);
    }

    // ---- a stored note outside the 5-octave range is pulled in to its top, and every note-on has the same velocity ----
    {
        fg::TrunkPlayer p;
        Run r = makeRun (6000);
        r.data.steps[0].note = 250;
        const auto ev = simulate (p, r);
        CHECK (! ev.empty() && ev[0].on && ev[0].note == 48 + fg::kStoredNoteMax && ev[0].velocity == fg::kPlayerVelocity);
    }

    // ---- an instance that is never started, or a run of zero-length blocks, sends nothing and does not crash ----
    {
        fg::TrunkPlayer p;
        fg::HostBlock h; h.playing = true; h.havePpq = true;
        fg::PlayerSettings s = plainSettings();
        const auto data = makeData();
        fg::NoteEvent blk[4];
        CHECK (p.process (kSr, 0, h, s, data, blk, 4) == 0);
        CHECK (p.process (0.0, 512, h, s, data, blk, 4) == 0);
        CHECK (p.process (kSr, 512, h, s, data, blk, 0) == 0);
    }


    // =====================================================================================================================
    // Milestone 4: orders, Scale / Key / Root and RESET in the clock
    // =====================================================================================================================
    {   // every order, a few lengths: the note at boundary k is the step orderStepIndex (k), and every block size gives the same notes
        const int lengths[] = { 1, 3, 8, 13, 20 };
        for (int order = 0; order < fg::kNumOrders; ++order)
            for (int L : lengths)
            {
                std::vector<Ev> first;
                for (int block : { 512, 64, 1, 8192 })
                {
                    if (block == 1 && (L != 8 || order > 2)) continue;           // 1-sample blocks are slow: a few cases are enough
                    fg::TrunkPlayer p;
                    Run r = makeRun (block == 1 ? 12000 : 192000);
                    r.block = block;
                    const int ord = order, len = L;
                    r.settings = [=] (long long) { fg::PlayerSettings s = plainSettings(); s.length = len; s.order = ord; s.pendRepeat = ord == 2 && len == 8; s.seed = 99; return s; };
                    const auto ev = simulate (p, r);
                    const auto a = analyse (ev);
                    CHECK (a.ok && a.sounding <= 1);
                    const auto ons = onsOf (ev);
                    bool right = ! ons.empty();
                    for (std::size_t i = 0; i < ons.size(); ++i)
                    {
                        const int idx = fg::orderStepIndex ((long long) i, L, order, order == 2 && L == 8, 99, fg::ChanceMix());
                        right = right && ons[i].note == 48 + idx && std::llabs (ons[i].t - 6000LL * (long long) i) <= 1;
                    }
                    CHECK (right);
                    if (block == 512) first = ev;
                    else if (block != 1) CHECK (sameEvents (first, ev));
                }
            }
    }

    {   // the step shown to the window follows the order, also when Logic starts in the middle of a step
        fg::TrunkPlayer p;
        fg::HostBlock h; h.playing = true; h.havePpq = true; h.ppq = 5 * 0.25 + 0.1;
        fg::PlayerSettings s = plainSettings();
        s.order = fg::kOrderReverse;
        const auto data = makeData();
        fg::NoteEvent blk[8];
        p.process (kSr, 64, h, s, data, blk, 8);
        CHECK (p.currentStep() == 7 - 5);
        s.order = fg::kOrderConverge;
        fg::TrunkPlayer q;
        q.process (kSr, 64, h, s, data, blk, 8);
        CHECK (q.currentStep() == fg::orderStepIndex (5, 8, fg::kOrderConverge, false, 1, fg::ChanceMix()));
    }

    {   // Scale, Key and Root: a stored place plays the pitch fg::pitchFor gives, applied as each note starts
        fg::TrunkPlayer p;
        Run r = makeRun (24000);
        r.data.steps[0].note = 12;                                                    // octave 1, degree 0
        r.data.steps[1].note = 14;                                                    // degree 2
        r.settings = [] (long long) { fg::PlayerSettings s; s.scale = 0; s.key = 2; s.root = 2; s.length = 2; return s; };
        const auto ons = onsOf (simulate (p, r));
        CHECK (ons.size() >= 2 && ons[0].note == fg::pitchFor (12, 0, 2, 2) && ons[1].note == fg::pitchFor (14, 0, 2, 2));
        CHECK (ons[0].note == 66 && ons[1].note == 69);                              // key D, Root +2: the place of degree 0 plays F# (62 + 4), degree 2 plays A (62 + 7)
    }
    {   // changing Key or Scale while playing changes the next note, never one that is sounding; every note-off matches its note-on
        fg::TrunkPlayer p;
        Run r = makeRun (96000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.length = 4; s.key = t >= 40000 ? 5 : 0; s.scale = t >= 70000 ? 0 : kChromatic; return s; };
        const auto ev = simulate (p, r);
        const auto a = analyse (ev);
        CHECK (a.ok && a.sounding <= 1);
        bool keyed = true;
        for (const auto& e : onsOf (ev))
        {
            const int stored = (int) floorMod ((long long) std::llround (e.ppq / 0.25), 4);
            const int expected = fg::pitchFor (stored, e.t >= 70000 ? 0 : kChromatic, e.t >= 40000 ? 5 : 0, 0);
            keyed = keyed && e.note == expected;
        }
        CHECK (keyed);
    }

    {   // RESET: the next step to start plays step 1 of the order, and counting goes on from there
        fg::TrunkPlayer p;
        Run r = makeRun (96000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.resetCount = t >= 20000 ? 1 : 0; return s; };   // pressed between boundary 3 (18000) and 4 (24000)
        const auto ons = onsOf (simulate (p, r));
        bool ok = ons.size() >= 12;
        for (std::size_t i = 0; ok && i < ons.size(); ++i)
        {
            const int expected = i < 4 ? (int) i : (int) ((i - 4) % 8);
            ok = ons[i].note == 48 + expected && std::llabs (ons[i].t - 6000LL * (long long) i) <= 1;
        }
        CHECK (ok);
    }
    {   // RESET with Reverse restarts the reverse pass; with Pendulum it restarts the pendulum
        for (int order : { (int) fg::kOrderReverse, (int) fg::kOrderPendulum })
        {
            fg::TrunkPlayer p;
            Run r = makeRun (96000);
            const int ord = order;
            r.settings = [=] (long long t) { fg::PlayerSettings s = plainSettings(); s.order = ord; s.resetCount = t >= 20000 ? 1 : 0; return s; };
            const auto ons = onsOf (simulate (p, r));
            bool ok = ons.size() >= 12;
            for (std::size_t i = 4; ok && i < ons.size(); ++i)
                ok = ons[i].note == 48 + fg::orderStepIndex ((long long) (i - 4), 8, ord, false, 1, fg::ChanceMix());
            CHECK (ok);
        }
    }
    {   // RESET pressed twice (two separate presses) restarts twice; a press in the same block as a boundary applies to that boundary
        fg::TrunkPlayer p;
        Run r = makeRun (96000);
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.resetCount = (t >= 20000 ? 1 : 0) + (t >= 50000 ? 1 : 0); return s; };   // boundary 8 is at 48000, 9 at 54000
        const auto ons = onsOf (simulate (p, r));
        bool ok = ons.size() >= 14;
        for (std::size_t i = 0; ok && i < ons.size(); ++i)
        {
            int expected;
            if (i < 4) expected = (int) i;
            else if (i < 9) expected = (int) (i - 4);
            else expected = (int) (i - 9);
            ok = ons[i].note == 48 + expected % 8;
        }
        CHECK (ok);
    }
    {   // a RESET count that is already above 0 when the instance starts is not a press
        fg::TrunkPlayer p;
        Run r = makeRun (60000);
        r.settings = [] (long long) { fg::PlayerSettings s = plainSettings(); s.resetCount = 5; return s; };
        const auto ons = onsOf (simulate (p, r));
        bool ok = ons.size() >= 8;
        for (std::size_t i = 0; ok && i < ons.size(); ++i) ok = ons[i].note == 48 + (int) (i % 8);
        CHECK (ok);
    }
    {   // a RESET pressed while Logic is stopped has nothing to restart: playback then starts locked to the bars
        fg::TrunkPlayer p;
        Run r = makeRun (60000);
        r.host = [] (long long t)
        {
            fg::HostBlock h; h.playing = t >= 12288; h.havePpq = true; h.bpm = kBpm;
            h.ppq = 0.75 + (double) (t >= 12288 ? t - 12288 : 0) * kPps;            // starts playing at the start of step 3 (0.75 quarter)
            return h;
        };
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.resetCount = t >= 6144 ? 1 : 0; return s; };
        const auto ons = onsOf (simulate (p, r));
        CHECK (! ons.empty() && ons[0].note == 48 + 3);
    }
    {   // RESET is forgotten when Logic jumps: after a jump the step is the one that belongs to Logic's bars again
        fg::TrunkPlayer p;
        Run r = makeRun (120000);
        r.host = [] (long long t)
        {
            fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm;
            h.ppq = t < 61440 ? (double) t * kPps : 10.25 + (double) (t - 61440) * kPps;      // jump to 10.25 quarters = boundary 41
            return h;
        };
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.resetCount = t >= 20000 ? 1 : 0; return s; };
        const auto ev = simulate (p, r);
        long long firstAfter = -1; int noteAfter = -1;
        for (const auto& e : onsOf (ev)) if (e.t >= 61440) { firstAfter = e.t; noteAfter = e.note; break; }
        CHECK (firstAfter >= 0 && noteAfter == 48 + 41 % 8);                           // not the reset-based step (41 - 4) % 8 = 5
        CHECK (analyse (ev).ok);
    }
    {   // the same after Logic stops and starts again
        fg::TrunkPlayer p;
        Run r = makeRun (120000);
        r.host = [] (long long t)
        {
            fg::HostBlock h; h.playing = t < 40960 || t >= 61440; h.havePpq = true; h.bpm = kBpm;
            h.ppq = (double) t * kPps;                                                  // the song position keeps moving, as in Logic's "stop, then play from here"
            return h;
        };
        r.settings = [] (long long t) { fg::PlayerSettings s = plainSettings(); s.resetCount = t >= 20000 ? 1 : 0; return s; };
        const auto ev = simulate (p, r);
        long long afterRestart = -1; int note = -1; long long k = -1;
        for (const auto& e : onsOf (ev)) if (e.t >= 61440) { afterRestart = e.t; note = e.note; k = (long long) std::llround (e.ppq / 0.25); break; }
        CHECK (afterRestart >= 0 && note == 48 + (int) floorMod (k, 8));
    }
    {   // two instances with the same settings stay together in every order, even when one starts later (the random orders too)
        for (int order = 0; order < fg::kNumOrders; ++order)
        {
            fg::TrunkPlayer a, b;
            Run ra = makeRun (192000), rb = makeRun (96000);
            rb.host = linearHost (96000.0 * kPps);                                       // starts at the middle, as if on another track
            rb.block = 777;
            const int ord = order;
            ra.settings = rb.settings = [=] (long long) { fg::PlayerSettings s = plainSettings(); s.order = ord; s.length = 11; s.seed = 4242; return s; };
            const auto ea = onsOf (simulate (a, ra)), eb = onsOf (simulate (b, rb));
            bool locked = ! eb.empty();
            for (std::size_t i = 0; i < eb.size() && locked; ++i)
            {
                const std::size_t j = i + 16;                                            // rb began 96000 samples = 16 steps later
                locked = j < ea.size() && ea[j].note == eb[i].note;
            }
            CHECK (locked);
        }
    }

    // =====================================================================================================================
    // Milestone 5: ties, ratchets, slew (CC 84), Mod (CC 1), the cap of 100 CC messages a second
    {
        auto settingsWith = [] (int modMode, bool cc, int rate = fg::kDefaultRate, int ratio = fg::kDefaultRatio, int length = 8)
        {
            return [=] (long long) { fg::PlayerSettings st = plainSettings(); st.modMode = modMode; st.sendCc = cc; st.rate = rate; st.ratio = ratio; st.length = length; return st; };
        };
        auto ccOf = [] (const std::vector<Ev>& ev, int cc) { std::vector<Ev> r; for (const auto& e : ev) if (e.cc == cc) r.push_back (e); return r; };
        auto notesOf = [] (const std::vector<Ev>& ev) { std::vector<Ev> r; for (const auto& e : ev) if (e.cc < 0) r.push_back (e); return r; };
        auto near = [] (long long a, long long b, long long tol = 1) { return std::llabs (a - b) <= tol; };
        const long long step = 6000;                          // one 1/16 step at 120 bpm and 48 kHz

        // ---- nothing but notes unless CC is asked for ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (96000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) { st.slew = 60; st.mod = 70; }
            const auto ev = simulate (p, r);
            CHECK (ccOf (ev, 1).empty() && ccOf (ev, 84).empty() && notesOf (ev).size() == ev.size());
        }

        // ---- ties ----
        {
            fg::TrunkPlayer p;                                  // the same pitch tied all the way: one note, never re-triggered
            Run r = makeRun (48000);
            r.data = makeData (100);
            for (auto& st : r.data.steps) st.note = 5;
            const auto ev = simulate (p, r);
            const auto a = analyse (ev, true);
            CHECK (a.ok && a.ons == 1 && a.offs == 0 && a.sounding == 1 && p.gateOpen());
        }
        {
            fg::TrunkPlayer p;                                  // a tie into a step with a shorter gate: new note first, old note off right after, then the short gate
            Run r = makeRun (24000);
            r.data = makeData (50);
            r.data.steps[0].gate = 100;
            const auto ev = simulate (p, r);
            bool pair = false, shortOff = false, noEarlyOff = true;
            for (std::size_t i = 0; i + 1 < ev.size(); ++i)
                if (ev[i].t == step && ev[i].on && ev[i].note == 49 && ev[i + 1].t == step && ! ev[i + 1].on && ev[i + 1].note == 48) pair = true;
            for (const auto& e : ev)
            {
                if (! e.on && e.note == 49 && near (e.t, step + 3000)) shortOff = true;
                if (! e.on && e.note == 48 && e.t < step) noEarlyOff = false;
            }
            CHECK (pair && shortOff && noEarlyOff && analyse (ev, true).ok);
        }
        {
            fg::TrunkPlayer p;                                  // 99 % is not a tie
            Run r = makeRun (12000);
            r.data = makeData (99);
            const auto ev = simulate (p, r);
            CHECK (analyse (ev).ok && ev.size() == 4 && near (ev[1].t, 5940) && near (ev[3].t, step + 5940));
        }
        {
            fg::TrunkPlayer p;                                  // slew on the NEXT step holds this note up to it (legato), so a mono synth can glide
            Run r = makeRun (24000);
            r.data = makeData (50);
            r.data.steps[1].slew = 40;
            const auto ev = simulate (p, r);
            bool pair = false, earlyOff = false, secondShort = false;
            for (std::size_t i = 0; i + 1 < ev.size(); ++i)
                if (ev[i].t == step && ev[i].on && ev[i].note == 49 && ev[i + 1].t == step && ! ev[i + 1].on && ev[i + 1].note == 48) pair = true;
            for (const auto& e : ev)
            {
                if (! e.on && e.note == 48 && e.t < step) earlyOff = true;
                if (! e.on && e.note == 49 && near (e.t, step + 3000)) secondShort = true;
            }
            CHECK (pair && ! earlyOff && secondShort);
        }
        {
            fg::TrunkPlayer p;                                  // a ratcheted step is never tied, gate length or not
            Run r = makeRun (12000);
            r.data = makeData (100);
            for (auto& st : r.data.steps) st.ratchet = 1;       // x2
            const auto ev = simulate (p, r);
            const auto a = analyse (ev);                        // strict: no overlaps anywhere
            CHECK (a.ok && a.ons == 4 && a.offs == 4 && near (ev[0].t, 0) && near (ev[2].t, 3000) && near (ev[1].t, 1500) && near (ev[3].t, 4500));
        }

        // ---- ratchets: every amount of the list, two block sizes, notes evenly spaced and half a slot long (never under 1 ms) ----
        for (int idx = 1; idx < fg::kNumRatchets; ++idx)
            for (int block : { 64, 5000 })
            {
                fg::TrunkPlayer p;
                Run r = makeRun (4 * step);
                r.block = block;
                r.data = makeData (100);                         // the gate length does not apply
                for (auto& st : r.data.steps) st.ratchet = (std::uint8_t) idx;
                const int amount = fg::kRatchetAmounts[idx];
                const auto ev = simulate (p, r);
                const auto a = analyse (ev);
                const double slot = (double) step / amount;
                const double width = std::min (std::max (0.5 * slot, 48.0), 0.9 * slot);
                bool good = a.ok && a.ons == 4 * amount && a.offs == 4 * amount;
                for (int st = 0; st < 4 && good; ++st)
                    for (int q = 0; q < amount && good; ++q)
                    {
                        const std::size_t i = (std::size_t) (2 * (st * amount + q));
                        good = i + 1 < ev.size() && ev[i].on && ! ev[i + 1].on && ev[i].note == 48 + st
                            && near (ev[i].t, (long long) std::llround (st * (double) step + q * slot))
                            && near (ev[i + 1].t - ev[i].t, (long long) std::llround (width), 2);
                    }
                CHECK (good);
            }

        // ---- slew: CC 84 before the note, only when the value changes, none for OFF steps, sent again after a jump ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (48000);
            r.data = makeData (50);
            for (int i = 0; i < 8; ++i) r.data.steps[(std::size_t) i].slew = (std::uint8_t) ((i * 13) % 101);
            r.data.steps[2].on = false;
            r.data.steps[5].slew = r.data.steps[4].slew;        // unchanged value: not sent again
            r.settings = settingsWith (0, true);
            const auto ev = simulate (p, r);
            const auto cc84 = ccOf (ev, 84);
            int last = -1; std::size_t idx = 0; bool good = true;
            for (int i = 0; i < 8; ++i)
            {
                if (! r.data.steps[(std::size_t) i].on) continue;
                const int v = (int) std::lround (r.data.steps[(std::size_t) i].slew * 127.0 / 100.0);
                if (v != last) { good = good && idx < cc84.size() && cc84[idx].value == v && near (cc84[idx].t, i * step); ++idx; last = v; }
            }
            CHECK (good && idx == cc84.size() && cc84.size() >= 5);
            bool before = true;                                  // the CC comes before the note of the same sample
            for (std::size_t i = 0; i < ev.size(); ++i)
                if (ev[i].cc == 84)
                    for (std::size_t j = 0; j < i; ++j)
                        if (ev[j].cc < 0 && ev[j].t == ev[i].t && ev[j].on) before = false;
            CHECK (before);
        }
        {
            fg::TrunkPlayer p;                                  // the same slew everywhere: sent once; after a jump, sent again
            Run r = makeRun (96000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) st.slew = 50;
            r.settings = settingsWith (0, true);
            r.host = [] (long long t) { fg::HostBlock h; h.playing = true; h.havePpq = true; h.ppq = (double) t * kPps + (t >= 48000 ? 8.0 : 0.0); return h; };
            const auto ev = simulate (p, r);
            const auto cc84 = ccOf (ev, 84);
            CHECK (cc84.size() == 2 && cc84[0].value == 64 && cc84[0].t == 0 && cc84[1].t >= 48000 && cc84[1].t <= 48000 + step);
        }

        // ---- Mod, Velocity: a fixed level per step, also the note's velocity (lowest 1); OFF steps send 0 ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (48000);
            r.data = makeData (50);
            for (int i = 0; i < 8; ++i) r.data.steps[(std::size_t) i].mod = (std::uint8_t) ((i * 13) % 101);
            r.data.steps[3].on = false;
            r.settings = settingsWith (1, true);
            const auto ev = simulate (p, r);
            const auto cc1 = ccOf (ev, 1);
            const auto ons = onsOf (ev);
            int last = -1; std::size_t idx = 0, onIdx = 0; bool good = true, vel = true;
            for (int i = 0; i < 8; ++i)
            {
                const auto& st = r.data.steps[(std::size_t) i];
                const int v = st.on ? (int) std::lround (st.mod * 127.0 / 100.0) : 0;
                if (v != last) { good = good && idx < cc1.size() && cc1[idx].value == v && near (cc1[idx].t, i * step); ++idx; last = v; }
                if (st.on) { vel = vel && onIdx < ons.size() && ons[onIdx].velocity == std::max (1, v); ++onIdx; }
            }
            CHECK (good && vel && idx == cc1.size() && onIdx == ons.size());
        }
        {
            fg::TrunkPlayer p;                                  // other modes: velocity 100
            Run r = makeRun (12000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) st.mod = 20;
            r.settings = settingsWith (2, true);
            const auto ons = onsOf (simulate (p, r));
            CHECK (ons.size() == 2 && ons[0].velocity == 100 && ons[1].velocity == 100);
        }

        // ---- Mod, Shapes: a fixed level for the "fixed" rates (sent once), a sine over one step ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (48000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) { st.modRate = 14; st.modShape = 3; }
            r.settings = settingsWith (0, true);
            const auto cc1 = ccOf (simulate (p, r), 1);
            CHECK (cc1.size() == 1 && cc1[0].value == (int) std::lround (127.0 * 0.5 * 3.0 / 7.0));
        }
        {
            fg::TrunkPlayer p;                                  // sine, x1, a step of a quarter (24000 samples): up to full scale and back, on a 10 ms grid
            Run r = makeRun (48000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) { st.modRate = 0; st.modShape = 0; }
            r.settings = settingsWith (0, true, 2, fg::kDefaultRatio, 4);
            const auto cc1 = ccOf (simulate (p, r), 1);
            int peak = 0; bool grid = true, up = true, down = true; int prev = -1; long long peakT = 0;
            for (const auto& e : cc1)
            {
                if (e.t >= 24000) break;
                if (e.value > peak) { peak = e.value; peakT = e.t; }
                if (e.t % 480 != 0) grid = false;
                if (e.t <= 12000 && e.value < prev) up = false;
                if (e.t > 12480 && e.value > prev) down = false;
                prev = e.value;
            }
            CHECK (! cc1.empty() && cc1[0].t == 0 && cc1[0].value == 0 && peak >= 126 && near (peakT, 12000, 480) && grid && up && down && prev <= 3);
        }
        {
            fg::TrunkPlayer p;                                  // x2 ratchet: two sines in a step
            Run r = makeRun (24000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) { st.modRate = 2; st.modShape = 0; }
            r.settings = settingsWith (0, true, 2, fg::kDefaultRatio, 4);
            const auto cc1 = ccOf (simulate (p, r), 1);
            int peaks = 0;
            for (std::size_t i = 1; i + 1 < cc1.size(); ++i)
                if (cc1[i].value >= 125 && cc1[i].value >= cc1[i - 1].value && cc1[i].value > cc1[i + 1].value) ++peaks;
            CHECK (peaks == 2);
        }
        {
            fg::TrunkPlayer p;                                  // OFF step in Shapes: 0
            Run r = makeRun (12000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) { st.modRate = 14; st.modShape = 7; }
            r.data.steps[1].on = false;
            r.settings = settingsWith (0, true);
            const auto cc1 = ccOf (simulate (p, r), 1);
            CHECK (cc1.size() == 2 && cc1[0].value == 64 && cc1[1].value == 0 && near (cc1[1].t, step));
        }

        // ---- Mod, Envelope: from full scale down to nothing over the step (peak at the very start) ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (24000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) st.mod = 0;
            r.settings = settingsWith (2, true, 2, fg::kDefaultRatio, 4);
            const auto cc1 = ccOf (simulate (p, r), 1);
            bool down = true;
            for (std::size_t i = 1; i < cc1.size(); ++i) down = down && cc1[i].value <= cc1[i - 1].value;
            CHECK (cc1.size() > 20 && cc1[0].value == 127 && cc1[0].t == 0 && down && cc1.back().value <= 3);
        }
        {
            fg::TrunkPlayer p;                                  // peak in the middle
            Run r = makeRun (24000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) st.mod = 50;
            r.settings = settingsWith (2, true, 2, fg::kDefaultRatio, 4);
            const auto cc1 = ccOf (simulate (p, r), 1);
            int peak = 0; long long peakT = 0;
            for (const auto& e : cc1) if (e.value > peak) { peak = e.value; peakT = e.t; }
            CHECK (cc1[0].value == 0 && peak >= 125 && near (peakT, 12000, 480) && cc1.back().value <= 6);
        }

        // ---- Mod, Smooth: a straight glide from the last level to the next ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (96000);
            r.data = makeData (100);
            for (int i = 0; i < 4; ++i) r.data.steps[(std::size_t) i].mod = (std::uint8_t) (i % 2 == 1 ? 100 : 0);
            r.settings = settingsWith (3, true, 2, fg::kDefaultRatio, 4);
            const auto cc1 = ccOf (simulate (p, r), 1);
            bool rise = true, fall = true; int topValue = 0, endValue = -1;
            int prev = -1;
            for (const auto& e : cc1)
            {
                if (e.t > 24000 && e.t < 48000) { rise = rise && e.value >= prev; topValue = std::max (topValue, e.value); }
                if (e.t > 48000 && e.t < 72000) { fall = fall && e.value <= prev; }
                if (e.t >= 48000 && e.t < 72000) endValue = e.value;
                prev = e.value;
            }
            CHECK (cc1[0].value == 127 && rise && fall && topValue >= 120 && endValue <= 4);       // step 1 glides down from the level of step 4 (the pattern is a loop); the 100 a second cap may drop the very last tick
        }

        // ---- the cap: never more than 100 CC messages in any one second, even at the fastest settings ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (3 * 48000);
            r.block = 512;
            r.data = makeData (50);
            for (int i = 0; i < 8; ++i) { auto& st = r.data.steps[(std::size_t) i]; st.slew = (std::uint8_t) (i % 2 ? 100 : 0); st.modRate = 7; st.modShape = 0; }
            r.host = linearHost (0.0, 300.0);                  // 300 bpm, 1/32 at x8: 320 steps a second
            r.settings = settingsWith (0, true, 8, 8, 8);
            const auto ev = simulate (p, r);
            std::vector<long long> times;
            for (const auto& e : ev) if (e.cc >= 0) times.push_back (e.t);
            bool capped = times.size() > 250;                  // plenty were sent (so the test means something)
            std::size_t lo = 0;
            for (std::size_t i = 0; i < times.size(); ++i)
            {
                while (times[i] - times[lo] >= 48000) ++lo;
                capped = capped && (i - lo + 1) <= 100;
            }
            CHECK (capped);
            CHECK (analyse (ev, true).sounding <= 2);
        }

        // ---- the same output whatever the block size, for all four Mod modes ----
        for (int mode = 0; mode < 4; ++mode)
        {
            fg::StepData d = makeData (70);
            for (int i = 0; i < 64; ++i)
            {
                auto& st = d.steps[(std::size_t) i];
                st.gate = (std::uint8_t) (30 + (i * 17) % 71);
                st.slew = (std::uint8_t) ((i * 37) % 101);
                st.ratchet = (std::uint8_t) (i % 8);
                st.mod = (std::uint8_t) ((i * 29) % 101);
                st.modShape = (std::uint8_t) (i % 8);
                st.modRate = (std::uint8_t) (i % 16);
                st.on = i % 6 != 4;
            }
            std::vector<Ev> reference;
            for (int block : { 512, 1, 37, 64, 4096, 8192 })
            {
                fg::TrunkPlayer p;
                Run r = makeRun (96000);
                r.block = block;
                r.data = d;
                r.settings = settingsWith (mode, true, 4, 4, 16);          // 1/8 steps (12000 samples), 16 steps
                const auto ev = simulate (p, r);
                if (block == 512) reference = ev;
                else CHECK (sameEvents (reference, ev));
                CHECK (analyse (ev, true).ok);
            }
        }

        // ---- stop and start, and jumps: the CC values are sent again ----
        {
            fg::TrunkPlayer p;
            Run r = makeRun (96000);
            r.data = makeData (50);
            for (auto& st : r.data.steps) { st.slew = 50; st.mod = 70; }
            r.settings = settingsWith (1, true);
            r.host = [] (long long t) { fg::HostBlock h; h.playing = ! (t >= 24000 && t < 36000); h.havePpq = true; h.ppq = (double) t * kPps; return h; };
            const auto ev = simulate (p, r);
            const auto cc84 = ccOf (ev, 84), cc1 = ccOf (ev, 1);
            CHECK (cc84.size() == 2 && cc1.size() == 2 && cc84[1].t >= 36000 && cc1[1].t >= 36000);
            CHECK (analyse (ev, true).ok);       // slew on every step: every step is held into the next (legato)
        }
    }

    // ================= milestones 6 and 7: branches, Mutate and Micro Mutate in the player =================
    {
        constexpr double kStep16 = 0.25;                         // a 1/16 step in quarter notes (6000 samples at 120 bpm, 48 kHz)
        struct Out { std::vector<Ev> ev; fg::StepData trunk; std::vector<fg::MutationEvent> muts; };

        // runs the player over `total` samples in blocks, with a trunk that Mutate may change
        auto runBranches = [] (const fg::PlayerSettings& s, fg::StepData trunk, const fg::StepData& source, long long total, int block, double startPpq = 0.0)
        {
            Out o;
            fg::TrunkPlayer p;
            std::vector<fg::NoteEvent> buf (8192);
            std::vector<fg::MutationEvent> mbuf (fg::kMaxMutationLog);
            for (long long t = 0; t < total; t += block)
            {
                const int n = (int) std::min<long long> (block, total - t);
                fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = startPpq + (double) t * kPps;
                const int count = p.process (kSr, n, h, s, trunk, source, buf.data(), (int) buf.size());
                for (int i = 0; i < count; ++i)
                {
                    Ev e; e.t = t + buf[(std::size_t) i].offset; e.on = buf[(std::size_t) i].on; e.note = buf[(std::size_t) i].note;
                    e.velocity = buf[(std::size_t) i].velocity; e.cc = buf[(std::size_t) i].cc; e.value = buf[(std::size_t) i].value;
                    e.ppq = h.ppq + (double) buf[(std::size_t) i].offset * kPps;
                    o.ev.push_back (e);
                }
                const int m = p.takeMutations (mbuf.data(), (int) mbuf.size());
                for (int i = 0; i < m; ++i) o.muts.push_back (mbuf[(std::size_t) i]);
            }
            o.trunk = trunk;
            return o;
        };
        auto settingsFor = [&] (int length, int branches, int path, std::uint32_t seed)
        {
            fg::PlayerSettings s = plainSettings();
            s.length = length; s.branches = branches; s.path = path; s.branchSeed = seed; s.seed = 77;
            return s;
        };
        auto noteOnsAtStep = [] (const std::vector<Ev>& ev)
        {
            std::vector<int> notes;
            for (const auto& e : ev) if (e.on) notes.push_back (e.note);
            return notes;
        };

        // ---- the loop is the trunk, then each branch of the path, (branches + 1) x LENGTH steps long ----
        for (int length : { 4, 8, 16 })
            for (int branches : { 1, 3, 7 })
                for (int path : { 0, 1, (1 << branches) - 1 })
                {
                    const fg::StepData trunk = makeData (50);
                    const fg::PlayerSettings s = settingsFor (length, branches, path, 4242);
                    fg::BranchSet set;
                    fg::buildBranches (set, trunk, length, kChromatic, branches, path, 4242);
                    const long long loopSteps = (long long) (branches + 1) * length;
                    const long long total = (long long) (2 * loopSteps + 3) * 6000;
                    const auto o = runBranches (s, trunk, trunk, total, 512);
                    const auto notes = noteOnsAtStep (o.ev);
                    bool ok = o.ev.size() > 0 && analyse (o.ev, true).ok;
                    // the notes that should sound: every step that is ON, except that a step tied from a gate of 100 % at the same pitch does not sound again
                    std::vector<int> expected;
                    int tiedPitch = -1;
                    for (long long i = 0; i < 2 * loopSteps + 3; ++i)
                    {
                        const int section = (int) ((i / length) % (branches + 1));
                        const int index = (int) (i % length);
                        const fg::Step& st = section == 0 ? trunk.steps[(std::size_t) index] : set.steps[(std::size_t) (section - 1)][(std::size_t) index];
                        if (! st.on) { tiedPitch = -1; continue; }
                        const int pitch = fg::pitchFor (st.note, kChromatic, 0, 0);
                        if (pitch != tiedPitch) expected.push_back (pitch);
                        tiedPitch = st.gate >= 100 ? pitch : -1;
                    }
                    ok = ok && notes == expected;
                    CHECK (ok);
                    CHECK (o.muts.empty());
                }

        // ---- the branches play one step in step with Logic's bars: starting in the middle gives the same notes as starting at the beginning ----
        {
            const fg::StepData trunk = makeData (50);
            const fg::PlayerSettings s = settingsFor (8, 3, 5, 99);
            const auto whole = runBranches (s, trunk, trunk, 60 * 6000, 512, 0.0);
            const auto late = runBranches (s, trunk, trunk, 20 * 6000, 512, 40 * kStep16);       // starts at step 40 (section 1 of the second loop)
            const auto wholeNotes = noteOnsAtStep (whole.ev), lateNotes = noteOnsAtStep (late.ev);
            // find where step 40 starts in the whole run: count note-ons before position 40 steps
            std::size_t before = 0;
            for (const auto& e : whole.ev) if (e.on && e.ppq < 40 * kStep16 - 1e-6) ++before;
            bool same = ! lateNotes.empty() && before + lateNotes.size() <= wholeNotes.size();
            for (std::size_t i = 0; same && i < lateNotes.size(); ++i) same = lateNotes[i] == wholeNotes[before + i];
            CHECK (same);
        }

        // ---- the same at every block size ----
        {
            const fg::StepData trunk = makeData (50);
            const fg::PlayerSettings s = settingsFor (8, 5, 21, 31337);
            const auto ref = runBranches (s, trunk, trunk, 80 * 6000, 512);
            for (int block : { 1, 37, 64, 480, 4096, 8192 })
                CHECK (sameEvents (ref.ev, runBranches (s, trunk, trunk, 80 * 6000, block).ev));
        }

        // ---- the branches come from the snapshot, not from the live trunk ----
        {
            const fg::StepData source = makeData (50);
            fg::StepData live = source;
            for (auto& st : live.steps) st.note = (std::uint8_t) std::min (st.note + 5, fg::kStoredNoteMax);      // the trunk has moved on since the snapshot
            const fg::PlayerSettings s = settingsFor (8, 2, 2, 5);
            fg::BranchSet fromSource;
            fg::buildBranches (fromSource, source, 8, kChromatic, 2, 2, 5);
            const auto o = runBranches (s, live, source, 24 * 6000, 512);
            const auto notes = noteOnsAtStep (o.ev);
            bool trunkLive = true, branchesSnap = true;
            for (int i = 0; i < 8; ++i) trunkLive = trunkLive && notes[(std::size_t) i] == fg::pitchFor (live.steps[(std::size_t) i].note, kChromatic, 0, 0);
            for (int b = 0; b < 2; ++b)
                for (int i = 0; i < 8; ++i)
                    if (fromSource.steps[(std::size_t) b][(std::size_t) i].on)
                        branchesSnap = branchesSnap && std::find (notes.begin() + 8, notes.end(), fg::pitchFor (fromSource.steps[(std::size_t) b][(std::size_t) i].note, kChromatic, 0, 0)) != notes.end();
            CHECK (trunkLive && branchesSnap);
        }

        // ---- changing the path, the branch count or the revision builds new branches at once ----
        {
            const fg::StepData trunk = makeData (50);
            fg::TrunkPlayer p;
            std::vector<fg::NoteEvent> buf (4096);
            fg::StepData live = trunk;
            fg::PlayerSettings s = settingsFor (4, 3, 0, 8);
            auto notesOfLoop = [&] (double startPpq)          // plays 16 steps from a song position, returns the note-ons
            {
                std::vector<int> notes;
                fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = startPpq;
                const int n = p.process (kSr, 16 * 6000, h, s, live, trunk, buf.data(), (int) buf.size());
                for (int i = 0; i < n; ++i) if (buf[(std::size_t) i].cc < 0 && buf[(std::size_t) i].on) notes.push_back (buf[(std::size_t) i].note);
                return notes;
            };
            const auto pathA = notesOfLoop (0.0);
            s.path = 7;
            const auto pathB = notesOfLoop (16 * kStep16);
            CHECK (pathA != pathB);
            s.path = 0;
            const auto pathAagain = notesOfLoop (32 * kStep16);
            CHECK (pathA == pathAagain);                      // a path repeats
            s.branches = 0;
            const auto none = notesOfLoop (48 * kStep16);
            CHECK (none.size() == 16 && std::equal (none.begin(), none.begin() + 4, pathA.begin()) && std::equal (none.begin() + 4, none.begin() + 8, none.begin()));
        }

        // ---- RESET with branches: the next step is step 1 of the trunk ----
        {
            const fg::StepData trunk = makeData (50);
            fg::TrunkPlayer p;
            std::vector<fg::NoteEvent> buf (4096);
            fg::StepData live = trunk;
            fg::PlayerSettings s = settingsFor (4, 2, 3, 6);
            auto run = [&] (double ppq, int steps)
            {
                std::vector<int> notes;
                fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = ppq;
                const int n = p.process (kSr, steps * 6000, h, s, live, trunk, buf.data(), (int) buf.size());
                for (int i = 0; i < n; ++i) if (buf[(std::size_t) i].cc < 0 && buf[(std::size_t) i].on) notes.push_back (buf[(std::size_t) i].note);
                return notes;
            };
            run (0.0, 6);                                     // into the first branch
            s.resetCount++;
            const auto after = run (6 * kStep16, 4);
            CHECK (after.size() == 4);
            bool trunkAgain = after.size() == 4;
            for (int i = 0; i < 4 && trunkAgain; ++i) trunkAgain = after[(std::size_t) i] == fg::pitchFor (trunk.steps[(std::size_t) i].note, kChromatic, 0, 0);
            CHECK (trunkAgain);
        }

        // ---- Mutate while playing ----
        {
            const fg::StepData trunk = makeData (50);
            // knobs at 0: nothing changes, nothing is listed
            fg::PlayerSettings s0 = settingsFor (8, 0, 0, 1);
            const auto quiet = runBranches (s0, trunk, trunk, 64 * 6000, 512);
            CHECK (quiet.trunk == trunk && quiet.muts.empty());

            // zone 1 only moves notes; the list matches what changed; same result at every block size
            fg::PlayerSettings s1 = settingsFor (8, 0, 0, 1);
            s1.mutate = 0.5;                                  // 100 % note mutation
            const auto m1 = runBranches (s1, trunk, trunk, 64 * 6000, 512);
            bool onlyNotes = true; int changedSteps = 0;
            for (int i = 0; i < 8; ++i)
            {
                const auto& a = m1.trunk.steps[(std::size_t) i]; const auto& b = trunk.steps[(std::size_t) i];
                onlyNotes = onlyNotes && a.gate == b.gate && a.on == b.on && a.slew == b.slew && a.ratchet == b.ratchet && a.mod == b.mod;
                if (a.note != b.note) ++changedSteps;
            }
            CHECK (onlyNotes && changedSteps >= 6);
            CHECK ((int) m1.muts.size() == 64);               // every step that started changed (100 %)
            fg::StepData replay = trunk;                      // applying the list in order gives the trunk the player ended with
            for (const auto& m : m1.muts) replay.steps[(std::size_t) m.index] = m.step;
            CHECK (replay == m1.trunk);
            for (int block : { 1, 37, 4096 })
            {
                const auto other = runBranches (s1, trunk, trunk, 64 * 6000, block);
                CHECK (other.trunk == m1.trunk && sameEvents (m1.ev, other.ev));
            }

            // the notes that sound are the notes the step has as it starts: the mutated ones
            {
                std::size_t k = 0; bool ok = true;
                const auto notes = noteOnsAtStep (m1.ev);
                for (const auto& m : m1.muts)
                {
                    if (k >= notes.size()) { ok = false; break; }
                    ok = ok && notes[k] == fg::pitchFor (m.step.note, kChromatic, 0, 0);
                    ++k;
                }
                CHECK (ok);
            }

            // the epoch is stamped on every change
            {
                fg::PlayerSettings se = s1; se.epoch = 4321;
                const auto me = runBranches (se, trunk, trunk, 16 * 6000, 512);
                bool all = ! me.muts.empty();
                for (const auto& m : me.muts) all = all && m.epoch == 4321;
                CHECK (all);
            }

            // zone 2 (knob 1.0): gate lengths and on / off change too; some steps switch off, so fewer notes play
            fg::PlayerSettings s2 = settingsFor (8, 0, 0, 1);
            s2.mutate = 1.0;
            const auto m2 = runBranches (s2, trunk, trunk, 200 * 6000, 512);
            int gateChanged = 0, offSteps = 0;
            for (const auto& st : m2.trunk.steps) { if (st.gate != 50) ++gateChanged; if (! st.on) ++offSteps; }
            CHECK (gateChanged >= 1 && analyse (m2.ev, true).ok);
            (void) offSteps;

            // it never touches anything beyond LENGTH
            bool beyond = true;
            for (int i = 8; i < 64; ++i) beyond = beyond && m2.trunk.steps[(std::size_t) i] == trunk.steps[(std::size_t) i];
            CHECK (beyond);
        }

        // ---- Mutate with branches: branch steps change in the branch and are not listed; the trunk's are ----
        {
            const fg::StepData trunk = makeData (50);
            fg::PlayerSettings s = settingsFor (4, 2, 1, 17);
            s.mutate = 0.5;
            const auto o = runBranches (s, trunk, trunk, 12 * 6000, 512);              // exactly one loop: 4 trunk steps + 8 branch steps
            CHECK (o.muts.size() == 4);
            for (const auto& m : o.muts) CHECK (m.index >= 0 && m.index < 4);
            CHECK (analyse (o.ev, true).ok);
        }

        // ---- Micro Mutate while playing ----
        {
            const fg::StepData trunk = makeData (50);
            fg::PlayerSettings s = settingsFor (8, 0, 0, 1);
            s.microRatchet = 1.0;
            const auto r = runBranches (s, trunk, trunk, 64 * 6000, 512);
            int ratcheted = 0; bool restSame = true;
            for (int i = 0; i < 8; ++i)
            {
                ratcheted += r.trunk.steps[(std::size_t) i].ratchet != 0;
                restSame = restSame && r.trunk.steps[(std::size_t) i].note == trunk.steps[(std::size_t) i].note && r.trunk.steps[(std::size_t) i].slew == 0 && r.trunk.steps[(std::size_t) i].mod == 0;
            }
            CHECK (ratcheted >= 4 && restSame);
            CHECK (analyse (r.ev, true).ok);
            for (int block : { 1, 480, 8192 }) CHECK (sameEvents (r.ev, runBranches (s, trunk, trunk, 64 * 6000, block).ev));

            fg::PlayerSettings sl = settingsFor (8, 0, 0, 1);
            sl.microSlew = 1.0; sl.sendCc = true;
            const auto slewRun = runBranches (sl, trunk, trunk, 64 * 6000, 512);
            int slewed = 0; for (int i = 0; i < 8; ++i) slewed += slewRun.trunk.steps[(std::size_t) i].slew > 0;
            CHECK (slewed >= 3);
            {
                bool anyCc84 = false;
                for (const auto& e : slewRun.ev) anyCc84 = anyCc84 || e.cc == 84;
                CHECK (anyCc84);
            }
            //                        // the new slew amounts go out as CC 84
            CHECK (analyse (slewRun.ev, true).ok);

            fg::PlayerSettings sm = settingsFor (8, 0, 0, 1);
            sm.microMod = 1.0; sm.modMode = 2;                              // Envelope: a new Mod value each step
            const auto modRun = runBranches (sm, trunk, trunk, 64 * 6000, 512);
            int modded = 0; for (int i = 0; i < 8; ++i) modded += modRun.trunk.steps[(std::size_t) i].mod > 0;
            CHECK (modded >= 6);
            CHECK (analyse (modRun.ev, true).ok);

            // all four knobs at once, with branches, over a long run at several block sizes: same result, never a stuck note
            fg::PlayerSettings sa = settingsFor (8, 4, 9, 77);
            sa.mutate = 0.8; sa.microRatchet = 0.5; sa.microSlew = 0.5; sa.microMod = 0.5;
            const auto ref = runBranches (sa, trunk, trunk, 400 * 6000, 512);
            CHECK (analyse (ref.ev, true).ok);
            for (int block : { 64, 4096 }) CHECK (sameEvents (ref.ev, runBranches (sa, trunk, trunk, 400 * 6000, block).ev));
        }

        // ---- the settings that must not change anything: 0 branches and all four knobs at 0 plays exactly as before ----
        {
            fg::StepData data = fg::StepData::makeSample();
            fg::PlayerSettings s = plainSettings();
            s.length = 13; s.order = fg::kOrderPendulum; s.sendCc = true; s.modMode = 0;
            fg::TrunkPlayer a, b;
            std::vector<fg::NoteEvent> ba (4096), bb (4096);
            bool same = true;
            fg::StepData trunkB = data;
            for (int blockNo = 0; blockNo < 200; ++blockNo)
            {
                fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = blockNo * 512 * kPps;
                const int na = a.process (kSr, 512, h, s, data, ba.data(), 4096);                 // the read-only form
                const int nb = b.process (kSr, 512, h, s, trunkB, data, bb.data(), 4096);         // the full form
                same = same && na == nb;
                for (int i = 0; same && i < na; ++i)
                    same = ba[(std::size_t) i].offset == bb[(std::size_t) i].offset && ba[(std::size_t) i].on == bb[(std::size_t) i].on
                        && ba[(std::size_t) i].note == bb[(std::size_t) i].note && ba[(std::size_t) i].cc == bb[(std::size_t) i].cc && ba[(std::size_t) i].value == bb[(std::size_t) i].value;
            }
            CHECK (same && trunkB == data);
        }

        // ---- what the window shows: the section that is playing ----
        {
            const fg::StepData trunk = makeData (50);
            fg::TrunkPlayer p;
            fg::StepData live = trunk;
            std::vector<fg::NoteEvent> buf (4096);
            fg::PlayerSettings s = settingsFor (4, 2, 0, 3);
            std::vector<int> sections;
            for (int i = 0; i < 12; ++i)                       // one block per step
            {
                fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = i * kStep16 + 0.001;
                p.process (kSr, 5000, h, s, live, trunk, buf.data(), 4096);
                sections.push_back (p.currentSection());
            }
            const std::vector<int> expected { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2 };
            CHECK (sections == expected);
            fg::HostBlock stopped; stopped.playing = false; stopped.havePpq = true;
            p.process (kSr, 512, stopped, s, live, trunk, buf.data(), 4096);
            CHECK (p.currentSection() == -1 && p.currentStep() == -1);
        }
    }

    // ---- Milestone 8: Performance mode (Ratchet FX, Slew FX, Mutate FX and ornaments) ----
    {
        const double kStep = 0.25;                      // quarters per 1/16 step
        const long long kStepSamples = 6000;            // at 120 bpm, 48 kHz
        auto perfRun = [&] (double ratchet, double slew, double mutate, int block, long long steps, int length = 8)
        {
            Run r = makeRun (kStepSamples * steps);
            r.block = block;
            r.settings = [=] (long long) { fg::PlayerSettings s = plainSettings(); s.length = length; s.seed = 77; s.sendCc = true;
                                           s.perfRatchet = ratchet; s.perfSlew = slew; s.perfMutate = mutate; return s; };
            return r;
        };

        // the three knobs at 0 change nothing (identical to a run that never heard of them)
        {
            fg::TrunkPlayer a, b;
            Run r0 = makeRun (kStepSamples * 32);
            r0.settings = [=] (long long) { fg::PlayerSettings st = plainSettings(); st.length = 8; st.seed = 77; st.sendCc = true; return st; };     // never sets the three knobs
            const auto plain = simulate (a, r0);
            Run r1 = perfRun (0.0, 0.0, 0.0, 512, 32);
            CHECK (sameEvents (plain, simulate (b, r1), 0));
        }

        // RATCHET FX zone 2: one fixed amount for every step, eight equal bands for x1 x2 x3 x4 x6 x8 x16 x32
        for (int band = 0; band < fg::kNumRatchets; ++band)
        {
            const double knob = 0.5 + 0.5 * ((double) band + 0.5) / 8.0;
            fg::TrunkPlayer p;
            const auto ev = simulate (p, perfRun (knob, 0.0, 0.0, 512, 8));
            CHECK (fg::perfFixedRatchet (knob) == band);
            CHECK (analyse (ev).ok && analyse (ev).sounding <= 1);
            CHECK (analyse (ev).ons == 8 * fg::kRatchetAmounts[band]);
        }

        // RATCHET FX zone 1: a chance; amounts x2 x3 x4 x6 only; none at 0, nearly every step at 0.5
        {
            fg::TrunkPlayer p;
            const auto full = simulate (p, perfRun (0.5, 0.0, 0.0, 512, 64));
            int maxPerStep = 0;
            std::vector<int> per (64, 0);
            for (const auto& e : full) if (e.on) { const int k = (int) std::floor (e.ppq / kStep + 1e-6); if (k >= 0 && k < 64) ++per[(std::size_t) k]; }
            for (int c : per) maxPerStep = std::max (maxPerStep, c);
            bool allowed = true;
            for (int c : per) allowed = allowed && (c == 2 || c == 3 || c == 4 || c == 6);
            CHECK (allowed && maxPerStep <= 6);
            fg::TrunkPlayer q;
            const auto half = simulate (q, perfRun (0.25, 0.0, 0.0, 512, 64));
            const int onsHalf = analyse (half).ons;
            CHECK (onsHalf > 64 + 8 && onsHalf < 64 * 4);                                  // some steps ratcheted, not all
        }

        // SLEW FX: zone 2 fixed slew goes out as CC 84 (knob 1.0 = 100 % = 127, 0.75 = 50 % = 64); steps are held into each other for the glide
        for (const auto& pair : { std::pair<double, int> { 1.0, 127 }, std::pair<double, int> { 0.75, 64 } })
        {
            fg::TrunkPlayer p;
            const auto ev = simulate (p, perfRun (0.0, pair.first, 0.0, 512, 16));
            bool right = false, wrong = false;
            for (const auto& e : ev) if (e.cc == 84) { if (e.value == pair.second) right = true; else wrong = true; }
            CHECK (right && ! wrong);
            CHECK (analyse (ev, true).ok);
        }
        {
            fg::TrunkPlayer p;
            const auto ev = simulate (p, perfRun (0.0, 0.5, 0.0, 512, 64));                 // zone 1 at 100 %: random slew, 10 to 100 %
            int ccs = 0, bad = 0;
            for (const auto& e : ev) if (e.cc == 84) { ++ccs; if (e.value != 0 && (e.value < 13 || e.value > 127)) ++bad; }
            CHECK (ccs >= 8 && bad == 0);
            CHECK (analyse (ev, true).ok);
        }

        // MUTATE FX zone 1: notes and gates change temporarily; the stored steps never do
        {
            fg::TrunkPlayer p;
            Run r = perfRun (0.0, 0.0, 0.5, 512, 64);
            const auto ev = simulate (p, r);
            const fg::StepData before = makeData();
            CHECK (r.data == before);                                                      // the player only read the data
            int changedNotes = 0;
            for (const auto& e : ev) if (e.on) { const int k = (int) std::lround (e.ppq / kStep); if (k >= 0 && e.note != 48 + (int) floorMod (k, 8)) ++changedNotes; }
            CHECK (changedNotes >= 8);
            CHECK (analyse (ev).ok);
        }
        // and with the writable trunk the steps and the mutation log stay untouched too
        {
            fg::TrunkPlayer p;
            fg::StepData live = makeData(), snapshot = makeData();
            std::vector<fg::NoteEvent> buf (4096);
            fg::PlayerSettings s = plainSettings();
            s.perfRatchet = 0.9; s.perfSlew = 0.8; s.perfMutate = 0.95; s.sendCc = true; s.seed = 5;
            for (int i = 0; i < 400; ++i)
            {
                fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = (double) i * 512 * kPps;
                p.process (kSr, 512, h, s, live, snapshot, buf.data(), 4096);
            }
            fg::MutationEvent log[8];
            CHECK (live == makeData() && p.takeMutations (log, 8) == 0);
        }

        // the same notes at the same samples at any block size, for every combination of the three knobs
        for (double ratchet : { 0.0, 0.3, 0.8 })
            for (double slew : { 0.0, 0.4, 0.9 })
                for (double mutate : { 0.0, 0.4, 0.6, 0.75, 0.9, 1.0 })
                {
                    fg::TrunkPlayer p0;
                    const auto reference = simulate (p0, perfRun (ratchet, slew, mutate, 512, 96, 12));
                    CHECK (analyse (reference, true).ok);
                    for (int block : { 1, 77, 480, 4096, 8192 })
                    {
                        fg::TrunkPlayer p;
                        Run r = perfRun (ratchet, slew, mutate, block, 96, 12);
                        if (block == 1) r.total = kStepSamples * 12;
                        const auto ev = simulate (p, r);
                        if (block == 1)
                        {
                            // only the first 12 steps at block size 1: compare with the same part of the reference
                            std::vector<Ev> head;
                            for (const auto& e : reference) if (e.t < r.total - 2) head.push_back (e);
                            std::vector<Ev> got;
                            for (const auto& e : ev) if (e.t < r.total - 2) got.push_back (e);
                            CHECK (sameEvents (head, got, 0));
                        }
                        else
                            CHECK (sameEvents (reference, ev, 0));
                    }
                }

        // ORNAMENTS: exactly the notes the plan says, at the eighth-note positions, step by step (every step ON, forward order, length 8)
        {
            const int length = 8;
            const std::uint32_t perfSeed = 77u ^ 0x5e11f0f5u;
            const fg::StepData d = makeData();
            for (double knob : { 0.6, 0.75, 0.95, 1.0 })
            {
                fg::TrunkPlayer p;
                const long long steps = 160;
                const auto ev = simulate (p, perfRun (0.0, 0.0, knob, 512, steps, length));
                CHECK (analyse (ev).ok && analyse (ev).sounding <= 1);

                struct Exp { double ppq; int note; };
                std::vector<Exp> expected;
                int swallow = 0, ornaments = 0;
                std::vector<int> kindsSeen (fg::kNumOrnaments, 0);
                for (long long k = 0; k < steps; ++k)
                {
                    if (swallow > 0) { --swallow; continue; }
                    const fg::Step& stored = d.steps[(std::size_t) floorMod (k, length)];
                    const auto pr = fg::performStep (stored, k, perfSeed, kChromatic, { 0.0, 0.0, knob });
                    fg::Step step = pr.step;
                    bool done = false;
                    if (pr.ornament >= 0)
                    {
                        const fg::Step& nx = d.steps[(std::size_t) floorMod (k + 1, length)];
                        const fg::Step& pv = d.steps[(std::size_t) floorMod (k - 1, length)];
                        int degrees[fg::kMaxOrnamentNotes];
                        const int cnt = fg::ornamentPlan (pr.ornament, fg::storedToDegree (stored.note, kChromatic),
                                                          fg::storedToDegree (nx.note, kChromatic), fg::storedToDegree (pv.note, kChromatic), kChromatic, degrees);
                        if (cnt > 0)
                        {
                            ++ornaments; ++kindsSeen[(std::size_t) pr.ornament];
                            CHECK (cnt == fg::ornamentNotes (pr.ornament));
                            for (int i = 0; i < cnt; ++i)
                                if (degrees[i] != fg::kOrnRest)
                                    expected.push_back ({ (double) k * kStep + (double) i * kStep / 2.0, fg::pitchFor (fg::degreeToStored (degrees[i], kChromatic), kChromatic, 0, 0) });
                            swallow = fg::ornamentSteps (pr.ornament) - 1;
                            done = true;
                        }
                        else
                            step = pr.stepIfNoOrnament;
                    }
                    if (! done)
                        expected.push_back ({ (double) k * kStep, fg::pitchFor (step.note, kChromatic, 0, 0) });
                }
                std::vector<Ev> got;
                for (const auto& e : ev) if (e.on) got.push_back (e);
                bool match = true;
                std::size_t compared = 0;
                for (std::size_t i = 0; i < got.size() && i < expected.size(); ++i)
                {
                    ++compared;
                    if (got[i].note != expected[i].note || std::fabs (got[i].ppq - expected[i].ppq) > 1e-4) match = false;
                }
                CHECK (match && compared >= 100);
                CHECK (std::abs ((int) got.size() - (int) expected.size()) <= 0);
                CHECK (ornaments >= 10);
                if (knob >= 0.95)
                {
                    int kinds = 0;
                    for (int c : kindsSeen) kinds += c > 0;
                    CHECK (kinds >= 12);
                }
                // an ornament note is 80 % of an eighth: every note-off is 0.8 of an eighth after its note-on (no ratchets here)
                bool lengthsOk = true;
                for (std::size_t i = 0; i + 1 < ev.size(); ++i)
                    if (ev[i].cc < 0 && ev[i].on && i + 1 < ev.size() && ev[i + 1].cc < 0 && ! ev[i + 1].on && ev[i + 1].note == ev[i].note)
                    {
                        const double len = (double) (ev[i + 1].t - ev[i].t) / 24000.0;          // quarters
                        if (std::fabs (len - 0.5 * 0.125) > 0.001 && std::fabs (len - 0.125 * 0.8) > 0.001 && std::fabs (len - 0.25 * 0.5) > 0.001)
                            lengthsOk = false;
                    }
                CHECK (lengthsOk);
            }
        }

        // the ornament and the other performance steps end cleanly: nothing left sounding after Logic stops, jumps or cycles
        {
            fg::TrunkPlayer p;
            fg::StepData d = makeData();
            std::vector<fg::NoteEvent> buf (4096);
            fg::PlayerSettings s = plainSettings();
            s.perfMutate = 1.0; s.perfRatchet = 0.2; s.perfSlew = 0.3; s.sendCc = true; s.seed = 9;
            std::vector<int> sounding;
            auto feed = [&] (const fg::HostBlock& h, int n)
            {
                const int c = p.process (kSr, n, h, s, d, buf.data(), 4096);
                for (int i = 0; i < c; ++i)
                    if (buf[(std::size_t) i].cc < 0)
                    {
                        if (buf[(std::size_t) i].on) sounding.push_back (buf[(std::size_t) i].note);
                        else { auto it = std::find (sounding.begin(), sounding.end(), buf[(std::size_t) i].note); if (it != sounding.end()) sounding.erase (it); }
                    }
            };
            for (int round = 0; round < 40; ++round)
            {
                const double start = 0.37 * round;                                           // an odd place in a step each time
                for (int i = 0; i < 20; ++i)
                {
                    fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = start + i * 500 * kPps;
                    feed (h, 500);
                }
                fg::HostBlock stopped; stopped.playing = false; stopped.havePpq = true;
                feed (stopped, 256);
                CHECK (sounding.empty());
            }
            // a jump in the middle of an ornament
            for (int round = 0; round < 40; ++round)
            {
                for (int i = 0; i < 6; ++i)
                {
                    fg::HostBlock h; h.playing = true; h.havePpq = true; h.bpm = kBpm; h.ppq = 100.0 + round * 7.1 + i * 700 * kPps;
                    feed (h, 700);
                }
            }
            fg::HostBlock stopped; stopped.playing = false; stopped.havePpq = true;
            feed (stopped, 256);
            CHECK (sounding.empty());
        }

        // two instances with the same settings agree (the effects depend only on the song position and the seed)
        {
            fg::TrunkPlayer a, b;
            const auto ea = simulate (a, perfRun (0.7, 0.6, 0.9, 512, 64));
            Run late = perfRun (0.7, 0.6, 0.9, 700, 64);
            const auto eb = simulate (b, late);
            CHECK (sameEvents (ea, eb, 0));
        }
    }

    std::printf ("clock tests: %d checks passed\n", checks - failures);
    if (failures > 0)
    {
        std::printf ("clock tests: %d FAILED of %d checks\n", failures, checks);
        return 1;
    }
    return 0;
}
