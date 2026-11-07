// Engine self-test (milestone 2): checks the 46 controls against the approved control list, written out here a second time
// (id, kind, range, default) so a slip in Params.h is caught, then the menus, the step data and its saved form.
// Prints "engine tests: N checks passed" and exits 0, or lists every failure and exits 1.

#include <fg/Params.h>
#include <fg/Steps.h>

#include <cstdio>
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
    CHECK (sample.steps[0].note == 74 && sample.steps[1].note == 81 && sample.steps[15].note == 79);       // picture A
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

    std::printf ("engine tests: %d checks passed\n", checks - failures);
    if (failures > 0)
    {
        std::printf ("engine tests: %d FAILED of %d checks\n", failures, checks);
        return 1;
    }
    return 0;
}
