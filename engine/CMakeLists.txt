# The Fractal-Garden engine. Plain C++17, no JUCE, no plugin code.
#   include/fg/Params.h   the 46 controls (ids, ranges, defaults, menu choices): the single source for the plugin
#   include/fg/Steps.h    the 64 steps of data and how they are saved
#   include/fg/Pitch.h    the 28 scales and the pitch a stored note plays for Scale, Key and Root (milestone 4)
#   include/fg/Orders.h   the 11 play orders (milestone 4)
#   include/fg/Mod.h      the four Mod output modes and the sixteen Mod rates (milestone 5)
#   include/fg/Mutate.h   Mutate, Micro Mutate and Reseed: the random changes to steps (milestones 6 and 7)
#   include/fg/Branch.h   branches and Path (milestone 6)
#   include/fg/Perform.h  Performance mode: Ratchet FX, Slew FX, Mutate FX and the ornaments (milestone 8)
#   include/fg/Clock.h    the sequencer clock and the trunk player (milestone 3): Logic's song position in, notes out
# The rest of the sequencer arrives in later milestones.

add_library(fg_engine STATIC
    src/Params.cpp
    src/Pitch.cpp
    src/Orders.cpp
    src/Steps.cpp
    src/Mod.cpp
    src/Mutate.cpp
    src/Branch.cpp
    src/Perform.cpp
    src/Clock.cpp
)

target_include_directories(fg_engine PUBLIC include)
target_compile_features(fg_engine PUBLIC cxx_std_17)

if(FG_BUILD_TESTS)
    add_executable(fg_engine_test tests/engine_test.cpp)
    target_link_libraries(fg_engine_test PRIVATE fg_engine)
    add_test(NAME engine COMMAND fg_engine_test)

    add_executable(fg_clock_test tests/clock_test.cpp)
    target_link_libraries(fg_clock_test PRIVATE fg_engine)
    add_test(NAME clock COMMAND fg_clock_test)
endif()
