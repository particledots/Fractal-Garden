#include <fg/Params.h>

#include <cstdio>
#include <cstring>

namespace fg
{

const ParamSpec* findParam (const char* paramId)
{
    if (paramId == nullptr)
        return nullptr;
    for (const auto& p : kParams)
        if (std::strcmp (p.id, paramId) == 0)
            return &p;
    return nullptr;
}

void noteName (int midiNote, char* out, std::size_t outSize)
{
    if (out == nullptr || outSize == 0)
        return;
    const int n = midiNote < 0 ? 0 : (midiNote > 127 ? 127 : midiNote);
    std::snprintf (out, outSize, "%s%d", kKeyNames[n % 12], n / 12 - 2);     // Logic's naming: 60 = C3
}

} // namespace fg
