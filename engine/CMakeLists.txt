# The Fractal-Garden engine. Plain C++17, no JUCE, no plugin code.
#   include/fg/Params.h   the 46 controls (ids, ranges, defaults, menu choices): the single source for the plugin
#   include/fg/Steps.h    the 64 steps of data and how they are saved
# The sequencer itself arrives in later milestones.

add_library(fg_engine STATIC
    src/Params.cpp
    src/Steps.cpp
)

target_include_directories(fg_engine PUBLIC include)
target_compile_features(fg_engine PUBLIC cxx_std_17)

if(FG_BUILD_TESTS)
    add_executable(fg_engine_test tests/engine_test.cpp)
    target_link_libraries(fg_engine_test PRIVATE fg_engine)
    add_test(NAME engine COMMAND fg_engine_test)
endif()
