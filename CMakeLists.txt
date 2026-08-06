cmake_minimum_required(VERSION 3.24)

project(SappOrchestra VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SAPPORCHESTRA_BUILD_PLUGIN "Build JUCE plugin + standalone targets" ON)
option(SAPPORCHESTRA_BUILD_TESTS  "Build unit tests"                       ON)
option(SAPPORCHESTRA_BUILD_CLI    "Build the sapporchestra agent CLI"      ON)

include(cmake/CompilerWarnings.cmake)
include(FetchContent)

# ------------------------------------------------------------- SappSounds ---
# Local development uses the sibling checkout; CI or isolated builds may set
# SAPPSOUNDS_DIR or fall back to FetchContent from the public repo.
set(SAPPSOUNDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sappsounds" CACHE PATH
    "Path to a local SappSounds checkout (sibling directory by default)")

if(EXISTS "${SAPPSOUNDS_DIR}/CMakeLists.txt")
  add_subdirectory("${SAPPSOUNDS_DIR}" "${CMAKE_BINARY_DIR}/SappSounds")
else()
  FetchContent_Declare(SappSounds
    GIT_REPOSITORY https://github.com/localteampartners/sappsounds.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(SappSounds)
endif()

# ------------------------------------------------------------------- core ---
# Orchestra policy on top of Sapp::Sounds. Framework-independent (no JUCE):
# consumed by the plugin, the CLI, and the tests.
add_library(sapporchestra_core STATIC
  src/core/OrchestraEngine.cpp
  src/core/OrchestraRender.cpp
  src/core/SappLinkCCMap.cpp
)
target_include_directories(sapporchestra_core PUBLIC src)
target_link_libraries(sapporchestra_core PUBLIC Sapp::Sounds)
sapporchestra_set_warnings(sapporchestra_core)

# -------------------------------------------------------------------- cli ---
if(SAPPORCHESTRA_BUILD_CLI)
  add_executable(sapporchestra-cli src/cli/main.cpp)
  set_target_properties(sapporchestra-cli PROPERTIES OUTPUT_NAME sapporchestra)
  target_link_libraries(sapporchestra-cli PRIVATE sapporchestra_core)
  sapporchestra_set_warnings(sapporchestra-cli)
endif()

# ------------------------------------------------------------------ tests ---
if(SAPPORCHESTRA_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(SappOrchestraTests
    tests/unit/test_orchestra_engine.cpp
    tests/unit/test_room.cpp
    tests/unit/test_render.cpp
    tests/unit/test_sapplink.cpp
  )
  target_link_libraries(SappOrchestraTests PRIVATE sapporchestra_core Catch2::Catch2WithMain)
  target_compile_definitions(SappOrchestraTests PRIVATE
    SAPPORCH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
  sapporchestra_set_warnings(SappOrchestraTests)

  enable_testing()
  add_test(NAME unit COMMAND SappOrchestraTests)
endif()

# ----------------------------------------------------------------- plugin ---
if(SAPPORCHESTRA_BUILD_PLUGIN)
  # Same pinned JUCE tag as SappSynth (see DECISIONS.md there).
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(SappOrchestraPlugin
    PRODUCT_NAME                "SappOrchestra"
    COMPANY_NAME                "LocalTeamPartners"
    BUNDLE_ID                   com.localteampartners.sapporchestra
    PLUGIN_MANUFACTURER_CODE    Ltpr
    PLUGIN_CODE                 Sorc
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    FORMATS                     Standalone VST3 AU
  )
  if(APPLE)
    juce_enable_copy_plugin_step(SappOrchestraPlugin)
  endif()

  target_sources(SappOrchestraPlugin PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
  )
  target_link_libraries(SappOrchestraPlugin PRIVATE
    sapporchestra_core
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
  )
  target_compile_definitions(SappOrchestraPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
  )

  if(APPLE)
    # Offscreen editor snapshot tool (UI verification without screen capture).
    juce_add_gui_app(SappOrchestraUiShot PRODUCT_NAME "SappOrchestraUiShot")
    target_sources(SappOrchestraUiShot PRIVATE
      tools/uishot/main.cpp
      src/plugin/PluginProcessor.cpp
      src/plugin/PluginEditor.cpp
    )
    target_include_directories(SappOrchestraUiShot PRIVATE src/plugin)
    target_link_libraries(SappOrchestraUiShot PRIVATE
      sapporchestra_core
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_recommended_config_flags
    )
    target_compile_definitions(SappOrchestraUiShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  endif()
endif()
