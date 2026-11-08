#include <fg/Patterns.h>

#include <algorithm>

namespace fg
{

namespace { constexpr std::uint8_t kBankVersion = 1; }

bool Pattern::operator== (const Pattern& o) const
{
    return used == o.used && length == o.length && steps == o.steps;
}

bool PatternBank::operator== (const PatternBank& o) const
{
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (! (slots[i] == o.slots[i]))
            return false;
    return true;
}

bool PatternBank::isUsed (int slot) const
{
    return slot >= 0 && slot < kNumPatternSlots && slots[(std::size_t) slot].used;
}

int PatternBank::usedCount() const
{
    int n = 0;
    for (const auto& s : slots)
        n += s.used ? 1 : 0;
    return n;
}

bool PatternBank::save (int slot, const StepData& steps, int length)
{
    if (slot < 0 || slot >= kNumPatternSlots)
        return false;
    auto& p = slots[(std::size_t) slot];
    p.used = true;
    p.length = std::clamp (length, 1, kNumSteps);
    p.steps = steps;
    return true;
}

bool PatternBank::load (int slot, StepData& steps, int& length) const
{
    if (! isUsed (slot))
        return false;
    const auto& p = slots[(std::size_t) slot];
    steps = p.steps;
    length = p.length;
    return true;
}

std::vector<std::uint8_t> PatternBank::toBytes() const
{
    std::vector<std::uint8_t> out;
    out.push_back (kBankVersion);
    for (const auto& p : slots)
    {
        out.push_back (p.used ? 1 : 0);
        if (! p.used)
            continue;
        const auto stepBytes = p.steps.toBytes();
        out.push_back ((std::uint8_t) std::clamp (p.length, 1, kNumSteps));
        out.push_back ((std::uint8_t) (stepBytes.size() & 0xff));
        out.push_back ((std::uint8_t) ((stepBytes.size() >> 8) & 0xff));
        out.insert (out.end(), stepBytes.begin(), stepBytes.end());
    }
    return out;
}

bool PatternBank::fromBytes (const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 1 || data[0] != kBankVersion)
        return false;
    PatternBank loaded;
    std::size_t pos = 1;
    for (auto& p : loaded.slots)
    {
        if (pos >= size)
            return false;
        const std::uint8_t used = data[pos++];
        if (used > 1)
            return false;
        if (used == 0)
            continue;
        if (pos + 3 > size)
            return false;
        const int length = data[pos++];
        const std::size_t n = (std::size_t) data[pos] | ((std::size_t) data[pos + 1] << 8);
        pos += 2;
        if (length < 1 || length > kNumSteps || n == 0 || pos + n > size)
            return false;
        StepData steps;
        if (! steps.fromBytes (data + pos, n))
            return false;
        pos += n;
        p.used = true;
        p.length = length;
        p.steps = steps;
    }
    if (pos != size)
        return false;
    *this = loaded;
    return true;
}

//==============================================================================
// The shared pattern
std::vector<std::uint8_t> sharedPatternToBytes (const StepData& steps, int length)
{
    std::vector<std::uint8_t> out { 'F', 'G', 'S', 'P', 1 };
    const auto stepBytes = steps.toBytes();
    out.push_back ((std::uint8_t) std::clamp (length, 1, kNumSteps));
    out.push_back ((std::uint8_t) (stepBytes.size() & 0xff));
    out.push_back ((std::uint8_t) ((stepBytes.size() >> 8) & 0xff));
    out.insert (out.end(), stepBytes.begin(), stepBytes.end());
    return out;
}

bool sharedPatternFromBytes (const std::uint8_t* data, std::size_t size, StepData& steps, int& length)
{
    if (data == nullptr || size < 8 || data[0] != 'F' || data[1] != 'G' || data[2] != 'S' || data[3] != 'P' || data[4] != 1)
        return false;
    const int len = data[5];
    const std::size_t n = (std::size_t) data[6] | ((std::size_t) data[7] << 8);
    if (len < 1 || len > kNumSteps || n == 0 || 8 + n != size)
        return false;
    StepData loaded;
    if (! loaded.fromBytes (data + 8, n))
        return false;
    steps = loaded;
    length = len;
    return true;
}

} // namespace fg
