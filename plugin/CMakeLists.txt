# The Fractal-Garden plugin wrapper: what Logic sees (AU / VST3, a MIDI effect) and the styled panel.
# The control list and the step data live in ../engine.

# The fonts used by the styled panel (Space Grotesk and IBM Plex Mono, both free open-licence fonts; the licence texts sit
# next to them in Fonts/). They are built into the plugin so it looks the same on every Mac.
juce_add_binary_data(FractalGardenFonts
    HEADER_NAME FgFontData.h
    NAMESPACE   FgFontData
    SOURCES
        Fonts/SpaceGrotesk-Regular.ttf
        Fonts/SpaceGrotesk-Medium.ttf
        Fonts/SpaceGrotesk-Bold.ttf
        Fonts/IBMPlexMono-Regular.ttf
        Fonts/IBMPlexMono-Medium.ttf
)

# The Audio Unit format exists on macOS only; elsewhere (the Linux checking computer) only VST3 is built.
if(APPLE)
    set(FG_FORMATS AU VST3)
else()
    set(FG_FORMATS VST3)
endif()

# A MIDI effect (AU type aumi), set up like Dotriaconta-tone: no audio buses, MIDI in and MIDI out.
# PRODUCT_NAME is the file name (FractalGarden.component / .vst3); PLUGIN_NAME is the name Logic shows.
juce_add_plugin(FractalGarden
    COMPANY_NAME                "particledots"
    BUNDLE_ID                   com.particledots.fractalgarden
    PLUGIN_MANUFACTURER_CODE    Pdot
    PLUGIN_CODE                 Fgdn
    FORMATS                     ${FG_FORMATS}
    PRODUCT_NAME                "FractalGarden"
    PLUGIN_NAME                 "Fractal-Garden"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx
    AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor
)

target_sources(FractalGarden PRIVATE
    Source/Parameters.h
    Source/Parameters.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/PanelLookAndFeel.h
    Source/PanelLookAndFeel.cpp
    Source/StyledPanel.h
    Source/StyledPanel.cpp
    Source/SliderPanel.h
    Source/SliderPanel.cpp
)

target_compile_definitions(FractalGarden PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FractalGarden
    PRIVATE
        fg_engine
        FractalGardenFonts
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Optional, off by default (the GitHub build does not use it): a small program that draws the windows to picture files and runs
# automatic checks on the controls and the saved state, without needing Logic. Build with -DFG_BUILD_PREVIEW=ON.
option(FG_BUILD_PREVIEW "Build the offline window preview and control checks" OFF)
if(FG_BUILD_PREVIEW)
    juce_add_console_app(FractalGardenPreview PRODUCT_NAME "FractalGardenPreview")
    target_sources(FractalGardenPreview PRIVATE
        preview/preview.cpp
        Source/Parameters.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PanelLookAndFeel.cpp
        Source/StyledPanel.cpp
        Source/SliderPanel.cpp
    )
    target_include_directories(FractalGardenPreview PRIVATE Source)
    target_compile_definitions(FractalGardenPreview PRIVATE
        JucePlugin_Name="Fractal-Garden"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(FractalGardenPreview
        PRIVATE fg_engine FractalGardenFonts juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
