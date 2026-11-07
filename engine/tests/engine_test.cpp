// Engine self-test (milestones 2 and 4): checks the 46 controls against the approved control list, written out here a second time
// (id, kind, range, default) so a slip in Params.h is caught, then the menus, the step data and its saved form, and (milestone 4) the
// 28 scales, the pitch a stored note plays, the 11 play orders, Rotate and Transpose, and (milestone 5) the ratchet list, the sixteen Mod
// rates, the four Mod modes' curves, Resize and the conversion of older saved projects, and (milestones 6 and 7) Default Mutate's two
// zones, Micro Mutate, the Reseed ranges, branches and Path (the binary tree).
// Prints "engine tests: N checks passed" and exits 0, or lists every failure and exits 1.

#include <fg/Branch.h>
#include <fg/Mod.h>
#include <fg/Mutate.h>
#include <fg/Orders.h>
#include <fg/Params.h>
#include <fg/Pitch.h>
#include <fg/Steps.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

static int checks = 0, failures = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

struct Expect { const char* id; char kind; int lo, hi, def; };      // kind: K knob, I whole number, C menu, T toggle, M momentary

static const Expect kExpected[46] = {
    { "branch", 'K', 0, 1, 0 }, { "path", 'K', 0, 1, 0 }, { "mutate", 'K', 0, 1, 0 },
    { "microRatchet", 'K', 0, 1, 0 }, { "microSlew", 'K', 0, 1, 0 }, { "microMod", 'K', 0, 1, 0 },
    { "perfRatchet", 'K', 0, 1, 0 }, { "perfSlew", 'K', 0, 1, 0 }, { "perfMutate", 'K', 0, 1, 0 },
    { "root", 'I', -24, 24, 0 }, { "knobMode", 'C', 0, 2, 0 }, { "length", 'I', 1, 64, 8 },
    { "scale", 'C', 0, 27, 0 }, { "key", 'C', 0, 11, 0 }, { "order", 'C', 0, 10, 0 },
    { "stepRate", 'C', 0, 8, 6 }, { "ratio", 'C', 0, 8, 4 }, { "modMode", 'C', 0, 3, 0 },
    { "patternSlot", 'C', 0, 15, 0 }, { "reseedScope", 'C', 0, 1, 0 },
    { "reset", 'M', 0, 1, 0 }, { "unmutate", 'M', 0, 1, 0 }, { "reseedAll", 'M', 0, 1, 0 },
    { "reseedBranches", 'M', 0, 1, 0 }, { "reseedNote", 'M', 0, 1, 0 }, { "reseedGate", 'M', 0, 1, 0 },
    { "reseedSlew", 'M', 0, 1, 0 }, { "reseedRatchet", 'M', 0, 1, 0 }, { "reseedMod", 'M', 0, 1, 0 },
    { "resizeShrink", 'M', 0, 1, 0 }, { "resizeExpand", 'M', 0, 1, 0 },
    { "rotateLeft", 'M', 0, 1, 0 }, { "rotateRight", 'M', 0, 1, 0 },
    { "transposeDown", 'M', 0, 1, 0 }, { "transposeUp", 'M', 0, 1, 0 },
    { "patternLoad", 'M', 0, 1, 0 }, { "patternSave", 'M', 0, 1, 0 },
    { "pendRepeat", 'T', 0, 1, 0 },
    { "chanceForward", 'I', 0, 100, 100 }, { "chanceBackward", 'I', 0, 100, 60 }, { "chancePendulum", 'I', 0, 100, 40 },
    { "chancePingPong", 'I', 0, 100, 0 }, { "chanceRandom", 'I', 0, 100, 20 }, { "chanceDrunk", 'I', 0, 100, 0 },
    { "chanceRunMin", 'I', 1, 32, 4 }, { "chanceRunMax", 'I', 1, 32, 16 },
};

static fg::Kind kindOf (char c)
{
    switch (c) { case 'K': return fg::Kind::Knob; case 'I': return fg::Kind::Int; case 'C': return fg::Kind::Choice;
                 case 'T': return fg::Kind::Toggle; default: return fg::Kind::Momentary; }
}

