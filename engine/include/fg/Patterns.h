// The pattern bank (milestone 9): 16 slots, saved with the Logic project.
//
// A slot holds what the Bloom v2 manual calls "the focused channel sequence": all 64 steps (Note, Gate, Slew, Ratchet, Mod value, Mod
// shape and rate, on / off) and LENGTH. Nothing else: Scale, Key, Root, Order, Rate, Ratio and the knobs are controls, not part of a
// pattern, and the Performance effects are never stored anywhere. A slot starts empty.
//
// Plain C++, no plugin code. The saved form is a block of bytes (the plugin keeps it as text in the project state):
//   [version = 1][16 times: used (0 or 1); if used: length (1 to 64), size (2 bytes, little end), then the StepData saved form].
// fromBytes returns false (and changes nothing) for anything it does not understand.

#pragma once

#include <fg/Steps.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fg
{
    struct Pattern
    {
        bool used = false;
        int length = 8;              // 1 to 64
        StepData steps;

        bool operator== (const Pattern&) const;
    };

    // The shared pattern (version 1.1.0): ONE pattern (the 64 steps and LENGTH) in a small block of bytes, for the file that SHARE writes and
    // GRAB reads. Layout: "FGSP", [version = 1], length (1 to 64), size (2 bytes, little end), then the StepData saved form.
    // sharedPatternFromBytes returns false (and changes nothing) for anything it does not understand: a missing, damaged or foreign file.
    std::vector<std::uint8_t> sharedPatternToBytes (const StepData&, int length);
    bool sharedPatternFromBytes (const std::uint8_t* data, std::size_t size, StepData& steps, int& length);

    struct PatternBank
    {
        std::array<Pattern, kNumPatternSlots> slots;

        bool isUsed (int slot) const;                                   // false for a slot outside 0 to 15
        int usedCount() const;

        // Stores the steps and LENGTH in a slot (a full slot is overwritten without asking). Returns false for a slot outside 0 to 15.
        bool save (int slot, const StepData&, int length);

        // Reads a slot. Returns false (and leaves the outputs alone) for an empty slot or a slot outside 0 to 15.
        bool load (int slot, StepData& steps, int& length) const;

        std::vector<std::uint8_t> toBytes() const;
        bool fromBytes (const std::uint8_t* data, std::size_t size);

        bool operator== (const PatternBank&) const;
    };
}
