# Hamamatsu 1000 — JUCE plugin shell (VST3 / AU / Standalone)
#
# Working title: "Hamamatsu 1000" — a legally-distinct homage to the
# Yamaha PM1000 channel strip (Yamaha's home city is Hamamatsu).
# The entire sound lives in ../dsp/Pm1000Dsp.{h,cpp} (framework-free).
# This directory is only the plugin wrapper: parameters, DAW I/O, GUI.
#
# Build (macOS, Xcode):  cmake -B build -G Xcode ; cmake --build build --config Release
# Build (Linux): install JUCE deps, then cmake -B build && cmake --build build -j
#
# NOTE: JUCE 8 is AGPLv3-or-commercial. DSP core has no JUCE dependency.

cmake_minimum_required(VERSION 3.22)
project(Hamamatsu1000 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8.0.6: use $JUCE_DIR if set (CI), else the vendored copy.
if (DEFINED ENV{JUCE_DIR} AND NOT "$ENV{JUCE_DIR}" STREQUAL "")
    set(JUCE_DIR "$ENV{JUCE_DIR}")
else ()
    set(JUCE_DIR "$ENV{HOME}/workspace/vendor/juce-8.0.6")
endif ()
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif ()
add_subdirectory("${JUCE_DIR}" juce EXCLUDE_FROM_ALL)

juce_add_plugin(Hamamatsu1000
    COMPANY_NAME "Sabatino Audio"
    PLUGIN_NAME "Hamamatsu 1000"
    DESCRIPTION "PM1000-style channel strip: discrete-style preamp, 3-band EQ, rack output section."
    PLUGIN_MANUFACTURER_CODE SabA
    PLUGIN_CODE Hm10
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Hamamatsu 1000"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(Hamamatsu1000 PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    PmUI.cpp
    ../dsp/Pm1000Dsp.cpp
)

target_include_directories(Hamamatsu1000 PRIVATE
    ../dsp
)

target_link_libraries(Hamamatsu1000 PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_dsp
)

target_compile_definitions(Hamamatsu1000 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Embedded silkscreen font (Oswald Bold, OFL) — condensed grotesque
# matching the PM1000's panel lettering.
juce_add_binary_data(PmAssets SOURCES Assets/Oswald-Bold.ttf Assets/PermanentMarker-Regular.ttf)
target_link_libraries(Hamamatsu1000 PRIVATE PmAssets)

# ─────────────────────────────────────────────────────────────────────
# Headless GUI snapshot renderer (dev verification only, not shipped).
# Produces build/PmSnapshot; run under xvfb to render the editor to
# /tmp/pm1000-gui.png for visual QA.
# ─────────────────────────────────────────────────────────────────────
add_executable(PmSnapshot
    ../tests/snapshot.cpp
    PluginProcessor.cpp
    PluginEditor.cpp
    PmUI.cpp
    ../dsp/Pm1000Dsp.cpp
)
target_include_directories(PmSnapshot PRIVATE ../dsp ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PmSnapshot PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    PmAssets
)
target_compile_definitions(PmSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_APPLICATION=1
)