int main()
{
    // ---- the 46 controls ----
    CHECK (fg::kNumParameters == 46);
    std::set<std::string> seen;
    int momentary = 0;
    for (int i = 0; i < 46; ++i)
    {
        const auto& p = fg::kParams[i];
        const auto& e = kExpected[i];
        CHECK (std::strcmp (p.id, e.id) == 0);                // same id, same position
        CHECK (p.kind == kindOf (e.kind));
        CHECK (p.minValue == e.lo && p.maxValue == e.hi);
        CHECK (p.defaultValue == e.def);
        CHECK (p.defaultValue >= p.minValue && p.defaultValue <= p.maxValue);
        CHECK (p.name != nullptr && p.name[0] != 0);
        CHECK (seen.insert (p.id).second);                    // no id twice
        CHECK (fg::findParam (p.id) == &p);
        if (p.kind == fg::Kind::Choice)
        {
            CHECK (p.choices != nullptr && p.numChoices == p.maxValue + 1 && p.minValue == 0);
            for (int c = 0; c < p.numChoices; ++c)
                CHECK (p.choices[c] != nullptr && p.choices[c][0] != 0);
        }
        else
            CHECK (p.choices == nullptr && p.numChoices == 0);
        if (p.kind == fg::Kind::Momentary) ++momentary;
    }
    CHECK (momentary == 17);
    CHECK (fg::findParam ("nonsense") == nullptr && fg::findParam (nullptr) == nullptr);

    // ---- menus (the order of every list is frozen) ----
    CHECK (fg::kNumScales == 28);
    static const char* const scales[28] = {
        "Major (Ionian)", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian (Natural Minor)", "Locrian",
        "Harmonic Minor", "Melodic Minor", "Harmonic Major", "Hungarian Minor", "Phrygian Dominant",
        "Double Harmonic (Byzantine)", "Neapolitan Minor", "Neapolitan Major", "Enigmatic",
        "Major Pentatonic", "Minor Pentatonic", "Blues", "Hirajoshi", "In Sen", "Kumoi", "Iwato",
        "Whole Tone", "Octatonic (Whole-Half)", "Octatonic (Half-Whole)", "Prometheus", "Chromatic" };
    for (int i = 0; i < 28; ++i) CHECK (std::strcmp (fg::kScaleNames[i], scales[i]) == 0);

    CHECK (fg::kNumOrders == 11);
    static const char* const orders[11] = { "Forward", "Reverse", "Pendulum", "Random", "Converge", "Diverge",
                                            "Converge & Diverge", "Page Jump", "Drunk walk", "Random no-repeat", "Chance mix" };
    for (int i = 0; i < 11; ++i) CHECK (std::strcmp (fg::kOrderNames[i], orders[i]) == 0);
    CHECK (std::strcmp (fg::kOrderNames[fg::kOrderPendulum], "Pendulum") == 0);
    CHECK (std::strcmp (fg::kOrderNames[fg::kOrderChanceMix], "Chance mix") == 0);

    CHECK (fg::kNumKeys == 12 && std::strcmp (fg::kKeyNames[0], "C") == 0 && std::strcmp (fg::kKeyNames[11], "B") == 0);
    CHECK (fg::kNumRates == 9 && std::strcmp (fg::kRateNames[0], "1 bar") == 0 && std::strcmp (fg::kRateNames[fg::kDefaultRate], "1/16") == 0 && std::strcmp (fg::kRateNames[8], "1/32") == 0);
    CHECK (fg::kNumRatios == 9 && std::strcmp (fg::kRatioNames[0], "/8") == 0 && std::strcmp (fg::kRatioNames[fg::kDefaultRatio], "x1") == 0 && std::strcmp (fg::kRatioNames[8], "x8") == 0);
    CHECK (fg::kNumModModes == 4 && std::strcmp (fg::kModModeNames[0], "Shapes") == 0 && std::strcmp (fg::kModModeNames[3], "Smooth") == 0);
    CHECK (fg::kNumKnobModes == 3 && std::strcmp (fg::kKnobModeNames[1], "Micro Mutate") == 0);
    CHECK (fg::kNumPatternSlots == 16 && std::strcmp (fg::kPatternSlotNames[0], "Slot 1") == 0 && std::strcmp (fg::kPatternSlotNames[15], "Slot 16") == 0);

    // ---- note names (Logic: 60 = C3) ----
    char buf[16];
    fg::noteName (74, buf, sizeof buf); CHECK (std::strcmp (buf, "D4") == 0);
    fg::noteName (60, buf, sizeof buf); CHECK (std::strcmp (buf, "C3") == 0);
    fg::noteName (48, buf, sizeof buf); CHECK (std::strcmp (buf, "C2") == 0);
    fg::noteName (91, buf, sizeof buf); CHECK (std::strcmp (buf, "G5") == 0);
    fg::noteName (61, buf, sizeof buf); CHECK (std::strcmp (buf, "C#3") == 0);

    // ---- step data ----
    const fg::StepData sample = fg::StepData::makeSample();
    for (const auto& s : sample.steps)
    {
        CHECK (s.note >= fg::kNoteMin && s.note <= fg::kNoteMax);
        CHECK (s.gate <= fg::kGateMax && s.slew <= fg::kSlewMax && s.mod <= fg::kModMax);
        CHECK (s.ratchet <= fg::kRatchetMax);
        CHECK (s.modShape < fg::kNumModShapes && s.modRate < fg::kNumModRates);
    }
    CHECK (sample.steps[0].note == fg::nearestMajorStored (74) && sample.steps[1].note == fg::nearestMajorStored (81) && sample.steps[15].note == fg::nearestMajorStored (79));       // picture A
    CHECK (! sample.steps[5].on && ! sample.steps[11].on && sample.steps[0].on);                           // steps 6 and 12 off
    CHECK (sample.steps[16].modShape == 0 && sample.steps[18].modShape == 6 && ! sample.steps[21].on);     // picture B: sine, pluck, step 22 off

    const auto bytes = sample.toBytes();
    CHECK (bytes.size() == 2 + 64 * 8);
    fg::StepData back;                                         // starts as default steps, not the sample
    CHECK (! (back == sample));
    CHECK (back.fromBytes (bytes.data(), bytes.size()));
    CHECK (back == sample);                                    // saved and loaded: identical

    fg::StepData untouched = sample;
    std::vector<std::uint8_t> bad = bytes;
    bad[0] = 99;                                               CHECK (! untouched.fromBytes (bad.data(), bad.size()) && untouched == sample);   // unknown version
    CHECK (! untouched.fromBytes (bytes.data(), bytes.size() - 1) && untouched == sample);                                                      // truncated
    CHECK (! untouched.fromBytes (nullptr, 0) && untouched == sample);
    bad = bytes; bad[1] = 0;                                   CHECK (! untouched.fromBytes (bad.data(), bad.size()));                           // zero steps
    bad = bytes; bad[2] = 250; bad[3] = 250; bad[5] = 200;     // note, gate, ratchet out of range: pulled in
    CHECK (untouched.fromBytes (bad.data(), bad.size()));
    CHECK (untouched.steps[0].note == fg::kNoteMax && untouched.steps[0].gate == fg::kGateMax && untouched.steps[0].ratchet == fg::kRatchetMax);


    // =====================================================================================================================
    // Milestone 4: scales and pitch
    // =====================================================================================================================
    {
        static const int kExpectedSize[28] = { 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 5, 5, 5, 5, 6, 8, 8, 6, 12 };
        for (int sc = 0; sc < fg::kNumScales; ++sc)
        {
            const int n = fg::scaleSize (sc);
            CHECK (n == kExpectedSize[sc]);
            CHECK (fg::scaleInterval (sc, 0) == 0);                                           // every scale has its key note
            bool rising = true;
            for (int d = 1; d < n; ++d)
                rising = rising && fg::scaleInterval (sc, d) > fg::scaleInterval (sc, d - 1) && fg::scaleInterval (sc, d) < 12;
            CHECK (rising);
            CHECK (fg::degreeCount (sc) == 5 * n);
            // stored place <-> running degree: a place with a degree the scale has comes back unchanged
            bool roundTrip = true;
            for (int st = 0; st <= fg::kStoredNoteMax; ++st)
                if (st % fg::kDegreeSlots < n)
                    roundTrip = roundTrip && fg::degreeToStored (fg::storedToDegree (st, sc), sc) == st;
            CHECK (roundTrip);
            CHECK (fg::degreeToStored (-5, sc) == 0);
            CHECK (fg::degreeToStored (999, sc) == 4 * fg::kDegreeSlots + (n - 1));
            CHECK (fg::storedToDegree (fg::kStoredNoteMax, sc) == 5 * n - 1);                 // the top place is the top degree whatever the scale
        }
        static const int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 }, kMinPent[5] = { 0, 3, 5, 7, 10 };
        for (int d = 0; d < 7; ++d) CHECK (fg::scaleInterval (0, d) == kMajor[d]);
        for (int d = 0; d < 5; ++d) CHECK (fg::scaleInterval (17, d) == kMinPent[d]);
        CHECK (fg::scaleInterval (0, 99) == 11 && fg::scaleInterval (0, -3) == 0);            // out of range is pulled in
        CHECK (fg::scaleSize (-4) == 7 && fg::scaleSize (500) == 12);

        // the pitch a stored note plays (MIDI, Logic naming 60 = C3; the lowest note is 48 = C2)
        CHECK (fg::pitchFor (12, 0, 0, 0) == 60);                       // octave 1, degree 0, C major
        CHECK (fg::pitchFor (0, 0, 0, 0) == 48);
        CHECK (fg::pitchFor (14, 0, 0, 0) == 64);                       // degree 2 of C major = E
        CHECK (fg::pitchFor (12, 0, 2, 0) == 62);                       // key D
        CHECK (fg::pitchFor (12, 5, 9, 0) == 69 + 0);                   // A natural minor, key A, degree 0
        CHECK (fg::pitchFor (12, 5, 9, 2) == 69 + 3);                   // two scale steps up: C
        CHECK (fg::pitchFor (12, 0, 0, 1) == 62);                       // Root +1 = one scale step up (D)
        CHECK (fg::pitchFor (12, 0, 0, 7) == 72);                       // Root +7 in a seven-note scale = one octave
        CHECK (fg::pitchFor (12, 16, 0, 5) == 72);                      // Root +5 in a five-note scale = one octave
        CHECK (fg::pitchFor (12, 0, 0, -1) == 59);                      // Root -1 goes down below the octave start
        CHECK (fg::pitchFor (12, 0, 0, -7) == 48);
        for (int st = 0; st <= fg::kStoredNoteMax; ++st)                // Chromatic, key C: place s plays 48 + s
            CHECK (fg::pitchFor (st, 27, 0, 0) == 48 + st);
        CHECK (fg::pitchFor (6, 16, 0, 0) == fg::pitchFor (4, 16, 0, 0) && fg::pitchFor (4, 16, 0, 0) == 57);   // a degree the scale lacks plays its top degree
        CHECK (fg::pitchFor (59, 0, 0, 0) == 107);
        CHECK (fg::pitchFor (59, 27, 11, 24) == 127);                   // the MIDI range is the limit
        bool inRange = true;
        for (int sc = 0; sc < fg::kNumScales; ++sc)
            for (int st = 0; st <= fg::kStoredNoteMax; st += 3)
                for (int key = 0; key < 12; key += 5)
                    for (int root = -24; root <= 24; root += 6)
                    {
                        const int m = fg::pitchFor (st, sc, key, root);
                        inRange = inRange && m >= 0 && m <= 127;
                    }
        CHECK (inRange);
        // every played note belongs to the scale above the key (the pitch class, counted from the key, is a scale tone)
        bool inScale = true;
        for (int sc = 0; sc < fg::kNumScales; ++sc)
            for (int st = 0; st <= fg::kStoredNoteMax; ++st)
                for (int key = 0; key < 12; key += 4)
                {
                    const int m = fg::pitchFor (st, sc, key, 3);
                    if (m > 0 && m < 127)
                        inScale = inScale && ((fg::scaleMask (sc) >> (((m - key) % 12 + 12) % 12)) & 1u) != 0;
                }
        CHECK (inScale);

        // converting the plain MIDI notes of versions 0.2.0 and 0.3.0
        CHECK (fg::nearestMajorStored (60) == 12 && fg::nearestMajorStored (48) == 0 && fg::nearestMajorStored (62) == 13);
        CHECK (fg::nearestMajorStored (61) == 12);                      // C# is as near to C as to D: the lower
        CHECK (fg::nearestMajorStored (63) == 13);                      // D# : D
        CHECK (fg::nearestMajorStored (500) == 4 * 12 + 6 && fg::nearestMajorStored (0) == 0);
        bool majorExact = true;
        for (int midi = 48; midi < 108; ++midi)
        {
            const int stored = fg::nearestMajorStored (midi);
            const int sem = (midi - 48) % 12;
            const bool inMajor = sem == 0 || sem == 2 || sem == 4 || sem == 5 || sem == 7 || sem == 9 || sem == 11;
            if (inMajor && fg::pitchFor (stored, 0, 0, 0) != midi)
                majorExact = false;
        }
        CHECK (majorExact);                                              // notes that are in C major keep their pitch exactly
    }

    {   // a project saved by 0.2.0 / 0.3.0 (format 1, MIDI notes) opens with each note on its nearest C major degree
        std::vector<std::uint8_t> v1;
        v1.push_back (1); v1.push_back (64);
        for (int i = 0; i < 64; ++i)
        {
            v1.push_back ((std::uint8_t) (60 + (i % 12)));               // note
            v1.push_back (40); v1.push_back (0); v1.push_back (1); v1.push_back (0); v1.push_back (0); v1.push_back (0);
            v1.push_back ((std::uint8_t) (i % 2));                        // on / off
        }
        fg::StepData d;
        CHECK (d.fromBytes (v1.data(), v1.size()));
        CHECK (d.steps[0].note == 12 && d.steps[2].note == 13 && d.steps[4].note == 14 && d.steps[11].note == 18);
        CHECK (d.steps[0].gate == 40 && ! d.steps[0].on && d.steps[1].on);
        const auto v2 = d.toBytes();
        CHECK (v2[0] == 3 && v2.size() == v1.size());                    // saved again in the new format
        fg::StepData again;
        CHECK (again.fromBytes (v2.data(), v2.size()) && again == d);
    }

    // =====================================================================================================================
    // Milestone 4: Rotate and Transpose
    // =====================================================================================================================
    {
        fg::StepData d;
        for (int i = 0; i < 64; ++i) { d.steps[(std::size_t) i].gate = (std::uint8_t) i; d.steps[(std::size_t) i].note = (std::uint8_t) (i % 60); d.steps[(std::size_t) i].on = i % 3 != 0; }
        const fg::StepData original = d;

        fg::rotateSteps (d, 8, +1);                                      // RIGHT: step 2's data becomes step 1, step 1's goes to the end
        CHECK (d.steps[0].gate == 1 && d.steps[6].gate == 7 && d.steps[7].gate == 0);
        CHECK (d.steps[0].note == original.steps[1].note && d.steps[7].on == original.steps[0].on && d.steps[0].on == original.steps[1].on);
        bool beyondSame = true;
        for (int i = 8; i < 64; ++i) beyondSame = beyondSame && d.steps[(std::size_t) i] == original.steps[(std::size_t) i];
        CHECK (beyondSame);                                              // steps beyond LENGTH are not touched
        fg::rotateSteps (d, 8, -1);                                      // LEFT undoes it
        CHECK (d == original);
        fg::rotateSteps (d, 8, -1);
        CHECK (d.steps[0].gate == 7 && d.steps[1].gate == 0 && d.steps[7].gate == 6);
        fg::rotateSteps (d, 8, +1);
        CHECK (d == original);
        for (int i = 0; i < 8; ++i) fg::rotateSteps (d, 8, +1);
        CHECK (d == original);                                           // eight steps right is a full circle
        fg::rotateSteps (d, 1, +1); fg::rotateSteps (d, 0, +1); fg::rotateSteps (d, 8, 0);
        CHECK (d == original);                                           // length 1, length 0 (pulled to 1) and no direction change nothing
        fg::rotateSteps (d, 64, +1);
        CHECK (d.steps[0].gate == 1 && d.steps[63].gate == 0);
        fg::rotateSteps (d, 200, -1);
        CHECK (d == original);                                           // a length of 200 is pulled to 64

        // Transpose
        fg::StepData t;
        t.steps[0].note = 0; t.steps[1].note = 6; t.steps[2].note = 12; t.steps[3].note = 4 * 12 + 6; t.steps[4].note = 14; t.steps[5].note = 9;
        t.steps[9].note = 5;
        fg::transposeSteps (t, 8, +1, 0);                                // C major, one scale step up
        CHECK (t.steps[0].note == 1 && t.steps[1].note == 12 && t.steps[2].note == 13);   // degree 6 of octave 0 goes to degree 0 of octave 1
        CHECK (t.steps[3].note == 4 * 12 + 6);                           // already the top: it stays
        CHECK (t.steps[4].note == 15);
        CHECK (t.steps[5].note == 12);                                   // a stored degree above the scale (9) plays as the top degree (6); one step up from it is the next octave
        CHECK (t.steps[9].note == 5);                                    // beyond LENGTH: not touched
        fg::transposeSteps (t, 8, -1, 0); fg::transposeSteps (t, 8, -1, 0);
        CHECK (t.steps[0].note == 0 && t.steps[1].note == 5);            // down stops at the bottom
        fg::StepData pent;
        pent.steps[0].note = 4; pent.steps[1].note = 12;
        fg::transposeSteps (pent, 2, +1, 16);                            // Major Pentatonic: degree 4 + 1 = next octave
        CHECK (pent.steps[0].note == 12 && pent.steps[1].note == 13);
        fg::transposeSteps (pent, 2, -1, 16);
        CHECK (pent.steps[0].note == 4 && pent.steps[1].note == 12);
        // the five-octave limits: 5 * scale size degrees
        fg::StepData top;
        top.steps[0].note = (std::uint8_t) fg::degreeToStored (fg::degreeCount (17) - 1, 17);
        const auto topBefore = top.steps[0].note;
        fg::transposeSteps (top, 1, +1, 17);
        CHECK (top.steps[0].note == topBefore);
    }

    // =====================================================================================================================
    // Milestone 4: the eleven play orders
    // =====================================================================================================================
    {
        CHECK (fg::kNumOrders == 11);
        CHECK (fg::kOrderForward == 0 && fg::kOrderReverse == 1 && fg::kOrderPendulum == 2 && fg::kOrderRandom == 3 && fg::kOrderConverge == 4);
        CHECK (fg::kOrderDiverge == 5 && fg::kOrderConvergeDiverge == 6 && fg::kOrderPageJump == 7 && fg::kOrderDrunk == 8);
        CHECK (fg::kOrderRandomNoRepeat == 9 && fg::kOrderChanceMix == 10);
        CHECK (std::strcmp (fg::kOrderNames[fg::kOrderConverge], "Converge") == 0 && std::strcmp (fg::kOrderNames[fg::kOrderPageJump], "Page Jump") == 0);
        CHECK (std::strcmp (fg::kOrderNames[fg::kOrderDrunk], "Drunk walk") == 0 && std::strcmp (fg::kOrderNames[fg::kOrderChanceMix], "Chance mix") == 0);

        const fg::ChanceMix cm;
        auto seq = [&] (int order, int L, int count, bool pend = false, std::uint32_t seed = 7, long long from = 0)
        {
            std::vector<int> v;
            for (int i = 0; i < count; ++i) v.push_back (fg::orderStepIndex (from + i, L, order, pend, seed, cm));
            return v;
        };
        auto eq = [] (const std::vector<int>& a, std::initializer_list<int> b) { return a == std::vector<int> (b); };

        CHECK (eq (seq (fg::kOrderForward, 8, 10), { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1 }));
        CHECK (eq (seq (fg::kOrderReverse, 8, 10), { 7, 6, 5, 4, 3, 2, 1, 0, 7, 6 }));
        CHECK (eq (seq (fg::kOrderPendulum, 4, 13), { 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0 }));
        CHECK (eq (seq (fg::kOrderPendulum, 4, 13, true), { 0, 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3, 3 }));
        CHECK (eq (seq (fg::kOrderPendulum, 2, 6), { 0, 1, 0, 1, 0, 1 }));
        CHECK (eq (seq (fg::kOrderPendulum, 2, 6, true), { 0, 1, 1, 0, 0, 1 }));
        CHECK (eq (seq (fg::kOrderConverge, 8, 8), { 0, 7, 1, 6, 2, 5, 3, 4 }));
        CHECK (eq (seq (fg::kOrderConverge, 5, 5), { 0, 4, 1, 3, 2 }));
        CHECK (eq (seq (fg::kOrderConverge, 2, 4), { 0, 1, 0, 1 }));
        CHECK (eq (seq (fg::kOrderDiverge, 8, 8), { 4, 3, 5, 2, 6, 1, 7, 0 }));
        CHECK (eq (seq (fg::kOrderDiverge, 5, 5), { 2, 3, 1, 4, 0 }));
        CHECK (eq (seq (fg::kOrderConvergeDiverge, 4, 12), { 0, 3, 1, 2, 2, 1, 3, 0, 0, 3, 1, 2 }));
        CHECK (eq (seq (fg::kOrderForward, 8, 4, false, 7, -4), { 4, 5, 6, 7 }));                   // a count-in before bar 1 (negative counters)
        CHECK (eq (seq (fg::kOrderReverse, 8, 3, false, 7, -1), { 0, 7, 6 }));
        CHECK (eq (seq (fg::kOrderConverge, 8, 3, false, 7, -2), { 3, 4, 0 }));
        CHECK (fg::orderStepIndex (5, 1, fg::kOrderRandom, false, 7, cm) == 0 && fg::orderStepIndex (-9, 1, fg::kOrderChanceMix, false, 7, cm) == 0);

        // Forward, Reverse, Converge, Diverge, Converge & Diverge and Random no-repeat visit every step once in each pass, for every length
        bool permutations = true;
        for (int L = 1; L <= 64; ++L)
            for (int order : { 0, 1, 4, 5, 9 })
                for (int pass = 0; pass < 6; ++pass)
                {
                    std::vector<int> seen (64, 0);
                    for (int i = 0; i < L; ++i) ++seen[(std::size_t) fg::orderStepIndex ((long long) pass * L + i, L, order, false, 3, cm)];
                    for (int i = 0; i < L; ++i) permutations = permutations && seen[(std::size_t) i] == 1;
                }
        CHECK (permutations);
        bool cdPermutations = true;
        for (int L = 1; L <= 64; ++L)
            for (int pass = 0; pass < 4; ++pass)
            {
                std::vector<int> seen (64, 0);
                for (int i = 0; i < L; ++i) ++seen[(std::size_t) fg::orderStepIndex ((long long) pass * L + i, L, fg::kOrderConvergeDiverge, false, 3, cm)];
                for (int i = 0; i < L; ++i) cdPermutations = cdPermutations && seen[(std::size_t) i] == 1;
            }
        CHECK (cdPermutations);
        {   // Random no-repeat really changes from pass to pass, and from seed to seed
            bool differs = false, seedDiffers = false;
            const auto p0 = seq (fg::kOrderRandomNoRepeat, 8, 8, false, 7, 0);
            for (int pass = 1; pass < 10; ++pass) if (seq (fg::kOrderRandomNoRepeat, 8, 8, false, 7, pass * 8) != p0) differs = true;
            if (seq (fg::kOrderRandomNoRepeat, 8, 8, false, 8, 0) != p0 || seq (fg::kOrderRandomNoRepeat, 8, 8, false, 9, 0) != p0) seedDiffers = true;
            CHECK (differs && seedDiffers);
        }

        // Every order stays inside 0 .. length - 1, for many lengths, counters (negative too) and seeds, and gives the same answer every time it is asked
        bool inRange = true, repeatable = true;
        for (int order = 0; order < fg::kNumOrders; ++order)
            for (int L : { 1, 2, 3, 7, 8, 9, 16, 20, 33, 64 })
                for (long long k = -40; k < 700; k += 3)
                {
                    const int a = fg::orderStepIndex (k, L, order, order == 2, 12345, cm);
                    const int b = fg::orderStepIndex (k, L, order, order == 2, 12345, cm);
                    inRange = inRange && a >= 0 && a < L;
                    repeatable = repeatable && a == b;
                }
        CHECK (inRange && repeatable);
        CHECK (fg::orderStepIndex (3, 0, 0, false, 1, cm) == 0 && fg::orderStepIndex (3, 500, 0, false, 1, cm) == 3);     // length 0 -> 1, length 500 -> 64
        CHECK (fg::orderStepIndex (3, 8, 99, false, 1, cm) == 3 && fg::orderStepIndex (3, 8, -1, false, 1, cm) == 3);     // an unknown order plays Forward

        {   // Random: a step can repeat, the seed changes the sequence, and it uses the whole range
            const auto a = seq (fg::kOrderRandom, 8, 200, false, 7), b = seq (fg::kOrderRandom, 8, 200, false, 8);
            CHECK (a != b);
            std::vector<int> seen (8, 0); bool repeatsInARow = false;
            for (std::size_t i = 0; i < a.size(); ++i) { ++seen[(std::size_t) a[i]]; if (i > 0 && a[i] == a[i - 1]) repeatsInARow = true; }
            bool all = true; for (int i = 0; i < 8; ++i) all = all && seen[(std::size_t) i] > 5;
            CHECK (all && repeatsInARow);
        }
        {   // Drunk walk: inside a 256-step stretch each step is the same, one to the left or one to the right
            bool walk = true, moved = false;
            for (int L : { 3, 8, 20 })
                for (long long k = 1; k < 255; ++k)
                {
                    const int a = fg::orderStepIndex (k - 1, L, fg::kOrderDrunk, false, 5, cm), b = fg::orderStepIndex (k, L, fg::kOrderDrunk, false, 5, cm);
                    const int d = ((b - a) % L + L) % L;
                    walk = walk && (d == 0 || d == 1 || d == L - 1);
                    moved = moved || d != 0;
                }
            CHECK (walk && moved);
        }
        {   // Chance mix: with only Forward weighted it is Forward inside a run; with every weight 0 it falls back to Forward
            fg::ChanceMix fwd; for (int i = 0; i < 6; ++i) fwd.weight[i] = 0; fwd.weight[fg::ChanceMix::Forward] = 100;
            bool forward = true;
            for (long long k = 1; k < 255; ++k)
            {
                const int a = fg::orderStepIndex (k - 1, 8, fg::kOrderChanceMix, false, 5, fwd), b = fg::orderStepIndex (k, 8, fg::kOrderChanceMix, false, 5, fwd);
                forward = forward && b == (a + 1) % 8;
            }
            CHECK (forward);
            fg::ChanceMix none = fwd; none.weight[fg::ChanceMix::Forward] = 0;
            bool fallback = true;
            for (long long k = 1; k < 100; ++k)
            {
                const int a = fg::orderStepIndex (k - 1, 8, fg::kOrderChanceMix, false, 5, none), b = fg::orderStepIndex (k, 8, fg::kOrderChanceMix, false, 5, none);
                fallback = fallback && b == (a + 1) % 8;
            }
            CHECK (fallback);
            // a different mix gives a different path, the same mix the same path
            fg::ChanceMix rnd = fwd; rnd.weight[fg::ChanceMix::Forward] = 0; rnd.weight[fg::ChanceMix::Random] = 100; rnd.runMin = rnd.runMax = 8;
            bool differs = false;
            for (long long k = 0; k < 64; ++k)
                differs = differs || fg::orderStepIndex (k, 8, fg::kOrderChanceMix, false, 5, rnd) != fg::orderStepIndex (k, 8, fg::kOrderChanceMix, false, 5, fwd);
            CHECK (differs);
        }
        {   // Page Jump: up to eight steps it is Forward; beyond, each block of eight plays one page forward and the next block picks a page
            CHECK (seq (fg::kOrderPageJump, 8, 40) == seq (fg::kOrderForward, 8, 40));
            CHECK (seq (fg::kOrderPageJump, 5, 40) == seq (fg::kOrderForward, 5, 40));
            bool pagesOk = true; std::vector<int> pageSeen (3, 0);
            for (long long visit = 0; visit < 300; ++visit)
            {
                const int first = fg::orderStepIndex (visit * 8, 20, fg::kOrderPageJump, false, 11, cm);
                const int page = first / 8;
                pagesOk = pagesOk && first % 8 == 0 && page >= 0 && page < 3;
                if (page >= 0 && page < 3) ++pageSeen[(std::size_t) page];
                const int length = page == 2 ? 4 : 8;
                for (int o = 0; o < 8; ++o)
                    pagesOk = pagesOk && fg::orderStepIndex (visit * 8 + o, 20, fg::kOrderPageJump, false, 11, cm) == page * 8 + o % length;
            }
            CHECK (pagesOk && pageSeen[0] > 40 && pageSeen[1] > 40 && pageSeen[2] > 40);
        }
    }

    // =====================================================================================================================
    // Milestone 5: ratchet list, gate range, Mod rates and curves, Resize, converting older saved data
    {
        // the ratchet amounts are the manual's list
        CHECK (fg::kNumRatchets == 8 && fg::kRatchetMax == 7);
        const int expectAmounts[8] = { 1, 2, 3, 4, 6, 8, 16, 32 };
        for (int i = 0; i < 8; ++i) CHECK (fg::kRatchetAmounts[i] == expectAmounts[i]);
        CHECK (fg::Step().ratchet == 0 && fg::Step().gate == 50);

        // clamping: gate 5 to 100, ratchet index 0 to 7, Mod rate 0 to 15
        fg::Step s; s.gate = 0; s.ratchet = 99; s.modRate = 99; s.slew = 250; s.mod = 250; s.clamp();
        CHECK (s.gate == fg::kGateMin && fg::kGateMin == 5 && s.ratchet == 7 && s.modRate == 15 && s.slew == 100 && s.mod == 100);
        s.gate = 200; s.clamp(); CHECK (s.gate == 100);

        // the sixteen Mod rates, in the manual's order
        CHECK (fg::kNumModRates == 16);
        const struct { int kind, n, part; } rates[16] = {
            { fg::kRateOneShot, 1, 0 }, { fg::kRateOneShot, 2, 0 }, { fg::kRateRatchet, 2, 0 }, { fg::kRateOneShot, 3, 0 },
            { fg::kRateRatchet, 3, 0 }, { fg::kRateOneShot, 4, 0 }, { fg::kRateRatchet, 4, 0 }, { fg::kRateRatchet, 8, 0 },
            { fg::kRatePart, 2, 0 }, { fg::kRatePart, 2, 1 }, { fg::kRatePart, 4, 0 }, { fg::kRatePart, 4, 1 },
            { fg::kRatePart, 4, 2 }, { fg::kRatePart, 4, 3 }, { fg::kRateFixed, 1, 0 }, { fg::kRateFixed, 1, 1 } };
        for (int i = 0; i < 16; ++i)
        {
            const auto r = fg::modRateInfo (i);
            CHECK ((int) r.kind == rates[i].kind && r.n == rates[i].n && r.part == rates[i].part);
            CHECK (std::strlen (fg::kModRateCaptions[i]) > 0 && std::strlen (fg::kModRateCaptions[i]) <= 15);      // fits under a step
        }

        // the eight shapes: starts, ends, and all stay inside 0 to 1
        for (int shape = 0; shape < 8; ++shape)
            for (int k = 0; k <= 100; ++k)
            {
                const double v = fg::modShapeValue (shape, k / 100.0);
                CHECK (v >= 0.0 && v <= 1.0);
            }
        CHECK (fg::modShapeValue (0, 0.0) < 1e-9 && std::fabs (fg::modShapeValue (0, 0.5) - 1.0) < 1e-9);              // sine
        CHECK (std::fabs (fg::modShapeValue (1, 0.5) - 1.0) < 1e-9 && fg::modShapeValue (1, 0.0) == 0.0);             // triangle
        CHECK (fg::modShapeValue (2, 0.25) == 0.25 && fg::modShapeValue (3, 0.25) == 0.75);                           // ramps
        CHECK (fg::modShapeValue (4, 0.1) == 1.0 && fg::modShapeValue (4, 0.6) == 0.0);                               // square
        CHECK (fg::modShapeValue (5, 0.1) == 1.0 && fg::modShapeValue (5, 0.3) == 0.0);                               // pulse
        CHECK (fg::modShapeValue (6, 0.0) == 1.0 && fg::modShapeValue (6, 0.9) < fg::modShapeValue (6, 0.1));         // pluck
        CHECK (fg::modShapeValue (7, 0.0) == 0.0 && fg::modShapeValue (7, 0.99) == 1.0);                              // stairs

        // Shapes mode: an OFF step is 0; x2 ratchet repeats; one-shot holds the last value; halves stretch; fixed levels
        fg::Step m; m.on = true; m.modShape = 2;                                                                      // ramp up
        m.modRate = 0;  CHECK (fg::shapesModValue (m, 0.5) == 0.5);                                                   // x1
        m.modRate = 2;  CHECK (std::fabs (fg::shapesModValue (m, 0.25) - 0.5) < 1e-9 && std::fabs (fg::shapesModValue (m, 0.75) - 0.5) < 1e-9);   // x2 ratchet
        m.modRate = 1;  CHECK (std::fabs (fg::shapesModValue (m, 0.25) - 0.5) < 1e-9 && fg::shapesModValue (m, 0.9) > 0.99);                        // x2 one-shot holds
        m.modRate = 8;  CHECK (std::fabs (fg::shapesModValue (m, 1.0) - 0.5) < 1e-3 && fg::shapesModValue (m, 0.0) == 0.0);                          // 1/2 first half
        m.modRate = 9;  CHECK (std::fabs (fg::shapesModValue (m, 0.0) - 0.5) < 1e-9 && fg::shapesModValue (m, 1.0) > 0.99);                          // 1/2 second half
        m.modRate = 13; CHECK (std::fabs (fg::shapesModValue (m, 0.0) - 0.75) < 1e-9);                                                               // 1/4 fourth quarter
        m.modRate = 14; m.modShape = 0; CHECK (fg::shapesModValue (m, 0.3) == 0.0);                                    // fixed 0-2.5 V, lowest level
        m.modShape = 7; CHECK (std::fabs (fg::shapesModValue (m, 0.3) - 0.5) < 1e-9);                                  // highest level of the lower half
        m.modRate = 15; m.modShape = 0; CHECK (std::fabs (fg::shapesModValue (m, 0.3) - 0.5) < 1e-9);                   // fixed 2.5-5 V, lowest level
        m.modShape = 7; CHECK (std::fabs (fg::shapesModValue (m, 0.3) - 1.0) < 1e-9 && ! fg::shapesModMoves (m));      // top level; constant
        m.modRate = 6; CHECK (fg::shapesModMoves (m));
        m.on = false;  CHECK (fg::shapesModValue (m, 0.5) == 0.0 && ! fg::shapesModMoves (m));

        // Envelope: 0 % instant attack, 50 % equal, 100 % long attack and an instant drop
        CHECK (fg::envelopeModValue (0, 0.0) == 1.0 && std::fabs (fg::envelopeModValue (0, 0.5) - 0.5) < 1e-9 && fg::envelopeModValue (0, 1.0) == 0.0);
        CHECK (fg::envelopeModValue (50, 0.0) == 0.0 && fg::envelopeModValue (50, 0.5) == 1.0 && fg::envelopeModValue (50, 1.0) == 0.0);
        CHECK (fg::envelopeModValue (100, 0.0) == 0.0 && std::fabs (fg::envelopeModValue (100, 0.5) - 0.5) < 1e-9 && fg::envelopeModValue (100, 1.0) == 1.0);
        // Smooth and levels
        CHECK (fg::smoothModValue (0.2, 0.8, 0.0) == 0.2 && std::fabs (fg::smoothModValue (0.2, 0.8, 0.5) - 0.5) < 1e-9 && std::fabs (fg::smoothModValue (0.2, 0.8, 1.0) - 0.8) < 1e-9);
        fg::Step lv; lv.mod = 50; lv.on = true; CHECK (fg::stepLevel (lv) == 0.5); lv.on = false; CHECK (fg::stepLevel (lv) == 0.0);
        CHECK (fg::modToCc (0.0) == 0 && fg::modToCc (1.0) == 127 && fg::modToCc (0.5) == 64 && fg::modToCc (-3.0) == 0 && fg::modToCc (9.0) == 127);
        CHECK (fg::kModCc == 1 && fg::kSlewCc == 84);
    }

    {
        // Resize
        fg::StepData d;
        for (int i = 0; i < 64; ++i) { d.steps[(std::size_t) i].note = (std::uint8_t) i; d.steps[(std::size_t) i].on = true; }
        int len = 8;
        CHECK (fg::resizeShrink (d, len) && len == 4);                                                                // 8 to 4: steps 1, 3, 5, 7 kept
        CHECK (d.steps[0].note == 0 && d.steps[1].note == 2 && d.steps[2].note == 4 && d.steps[3].note == 6 && d.steps[4].note == 4);   // past the new length: left as it was
        CHECK (fg::resizeShrink (d, len) && len == 2 && d.steps[0].note == 0 && d.steps[1].note == 4);
        CHECK (fg::resizeShrink (d, len) && len == 1 && d.steps[0].note == 0);
        CHECK (! fg::resizeShrink (d, len) && len == 1);                                                              // nothing below one step

        fg::StepData e;
        for (int i = 0; i < 64; ++i) { e.steps[(std::size_t) i].note = (std::uint8_t) i; e.steps[(std::size_t) i].on = true; }
        int len2 = 7;
        CHECK (fg::resizeShrink (e, len2) && len2 == 4 && e.steps[3].note == 6);                                       // 7 to 4, rounded up

        fg::StepData x;
        for (int i = 0; i < 8; ++i) { x.steps[(std::size_t) i].note = (std::uint8_t) (10 + i); x.steps[(std::size_t) i].gate = (std::uint8_t) (20 + i); x.steps[(std::size_t) i].on = i != 3; }
        int lx = 8;
        CHECK (fg::resizeExpand (x, lx) && lx == 16);                                                                 // Spread: originals on 1, 3, 5 ..., gaps OFF
        bool spread = true;
        for (int i = 0; i < 8; ++i)
        {
            spread = spread && x.steps[(std::size_t) (2 * i)].note == 10 + i && x.steps[(std::size_t) (2 * i)].gate == 20 + i;
            spread = spread && x.steps[(std::size_t) (2 * i)].on == (i != 3) && ! x.steps[(std::size_t) (2 * i + 1)].on;
            spread = spread && x.steps[(std::size_t) (2 * i + 1)].note == 10 + i;
        }
        CHECK (spread);
        CHECK (fg::resizeExpand (x, lx) && lx == 32 && fg::resizeExpand (x, lx) && lx == 64);
        CHECK (! fg::resizeExpand (x, lx) && lx == 64);                                                               // nothing past 64
        fg::StepData y; int ly = 33;
        CHECK (! fg::resizeExpand (y, ly) && ly == 33);                                                               // doubling would pass 64: refused
        ly = 32; CHECK (fg::resizeExpand (y, ly) && ly == 64);
        // shrink then expand: the kept steps are where they were, the others are gaps
        fg::StepData z; for (int i = 0; i < 8; ++i) { z.steps[(std::size_t) i].note = (std::uint8_t) i; z.steps[(std::size_t) i].on = true; }
        int lz = 8; fg::resizeShrink (z, lz); fg::resizeExpand (z, lz);
        CHECK (lz == 8 && z.steps[0].note == 0 && z.steps[2].note == 2 && z.steps[4].note == 4 && z.steps[6].note == 6 && z.steps[0].on && ! z.steps[1].on && ! z.steps[7].on);
    }

    {
        // Saved data: version 3 round trip, and projects from 0.2.0 to 0.4.0 (ratchet as a count, eight Mod rates) are converted
        fg::StepData d = fg::StepData::makeSample();
        d.steps[5].ratchet = 7; d.steps[5].modRate = 15; d.steps[5].gate = 5;
        const auto bytes = d.toBytes();
        fg::StepData back;
        CHECK (bytes[0] == 3 && back.fromBytes (bytes.data(), bytes.size()) && back == d);

        std::vector<std::uint8_t> v2;
        v2.push_back (2); v2.push_back (8);
        const int oldRatchet[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        const int oldRate[8]    = { 0, 1, 2, 3, 4, 5, 6, 7 };
        for (int i = 0; i < 8; ++i)
        {
            v2.push_back (12); v2.push_back (0); v2.push_back (30); v2.push_back ((std::uint8_t) oldRatchet[i]);
            v2.push_back (40); v2.push_back (3); v2.push_back ((std::uint8_t) oldRate[i]); v2.push_back (1);
        }
        fg::StepData c;
        CHECK (c.fromBytes (v2.data(), v2.size()));
        const int newRatchet[8] = { 0, 1, 2, 3, 3, 4, 4, 5 };        // x1 x2 x3 x4, 5 -> x4, x6, 7 -> x6, x8
        const int newRate[8]    = { 0, 1, 5, 2, 6, 8, 3, 0 };        // x1, x2 one-shot, x4 one-shot, x2 ratchet, x4 ratchet, 1/2 first half, x3 one-shot, x1
        bool conv = true;
        for (int i = 0; i < 8; ++i)
            conv = conv && c.steps[(std::size_t) i].ratchet == newRatchet[i] && c.steps[(std::size_t) i].modRate == newRate[i]
                        && c.steps[(std::size_t) i].gate == fg::kGateMin && c.steps[(std::size_t) i].slew == 30 && c.steps[(std::size_t) i].mod == 40;
        CHECK (conv);
        v2[0] = 1;                                                       // version 1 too (MIDI notes, converted as before)
        fg::StepData c1;
        CHECK (c1.fromBytes (v2.data(), v2.size()) && c1.steps[7].ratchet == 5 && c1.steps[7].modRate == 0);
    }

    // ================= milestones 6 and 7: Mutate, Micro Mutate, Reseed, branches and Path =================
    {
        // ---- the zone arithmetic of Default Mutate ----
        CHECK (fg::mutateNoteChance (0.0) == 0.0 && fg::mutateGateChance (0.0) == 0.0);
        CHECK (std::fabs (fg::mutateNoteChance (0.25) - 0.5) < 1e-12 && fg::mutateGateChance (0.25) == 0.0);
        CHECK (std::fabs (fg::mutateNoteChance (0.5) - 1.0) < 1e-12 && fg::mutateGateChance (0.5) == 0.0);          // 100 % note at the end of zone 1
        CHECK (fg::mutateNoteChance (0.5001) == 0.75 && fg::mutateNoteChance (1.0) == 0.75);                      // drops back to a fixed 75 %
        CHECK (fg::mutateGateChance (0.5001) < 0.001 && std::fabs (fg::mutateGateChance (0.75) - 0.5) < 1e-12 && fg::mutateGateChance (1.0) == 1.0);

        // ---- Default Mutate: knob 0 draws nothing and changes nothing ----
        {
            fg::Rng rng (5), untouched (5);
            fg::Step s;
            const fg::Step before = s;
            for (int i = 0; i < 100; ++i)
                CHECK (! fg::defaultMutate (s, 0.0, 0, rng));
            CHECK (s == before && rng.state == untouched.state);
        }

        // ---- zone 1: the note changes about as often as the knob says, the gate and on/off never ----
        for (double knob : { 0.1, 0.25, 0.5 })
        {
            fg::Rng rng (11);
            int changed = 0, gateTouched = 0;
            const int trials = 20000;
            for (int i = 0; i < trials; ++i)
            {
                fg::Step s;
                s.note = 30;
                const fg::Step before = s;
                fg::defaultMutate (s, knob, 0, rng);
                if (s.note != before.note) ++changed;
                if (s.gate != before.gate || s.on != before.on) ++gateTouched;
            }
            const double expected = fg::mutateNoteChance (knob) * trials;
            CHECK (gateTouched == 0);
            CHECK (std::fabs ((double) changed - expected) < 0.03 * trials);
        }

        // ---- zone 2: the note chance is 75 %; gate changes rise to 100 %; of those 70 % change the length, 30 % toggle on / off ----
        {
            fg::Rng rng (12);
            int noteChanged = 0, gateLength = 0, toggled = 0;
            const int trials = 30000;
            for (int i = 0; i < trials; ++i)
            {
                fg::Step s;
                s.note = 30; s.gate = 50; s.on = true;
                fg::defaultMutate (s, 1.0, 0, rng);
                if (s.note != 30) ++noteChanged;
                if (s.gate != 50) ++gateLength;
                if (! s.on) ++toggled;
            }
            CHECK (std::fabs ((double) noteChanged / trials - 0.75) < 0.02);
            CHECK (std::fabs ((double) toggled / trials - 0.30) < 0.02);
            CHECK (std::fabs ((double) gateLength / trials - 0.70) < 0.02);
        }
        {
            fg::Rng rng (13);                          // halfway through zone 2 the gate chance is 50 %
            int gateTouched = 0;
            const int trials = 20000;
            for (int i = 0; i < trials; ++i)
            {
                fg::Step s; s.gate = 50; s.on = true;
                fg::defaultMutate (s, 0.75, 0, rng);
                if (s.gate != 50 || ! s.on) ++gateTouched;
            }
            CHECK (std::fabs ((double) gateTouched / trials - 0.5) < 0.03);
        }

        // ---- the size of a pitch mutation: 1, 2 or 3 scale steps (50 / 24 / 16 %), or an octave one time in five; always in range ----
        for (int scale : { 0, 16, 27 })
        {
            fg::Rng rng (21 + (std::uint64_t) scale);
            const int size = fg::scaleSize (scale);
            int small = 0, octave = 0, other = 0, total = 0;
            for (int i = 0; i < 20000; ++i)
            {
                fg::Step s;
                s.note = (std::uint8_t) fg::degreeToStored (fg::degreeCount (scale) / 2, scale);
                const int d0 = fg::storedToDegree (s.note, scale);
                CHECK (fg::mutateNote (s, scale, rng));
                CHECK (s.note <= fg::kStoredNoteMax);
                const int dist = std::abs (fg::storedToDegree (s.note, scale) - d0);
                ++total;
                if (dist >= 1 && dist <= 3 && dist != size) ++small;
                else if (dist == size) ++octave;
                else ++other;
            }
            CHECK (other == 0 || size <= 3);
            CHECK (std::fabs ((double) octave / total - 0.2) < 0.03 || size <= 3);
            CHECK (small + octave == total);
        }
        {
            fg::Rng rng (31);                          // at the very bottom and top of the range a mutation goes the other way instead
            int stuck = 0;
            for (int i = 0; i < 2000; ++i)
            {
                fg::Step lo, hi; lo.note = 0; hi.note = (std::uint8_t) fg::degreeToStored (fg::degreeCount (0) - 1, 0);
                const int hiBefore = hi.note;
                if (! fg::mutateNote (lo, 0, rng) || lo.note == 0) ++stuck;
                if (! fg::mutateNote (hi, 0, rng) || hi.note == hiBefore) ++stuck;
            }
            CHECK (stuck == 0);
        }

        // ---- Micro Mutate ----
        {
            fg::Rng rng (41), untouched (41);
            fg::Step s;
            const fg::Step before = s;
            for (int i = 0; i < 50; ++i)
                CHECK (! fg::microMutate (s, 0.0, 0.0, 0.0, 0, rng));
            CHECK (s == before && rng.state == untouched.state);
        }
        {
            // each knob changes only its own thing, and 100 % changes (nearly) every step
            const int trials = 5000;
            int ratchetChanged = 0, slewChanged = 0, shapeChanged = 0, modChanged = 0;
            fg::Rng rng (42);
            for (int i = 0; i < trials; ++i)
            {
                fg::Step s; s.ratchet = 0; s.slew = 77; s.mod = 33; s.modShape = 2;
                fg::Step t = s;
                fg::microMutate (t, 1.0, 0.0, 0.0, 0, rng);
                CHECK (t.slew == s.slew && t.mod == s.mod && t.modShape == s.modShape && t.note == s.note && t.gate == s.gate);
                if (t.ratchet != s.ratchet) ++ratchetChanged;
                CHECK (t.ratchet <= fg::kRatchetMax);
                t = s; fg::microMutate (t, 0.0, 1.0, 0.0, 0, rng);
                CHECK (t.ratchet == s.ratchet && t.mod == s.mod && t.modShape == s.modShape);
                if (t.slew != s.slew) ++slewChanged;
                CHECK (t.slew <= fg::kSlewMax);
                t = s; fg::microMutate (t, 0.0, 0.0, 1.0, 0, rng);                    // Shapes mode: a new shape, the value is left alone
                CHECK (t.mod == s.mod && t.ratchet == s.ratchet && t.slew == s.slew);
                if (t.modShape != s.modShape) ++shapeChanged;
                t = s; fg::microMutate (t, 0.0, 0.0, 1.0, 2, rng);                    // Envelope mode: a new value, the shape is left alone
                CHECK (t.modShape == s.modShape);
                CHECK (t.mod <= fg::kModMax);
                if (t.mod != s.mod) ++modChanged;
            }
            CHECK (ratchetChanged > trials * 6 / 10);          // x1 is picked 30 % of the time, and then nothing changes
            CHECK (slewChanged > trials * 9 / 10);
            CHECK (shapeChanged > trials * 8 / 10);
            CHECK (modChanged > trials * 9 / 10);
        }
        {
            // half the knob, half the steps (the chance is the knob)
            fg::Rng rng (43);
            int changed = 0;
            const int trials = 20000;
            for (int i = 0; i < trials; ++i)
            {
                fg::Step s; s.slew = 77;
                fg::microMutate (s, 0.0, 0.5, 0.0, 0, rng);
                if (s.slew != 77) ++changed;
            }
            CHECK (std::fabs ((double) changed / trials - 0.5) < 0.03);
        }
        {
            // the ratchet draw: every amount can come up, x1 most often, x32 least
            fg::Rng rng (44);
            int counts[fg::kNumRatchets] = {};
            for (int i = 0; i < 40000; ++i) ++counts[fg::randomRatchet (rng)];
            for (int i = 0; i < fg::kNumRatchets; ++i) CHECK (counts[i] > 0);
            CHECK (counts[0] > counts[1] && counts[1] > counts[4] && counts[4] > counts[7]);
        }

        // ---- Reseed ----
        {
            const fg::StepData base = fg::StepData::makeSample();
            // Note: only the note field changes, every note is a real degree of the scale, the whole 5-octave range turns up
            for (int scale : { 0, 16, 27 })
            {
                fg::StepData d = base;
                fg::Rng rng (51);
                bool onlyNote = true;
                std::set<int> octaves;
                for (int round = 0; round < 40; ++round)
                {
                    fg::reseedSteps (d, 0, 64, fg::ReseedKind::Note, scale, rng);
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto& s = d.steps[(std::size_t) i];
                        onlyNote = onlyNote && s.gate == base.steps[(std::size_t) i].gate && s.on == base.steps[(std::size_t) i].on
                                            && s.slew == base.steps[(std::size_t) i].slew && s.ratchet == base.steps[(std::size_t) i].ratchet
                                            && s.mod == base.steps[(std::size_t) i].mod;
                        CHECK (s.note <= fg::kStoredNoteMax && (s.note % fg::kDegreeSlots) < fg::scaleSize (scale));
                        octaves.insert (s.note / fg::kDegreeSlots);
                    }
                }
                CHECK (onlyNote);
                CHECK ((int) octaves.size() == fg::kNumOctaves);
            }
            // Gate: about 20 % off, lengths from the list, nothing else changes
            {
                fg::StepData d = base; fg::Rng rng (52);
                int off = 0, total = 0; bool ok = true;
                for (int round = 0; round < 200; ++round)
                {
                    fg::reseedSteps (d, 0, 64, fg::ReseedKind::Gate, 0, rng);
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto& s = d.steps[(std::size_t) i];
                        ++total; if (! s.on) ++off;
                        ok = ok && s.note == base.steps[(std::size_t) i].note && s.slew == base.steps[(std::size_t) i].slew
                                && (s.gate == 10 || s.gate == 25 || s.gate == 40 || s.gate == 50 || s.gate == 60 || s.gate == 75 || s.gate == 90 || s.gate == 100);
                    }
                }
                CHECK (ok);
                CHECK (std::fabs ((double) off / total - 0.2) < 0.02);
            }
            // Slew: 70 % none, otherwise 10 to 100
            {
                fg::StepData d = base; fg::Rng rng (53);
                int zero = 0, total = 0; bool ok = true;
                for (int round = 0; round < 200; ++round)
                {
                    fg::reseedSteps (d, 0, 64, fg::ReseedKind::Slew, 0, rng);
                    for (const auto& s : d.steps) { ++total; if (s.slew == 0) ++zero; else ok = ok && s.slew >= 10 && s.slew <= 100; }
                }
                CHECK (ok);
                CHECK (std::fabs ((double) zero / total - 0.7) < 0.02);
            }
            // Ratchet: 75 % x1, every other amount appears
            {
                fg::StepData d = base; fg::Rng rng (54);
                int one = 0, total = 0; std::set<int> seen;
                for (int round = 0; round < 200; ++round)
                {
                    fg::reseedSteps (d, 0, 64, fg::ReseedKind::Ratchet, 0, rng);
                    for (const auto& s : d.steps) { ++total; if (s.ratchet == 0) ++one; seen.insert (s.ratchet); }
                }
                CHECK (std::fabs ((double) one / total - 0.75) < 0.02);
                CHECK ((int) seen.size() == fg::kNumRatchets);
            }
            // Mod: value, shape and rate; all 16 rates and 8 shapes turn up
            {
                fg::StepData d = base; fg::Rng rng (55);
                std::set<int> shapes, rates; int rateOne = 0, total = 0; bool ok = true;
                for (int round = 0; round < 200; ++round)
                {
                    fg::reseedSteps (d, 0, 64, fg::ReseedKind::Mod, 0, rng);
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto& s = d.steps[(std::size_t) i];
                        shapes.insert (s.modShape); rates.insert (s.modRate); ++total; if (s.modRate == 0) ++rateOne;
                        ok = ok && s.mod <= 100 && s.note == base.steps[(std::size_t) i].note && s.gate == base.steps[(std::size_t) i].gate;
                    }
                }
                CHECK (ok && (int) shapes.size() == 8 && (int) rates.size() == 16);
                CHECK (std::fabs ((double) rateOne / total - 0.6) < 0.02);
            }
            // All: every field is reseeded; a Page touches only its eight steps; the same seed gives the same result
            {
                fg::StepData a = base, b = base, pageOnly = base;
                fg::Rng r1 (56), r2 (56), r3 (56);
                fg::reseedSteps (a, 0, 64, fg::ReseedKind::All, 0, r1);
                fg::reseedSteps (b, 0, 64, fg::ReseedKind::All, 0, r2);
                CHECK (a == b);
                CHECK (! (a == base));
                fg::reseedSteps (pageOnly, 16, 8, fg::ReseedKind::All, 0, r3);
                bool inside = true, outside = true;
                for (int i = 0; i < 64; ++i)
                {
                    const bool same = pageOnly.steps[(std::size_t) i] == base.steps[(std::size_t) i];
                    if (i >= 16 && i < 24) inside = inside && ! same; else outside = outside && same;
                }
                CHECK (outside);
                CHECK (inside || std::count_if (pageOnly.steps.begin() + 16, pageOnly.steps.begin() + 24, [&] (const fg::Step& s) { return ! (s == base.steps[16]); }) > 0);
                fg::StepData edge = base; fg::Rng r4 (57);
                fg::reseedSteps (edge, 60, 20, fg::ReseedKind::All, 0, r4);          // cut off at 64, never past the end
                fg::reseedSteps (edge, 70, 5, fg::ReseedKind::All, 0, r4);           // wholly outside: nothing
                bool head = true;
                for (int i = 0; i < 60; ++i) head = head && edge.steps[(std::size_t) i] == base.steps[(std::size_t) i];
                CHECK (head);
            }
        }

        // ---- Branch and Path ----
        for (int n = 0; n <= 7; ++n)
        {
            CHECK (fg::branchCountFromKnob ((double) n / 7.0) == n);
            CHECK (fg::pathCountFor (n) == (1 << n));
            CHECK (fg::pathFromKnob (0.0, n) == 0 && fg::pathFromKnob (1.0, n) == (1 << n) - 1);
        }
        CHECK (fg::branchCountFromKnob (0.0) == 0 && fg::branchCountFromKnob (1.0) == 7 && fg::pathCountFor (7) == 128);
        CHECK (fg::pathFromKnob (0.5, 7) == 64 || fg::pathFromKnob (0.5, 7) == 63);

        {
            // siblings at a level always get different changes; every kind turns up; the same seed and path always agree
            std::set<int> kindsSeen;
            bool siblingsDiffer = true, repeats = true;
            for (std::uint32_t seed = 1; seed <= 200; ++seed)
                for (int n = 1; n <= 7; ++n)
                    for (int path = 0; path < (1 << n); path += (n > 4 ? 7 : 1))
                        for (int level = 1; level <= n; ++level)
                        {
                            const int k = fg::branchKindFor (level, path, n, seed);
                            kindsSeen.insert (k);
                            repeats = repeats && k == fg::branchKindFor (level, path, n, seed) && k >= 0 && k < fg::kNumBranchKinds;
                            const int sibling = path ^ (1 << (n - level));            // the other variant at this level (same earlier bits)
                            if (sibling < (1 << n)) siblingsDiffer = siblingsDiffer && k != fg::branchKindFor (level, sibling, n, seed);
                        }
            CHECK (siblingsDiffer && repeats && (int) kindsSeen.size() == fg::kNumBranchKinds);
        }

        {
            // the same inputs always give the same branches; paths that share their first bits share those branches; the trunk is never touched
            const fg::StepData trunk = fg::StepData::makeSample();
            for (int scale : { 0, 16, 27 })
            {
                fg::BranchSet a, b, c;
                const std::uint32_t seed = 987;
                fg::buildBranches (a, trunk, 8, scale, 7, 0b1011010, seed);
                fg::buildBranches (b, trunk, 8, scale, 7, 0b1011010, seed);
                CHECK (a.count == 7);
                bool same = true;
                for (int i = 0; i < 7; ++i) for (int j = 0; j < 64; ++j) same = same && a.steps[(std::size_t) i][(std::size_t) j] == b.steps[(std::size_t) i][(std::size_t) j];
                CHECK (same);
                fg::buildBranches (c, trunk, 8, scale, 7, 0b1011111, seed);            // first four bits the same, the last three different
                bool sharedHead = true, tailDiffers = false;
                for (int i = 0; i < 4; ++i) for (int j = 0; j < 64; ++j) sharedHead = sharedHead && a.steps[(std::size_t) i][(std::size_t) j] == c.steps[(std::size_t) i][(std::size_t) j];
                for (int i = 4; i < 7; ++i) for (int j = 0; j < 8; ++j) tailDiffers = tailDiffers || ! (a.steps[(std::size_t) i][(std::size_t) j] == c.steps[(std::size_t) i][(std::size_t) j]);
                CHECK (sharedHead);
                CHECK (tailDiffers);
                bool kindsMatch = true;
                for (int i = 0; i < 7; ++i) kindsMatch = kindsMatch && a.kind[(std::size_t) i] == fg::branchKindFor (i + 1, 0b1011010, 7, seed);
                CHECK (kindsMatch);
                // a different seed gives different branches
                fg::BranchSet d;
                fg::buildBranches (d, trunk, 8, scale, 7, 0b1011010, seed + 1);
                bool anyDifferent = false;
                for (int i = 0; i < 7; ++i) for (int j = 0; j < 8; ++j) anyDifferent = anyDifferent || ! (a.steps[(std::size_t) i][(std::size_t) j] == d.steps[(std::size_t) i][(std::size_t) j]);
                CHECK (anyDifferent);
            }
        }

        {
            // what each change does, checked branch by branch over many seeds
            const fg::StepData trunk = fg::StepData::makeSample();
            int seen[fg::kNumBranchKinds] = {};
            for (int scale : { 0, 16 })
                for (std::uint32_t seed = 1; seed <= 300; ++seed)
                {
                    const int L = 8 + (int) (seed % 9);
                    fg::BranchSet set;
                    fg::buildBranches (set, trunk, L, scale, 3, (int) (seed % 8), seed);
                    const int pivot = fg::storedToDegree (trunk.steps[0].note, scale);
                    for (int b = 0; b < 3; ++b)
                    {
                        const auto& prev = b == 0 ? trunk.steps : set.steps[(std::size_t) (b - 1)];
                        const auto& cur = set.steps[(std::size_t) b];
                        ++seen[set.kind[(std::size_t) b]];
                        bool ok = true;
                        switch (set.kind[(std::size_t) b])
                        {
                            case fg::kBranchReverse:
                                for (int i = 0; i < L; ++i) ok = ok && cur[(std::size_t) i] == prev[(std::size_t) (L - 1 - i)];
                                break;
                            case fg::kBranchInverse:
                                for (int i = 0; i < L; ++i)
                                    ok = ok && cur[(std::size_t) i].note == fg::degreeToStored (2 * pivot - fg::storedToDegree (prev[(std::size_t) i].note, scale), scale)
                                            && cur[(std::size_t) i].gate == prev[(std::size_t) i].gate && cur[(std::size_t) i].on == prev[(std::size_t) i].on;
                                break;
                            case fg::kBranchTranspose:
                            {
                                // the amount is the largest rise seen (a step at the top of the range is held there)
                                int up = 0;
                                for (int i = 0; i < L; ++i)
                                    up = std::max (up, fg::storedToDegree (cur[(std::size_t) i].note, scale) - fg::storedToDegree (prev[(std::size_t) i].note, scale));
                                ok = up >= 1 && up <= fg::scaleSize (scale);
                                for (int i = 0; i < L; ++i)
                                    ok = ok && cur[(std::size_t) i].note == fg::degreeToStored (fg::storedToDegree (prev[(std::size_t) i].note, scale) + up, scale);
                                break;
                            }
                            case fg::kBranchMutate:
                            {
                                int differs = 0;
                                for (int i = 0; i < L; ++i)
                                {
                                    differs += ! (cur[(std::size_t) i] == prev[(std::size_t) i]);
                                    ok = ok && cur[(std::size_t) i].slew == prev[(std::size_t) i].slew && cur[(std::size_t) i].ratchet == prev[(std::size_t) i].ratchet
                                            && cur[(std::size_t) i].mod == prev[(std::size_t) i].mod;
                                }
                                ok = ok && differs >= 1 - (L < 3 ? 1 : 0);
                                break;
                            }
                            default:
                                for (int i = 0; i < L; ++i)
                                    ok = ok && cur[(std::size_t) i].note <= fg::kStoredNoteMax && cur[(std::size_t) i].slew == prev[(std::size_t) i].slew
                                            && cur[(std::size_t) i].ratchet == prev[(std::size_t) i].ratchet && cur[(std::size_t) i].mod == prev[(std::size_t) i].mod;
                                break;
                        }
                        // steps past LENGTH carry the trunk's data
                        for (int i = L; i < 64; ++i) ok = ok && cur[(std::size_t) i] == trunk.steps[(std::size_t) i];
                        CHECK (ok);
                    }
                }
            for (int k = 0; k < fg::kNumBranchKinds; ++k) CHECK (seen[k] > 20);
            CHECK (fg::kBranchLetters[0] == 'R' && fg::kBranchLetters[1] == 'I' && fg::kBranchLetters[2] == 'T' && fg::kBranchLetters[3] == 'M' && fg::kBranchLetters[4] == '?');
        }
    }

    std::printf ("engine tests: %d checks passed\n", checks - failures);
    if (failures > 0)
    {
        std::printf ("engine tests: %d FAILED of %d checks\n", failures, checks);
        return 1;
    }
    return 0;
}
