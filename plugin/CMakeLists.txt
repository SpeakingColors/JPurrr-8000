# JUCE plugin (VST3 + Standalone). JUCE comes from -DJUCE_DIR=<path> or is fetched at configure time.
if(JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(JPurrr8000
  COMPANY_NAME "Studio 87"
  PRODUCT_NAME "JPurrr-8000"
  PLUGIN_MANUFACTURER_CODE St87
  PLUGIN_CODE JPu8
  FORMATS VST3 Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Instrument Synth
  COPY_PLUGIN_AFTER_BUILD FALSE)

file(GLOB_RECURSE JPURRR_PLUGIN_SRC CONFIGURE_DEPENDS Source/*.cpp)
target_sources(JPurrr8000 PRIVATE ${JPURRR_PLUGIN_SRC})
target_include_directories(JPurrr8000 PRIVATE Source)
target_compile_definitions(JPurrr8000 PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(JPurrr8000 PRIVATE jpurrr_core
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
