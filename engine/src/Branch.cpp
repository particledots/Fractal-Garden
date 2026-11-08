#include <fg/Branch.h>

#include <algorithm>
#include <cmath>

namespace fg
{

namespace
{
    std::uint64_t mix64 (std::uint64_t x)
    {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

    std::uint64_t nodeHash (std::uint32_t seed, int level, int prefix, std::uint64_t salt)
    {
        const std::uint64_t node = ((std::uint64_t) 1 << level) | (std::uint64_t) prefix;     // a unique number for every node of the tree
        return mix64 (mix64 ((std::uint64_t) seed * 0x2545f4914f6cdd1dULL + node) ^ (salt * 0xd1342543de82ef95ULL));
    }

    // The path bits of the first `level` levels (first level is the most significant bit).
    int prefixOf (int path, int level, int branches)
    {
        return path >> (branches - level);
    }

    int sanitiseBranches (int branches) { return std::clamp (branches, 0, kMaxBranches); }
    int sanitisePath (int path, int branches) { return std::clamp (path, 0, pathCountFor (branches) - 1); }

    constexpr int kGates[9] = { 10, 25, 40, 50, 50, 60, 75, 90, 100 };
}

int branchCountFromKnob (double knob)
{
    return std::clamp ((int) std::lround (knob * (double) kMaxBranches), 0, kMaxBranches);
}

int pathCountFor (int branches)
{
    return 1 << sanitiseBranches (branches);
}

int pathFromKnob (double knob, int branches)
{
    const int paths = pathCountFor (branches);
    return std::clamp ((int) std::lround (knob * (double) (paths - 1)), 0, paths - 1);
}

int branchKindFor (int level, int path, int branches, std::uint32_t seed)
{
    const int n = sanitiseBranches (branches);
    if (level < 1 || level > n)
        return kBranchReverse;
    const int p = sanitisePath (path, n);
    const int prefix = prefixOf (p, level, n);
    const int sibling0 = prefix & ~1;                       // the two variants at this level share everything but the last bit
    const int base = (int) (nodeHash (seed, level, sibling0, 1) % (std::uint64_t) kNumBranchKinds);
    if ((prefix & 1) == 0)
        return base;
    const int offset = 1 + (int) (nodeHash (seed, level, sibling0, 2) % (std::uint64_t) (kNumBranchKinds - 1));
    return (base + offset) % kNumBranchKinds;                // never the same as the other variant
}

void buildBranches (BranchSet& set, const StepData& source, int length, int scale, int branches, int path, std::uint32_t seed, int rangeOctaves)
{
    const int n = sanitiseBranches (branches);
    const int L = std::clamp (length, 1, kNumSteps);
    const int p = sanitisePath (path, n);
    set.count = n;
    const int pivot = storedToDegree (source.steps[0].note, scale);        // Inverse mirrors around the trunk's first note
    const int size = scaleSize (scale);

    for (int level = 1; level <= n; ++level)
    {
        auto& out = set.steps[(std::size_t) (level - 1)];
        // Steps past LENGTH never play; they just carry the trunk's data so every array holds valid steps.
        for (int i = 0; i < kNumSteps; ++i)
            out[(std::size_t) i] = source.steps[(std::size_t) i];

        auto prev = [&] (int i) -> const Step&
        {
            return level == 1 ? source.steps[(std::size_t) i] : set.steps[(std::size_t) (level - 2)][(std::size_t) i];
        };

        const int prefix = prefixOf (p, level, n);
        const int kind = branchKindFor (level, p, n, seed);
        set.kind[(std::size_t) (level - 1)] = kind;
        Rng rng (nodeHash (seed, level, prefix, 3));

        switch (kind)
        {
            case kBranchReverse:
                for (int i = 0; i < L; ++i)
                    out[(std::size_t) i] = prev (L - 1 - i);
                break;

            case kBranchInverse:
                for (int i = 0; i < L; ++i)
                {
                    Step s = prev (i);
                    s.note = (std::uint8_t) degreeToStored (2 * pivot - storedToDegree (s.note, scale), scale);
                    s.fine = (std::int16_t) -s.fine;
                    out[(std::size_t) i] = s;
                }
                break;

            case kBranchTranspose:
            {
                const int up = 1 + rng.below (size);                       // 1 scale step up to a whole octave
                for (int i = 0; i < L; ++i)
                {
                    Step s = prev (i);
                    s.note = (std::uint8_t) degreeToStored (storedToDegree (s.note, scale) + up, scale);
                    out[(std::size_t) i] = s;
                }
                break;
            }

            case kBranchMutate:
                for (int i = 0; i < L; ++i)
                {
                    Step s = prev (i);
                    if (rng.unit() < 0.6)
                        mutateNote (s, scale, rng, rangeOctaves);
                    if (rng.unit() < 0.25)
                        mutateGate (s, rng);
                    out[(std::size_t) i] = s;
                }
                break;

            default:        // Randomize
                for (int i = 0; i < L; ++i)
                {
                    Step s = prev (i);
                    s.note = (std::uint8_t) degreeToStored (rng.below (rangeTop (scale, rangeOctaves) + 1), scale);
                    s.fine = 0;
                    s.on = rng.unit() >= 0.2;
                    s.gate = (std::uint8_t) kGates[rng.below (9)];
                    out[(std::size_t) i] = s;
                }
                break;
        }
        for (int i = 0; i < L; ++i)
            out[(std::size_t) i].clamp();
    }
}

} // namespace fg
