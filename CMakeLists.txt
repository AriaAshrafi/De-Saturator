cmake_minimum_required(VERSION 3.25)

# Must be set before project(). Ignored on non-Apple platforms.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(DeSaturator VERSION 0.9.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so users don't need the VC++ redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(DESAT_BUILD_PLUGIN "Build the plugin (fetches JUCE)" ON)
option(DESAT_BUILD_TESTS "Build the test suite" ON)
option(DESAT_COPY_PLUGIN "Copy the built plugin into the system plugin folder (may need admin rights)" OFF)

include(FetchContent)

# ---------------------------------------------------------------------------
# The engines: plain C++ with no JUCE, so the offline tools and tests build in seconds
# ---------------------------------------------------------------------------
add_library(desat_dsp STATIC
    src/dsp/SpectralThinner.cpp
    src/dsp/LiveThinner.cpp
    src/dsp/Engine.cpp)
target_include_directories(desat_dsp PUBLIC src)
target_compile_features(desat_dsp PUBLIC cxx_std_20)
set_target_properties(desat_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(desat_dsp PRIVATE /W4)
else()
    target_compile_options(desat_dsp PRIVATE -Wall -Wextra)
endif()

# The render rig: desat_render [--out folder] [--in file.wav]
add_executable(desat_render tools/render.cpp)
target_include_directories(desat_render PRIVATE tools)
target_link_libraries(desat_render PRIVATE desat_dsp)

# CPU timing of each part, and a close look at the spectrum: desat_bench
add_executable(desat_bench tools/bench.cpp)
target_include_directories(desat_bench PRIVATE tools)
target_link_libraries(desat_bench PRIVATE desat_dsp)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(DESAT_BUILD_PLUGIN)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE)
    FetchContent_MakeAvailable(JUCE)

    # Interface assets from the design kit (design/), embedded in the binary
    juce_add_binary_data(DeSaturatorAssets
        NAMESPACE DeSaturatorAssets
        HEADER_NAME DeSaturatorAssets.h
        SOURCES
            design/fonts/Anybody-SemiBold-600.ttf
            design/fonts/Anybody-Bold-700.ttf
            design/fonts/Anybody-Medium-500.ttf
            design/fonts/DepartureMono-Regular.otf
            design/ui/assets/wordmark-header.svg
            design/ui/assets/wordmark-header-dark.svg
            design/ui/assets/credit.svg
            design/ui/assets/credit-dark.svg
            design/spec/spec.json)
    set_target_properties(DeSaturatorAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # Code shared by the plugin, the tests and the snapshot tool (compiled into each of them)
    add_library(desat_shared INTERFACE)
    target_sources(desat_shared INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/PluginProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/Analyser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/PluginEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/DeSaturatorView.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/AccessibleRegion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Theme.cpp)
    target_include_directories(desat_shared INTERFACE src)
    target_compile_definitions(desat_shared INTERFACE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        DESAT_VERSION_STRING="${PROJECT_VERSION}")
    target_link_libraries(desat_shared INTERFACE
        desat_dsp
        DeSaturatorAssets
        juce::juce_audio_utils
        juce::juce_dsp)

    set(DESAT_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND DESAT_FORMATS AU)   # Logic Pro / GarageBand only load AU
    endif()

    # An Aria Ashrafi product (com.ariaashrafi.*). The manufacturer and plugin codes identify the
    # plugin to hosts (AU, VST3 class IDs): never change them after a release.
    juce_add_plugin(DeSaturator
        PRODUCT_NAME                "De-Saturator"
        COMPANY_NAME                "Aria Ashrafi"
        COMPANY_WEBSITE             "https://ariaashrafi.com"
        BUNDLE_ID                   "com.ariaashrafi.desaturator"
        PLUGIN_MANUFACTURER_CODE    Aash
        PLUGIN_CODE                 Dst1
        ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/design/installers/icon-big.png"
        FORMATS                     ${DESAT_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE     # no shortcuts: keys stay with the host
        COPY_PLUGIN_AFTER_BUILD     ${DESAT_COPY_PLUGIN}
        VST3_CATEGORIES             Fx Filter Dynamics
        AU_MAIN_TYPE                kAudioUnitType_Effect)

    target_link_libraries(DeSaturator
        PRIVATE
            desat_shared
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # The design kit's VST3 resources (bundle icon, host snapshots), after JUCE writes moduleinfo.json.
    # The artefact path holds generator expressions, which add_custom_command evaluates (as JUCE's own steps do).
    get_target_property(DESAT_VST3_BUNDLE DeSaturator_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
    add_custom_command(TARGET DeSaturator_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DBUNDLE=${DESAT_VST3_BUNDLE}"
            "-DBRAND=${CMAKE_CURRENT_SOURCE_DIR}/design/installers/vst3"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Vst3Resources.cmake"
        VERBATIM)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DESAT_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    # The engines on their own: fast, no JUCE.
    add_executable(DeSaturatorDspTests tests/EngineTests.cpp)
    target_include_directories(DeSaturatorDspTests PRIVATE tools)
    target_link_libraries(DeSaturatorDspTests PRIVATE desat_dsp Catch2::Catch2WithMain)
    add_test(NAME DeSaturatorDspTests COMMAND DeSaturatorDspTests)

    if(DESAT_BUILD_PLUGIN)
        add_executable(DeSaturatorPluginTests tests/ProcessorTests.cpp)
        target_compile_definitions(DeSaturatorPluginTests PRIVATE
            DESAT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            JUCE_MODAL_LOOPS_PERMITTED=1)
        target_include_directories(DeSaturatorPluginTests PRIVATE tools)
        target_link_libraries(DeSaturatorPluginTests PRIVATE
            desat_shared
            Catch2::Catch2WithMain
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
        add_test(NAME DeSaturatorPluginTests COMMAND DeSaturatorPluginTests)

        # Renders the window to PNG files to compare with design/states: desat_snapshot [folder]
        add_executable(desat_snapshot tools/snapshot.cpp)
        target_compile_definitions(desat_snapshot PRIVATE
            DESAT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(desat_snapshot PRIVATE
            desat_shared
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    endif()
endif()
