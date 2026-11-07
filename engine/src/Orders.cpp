#include <fg/Orders.h>

#include <algorithm>

namespace fg
{

namespace
{
    long long posMod (long long a, long long m) { return ((a % m) + m) % m; }
    long long floorDivLL (long long a, long long m) { return (a - posMod (a, m)) / m; }

    // splitmix64 finaliser: a cheap, well-mixed 64-bit hash (from Dotriaconta-tone)
    std::uint64_t mix64 (std::uint64_t x)
    {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

    // Uniform [0, 1) from (key, stream, step counter). Stateless, so the result never depends on block size.
    double unitRandom (std::uint64_t key, std::uint64_t stream, long long k)
    {
        const std::uint64_t h = mix64 (mix64 (key ^ (stream * 0xd1342543de82ef95ULL)) ^ (std::uint64_t) k);
        return (double) (h >> 11) * (1.0 / 9007199254740992.0);      // 53 bits
    }

    // A small integer hash (a "lowbias32"-style mixer) used by Drunk walk, Random no-repeat and Chance mix (from Dotriaconta-tone)
    std::uint32_t mix32 (std::uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }
    std::uint32_t hash4 (std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d)
    {
        std::uint32_t h = mix32 (a + 0x9E3779B9U);
        h = mix32 (h ^ (b + 0x85EBCA6BU));
        h = mix32 (h ^ (c + 0xC2B2AE35U));
        return mix32 (h ^ (d + 0x27D4EB2FU));
    }

    constexpr long long kEpoch = 256;       // Drunk walk and Chance mix restart from a fresh position every 256 steps, so any step can be found quickly

    // Converge, one pass: the outermost steps first, jumping back and forth towards the middle: 1 8 2 7 3 6 4 5 for eight steps.
    int convergeAt (int m, int L)
    {
        return (m % 2 == 0) ? m / 2 : L - 1 - m / 2;
    }

    int newOrderStep (long long n, int L, int order, std::uint32_t seed, const ChanceMix& chance)
    {
        if (n < 0)
            n = 0;
        switch (order)
        {
            case kOrderRandomNoRepeat:     // every step once, in a new order each time round
            {
                int perm[kNumSteps];
                for (int i = 0; i < L; ++i)
                    perm[i] = i;
                const std::uint32_t cycleLo = (std::uint32_t) ((n / L) & 0xffffffffLL);
                for (int i = L - 1; i > 0; --i)
                {
                    const int j = (int) (hash4 (seed, cycleLo, (std::uint32_t) i, 2) % (std::uint32_t) (i + 1));
                    std::swap (perm[i], perm[j]);
                }
                return perm[n % L];
            }
            case kOrderDrunk:              // each step moves one to the left (40 %), one to the right (40 %) or stays (20 %), round the loop
            {
                const std::uint32_t epoch = (std::uint32_t) ((n / kEpoch) & 0xffffffffLL);
                const int m = (int) (n % kEpoch);
                int c = (int) (hash4 (seed, epoch, 0, 3) % (std::uint32_t) L);
                for (int i = 1; i <= m; ++i)
                {
                    const std::uint32_t r = hash4 (seed, epoch, (std::uint32_t) i, 4) % 10;
                    c += r < 4 ? -1 : (r < 8 ? 1 : 0);
                    c = ((c % L) + L) % L;
                }
                return c;
            }
            case kOrderChanceMix:          // runs of random length, each in an order picked by the weights
            {
                const std::uint32_t epoch = (std::uint32_t) ((n / kEpoch) & 0xffffffffLL);
                const int m = (int) (n % kEpoch);
                auto draw = [&] (std::uint32_t k) { return hash4 (seed, epoch, k, 5); };
                const int lo = std::max (1, std::min (chance.runMin, chance.runMax)), hi = std::max (chance.runMin, chance.runMax);
                std::uint32_t k = 0;
                int c = (int) (draw (k++) % (std::uint32_t) L), pdir = 1, mode = -1, remaining = 0;
                for (int i = 0; i <= m; ++i)
                {
                    if (remaining <= 0)
                    {
                        // pick the next order; the one just finished is left out unless it is the only one with a weight
                        int total = 0, others = 0;
                        for (int q = 0; q < ChanceMix::kNumModes; ++q)
                        {
                            total += chance.weight[q];
                            if (q != mode)
                                others += chance.weight[q];
                        }
                        const int usable = others > 0 ? others : total;
                        int pick = ChanceMix::Forward;
                        if (usable > 0)
                        {
                            int r = (int) (draw (k++) % (std::uint32_t) usable);
                            for (int q = 0; q < ChanceMix::kNumModes; ++q)
                            {
                                const int w = (others > 0 && q == mode) ? 0 : chance.weight[q];
                                if (r < w) { pick = q; break; }
                                r -= w;
                            }
                        }
                        mode = pick;
                        remaining = lo + (hi > lo ? (int) (draw (k++) % (std::uint32_t) (hi - lo + 1)) : 0);
                    }
                    if (i > 0)
                    {
                        switch (mode)
                        {
                            case ChanceMix::Backward: c = (c + L - 1) % L; break;
                            case ChanceMix::Pendulum:
                                if (L > 1)
                                {
                                    c += pdir;
                                    if (c >= L) { c = L - 2; pdir = -1; }
                                    else if (c < 0) { c = 1; pdir = 1; }
                                }
                                break;
                            case ChanceMix::PingPong:
                                if (c + pdir >= L || c + pdir < 0) pdir = -pdir; else c += pdir;
                                break;
                            case ChanceMix::Random: c = (int) (draw (k++) % (std::uint32_t) L); break;
                            case ChanceMix::Drunk:
                            {
                                const std::uint32_t r = draw (k++) % 10;
                                c += r < 4 ? -1 : (r < 8 ? 1 : 0);
                                c = ((c % L) + L) % L;
                                break;
                            }
                            default: c = (c + 1) % L; break;
                        }
                    }
                    --remaining;
                }
                return c;
            }
            default:
                return (int) (n % L);
        }
    }
}

int orderStepIndex (long long k, int length, int order, bool pendRepeat, std::uint32_t seed, const ChanceMix& chance)
{
    const int L = std::clamp (length, 1, kNumSteps);
    if (L <= 1)
        return 0;

    switch (order)
    {
        case kOrderReverse:
            return L - 1 - (int) posMod (k, L);

        case kOrderPendulum:           // 1-2-3-2-1-2-3...  or, with the ends repeated, 1-2-3-3-2-1-1-2-3...
        {
            const long long period = pendRepeat ? 2LL * L : 2LL * L - 2;
            const long long n = posMod (k, period);
            if (n < L)
                return (int) n;
            return (int) (pendRepeat ? 2LL * L - 1 - n : 2LL * L - 2 - n);
        }

        case kOrderRandom:             // a step can come up twice in a row
            return std::min (L - 1, (int) (unitRandom (mix64 ((std::uint64_t) seed + 1), 1, k) * L));

        case kOrderConverge:
            return convergeAt ((int) posMod (k, L), L);

        case kOrderDiverge:            // the Converge pass played backwards: from the middle out, jumping back and forth
            return convergeAt (L - 1 - (int) posMod (k, L), L);

        case kOrderConvergeDiverge:    // one Converge pass, then one Diverge pass, and so on
        {
            const int m = (int) posMod (k, L);
            return (floorDivLL (k, L) % 2 == 0) ? convergeAt (m, L) : convergeAt (L - 1 - m, L);
        }

        case kOrderPageJump:           // pages of eight steps: play a page forward, then jump to the start of a randomly chosen page
        {
            const int pages = (L + 7) / 8;
            if (pages <= 1)
                return (int) posMod (k, L);
            const long long visit = floorDivLL (k, 8);
            const int offset = (int) posMod (k, 8);
            const int page = (int) (hash4 (seed, (std::uint32_t) (visit & 0xffffffffLL), 0, 7) % (std::uint32_t) pages);
            const int start = page * 8;
            const int pageLength = std::min (8, L - start);        // the last page can be shorter; it then plays round within its 8 slots
            return start + offset % pageLength;
        }

        case kOrderDrunk:
        case kOrderRandomNoRepeat:
        case kOrderChanceMix:
            return newOrderStep (k, L, order, seed, chance);

        default:                       // Forward
            return (int) posMod (k, L);
    }
}

} // namespace fg
