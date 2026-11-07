// Engine self-test (milestones 2 and 4): checks the 46 controls against the approved control list, written out here a second time
// (id, kind, range, default) so a slip in Params.h is caught, then the menus, the step data and its saved form, and (milestone 4) the
// 28 scales, the pitch a stored note plays, the 11 play orders, Rotate and Transpose.
// Prints "engine tests: N checks passed" and exits 0, or lists every failure and exits 1.

#include <fg/Orders.h>
#include <fg/Params.h>
#include <fg/Pitch.h>
#include <fg/Steps.h>

#include <algorithm>
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
        CHECK (s.ratchet >= 1 && s.ratchet <= fg::kRatchetMax);
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
        CHECK (v2[0] == 2 && v2.size() == v1.size());                    // saved again in the new format
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

    std::printf ("engine tests: %d checks passed\n", checks - failures);
    if (failures > 0)
    {
        std::printf ("engine tests: %d FAILED of %d checks\n", failures, checks);
        return 1;
    }
    return 0;
}
