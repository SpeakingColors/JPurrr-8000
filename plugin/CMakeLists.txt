# JUCE plugin (VST3 + Standalone). JUCE comes from -DJUCE_DIR=<path> or is fetched at configure time.
if(JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(juce)
endif()

# App icon (assets/icon, made by tools/make_icons.py). JUCE writes it into the macOS .app and VST3 bundle
# (Info.plist CFBundleIconFile) and into the Windows exe and VST3 (resource, Plugin.ico, desktop.ini).
set(JP_ICON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon)
set(JP_INSTALLER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../installer)
if(APPLE)
  set(JP_ICON_BIG ${JP_ICON_DIR}/jpurrr-8000-macos-1024.png)
  set(JP_ICON_SMALL "")
else()
  set(JP_ICON_BIG ${JP_ICON_DIR}/jpurrr-8000-1024.png)
  set(JP_ICON_SMALL ${JP_ICON_DIR}/jpurrr-8000-256.png)
endif()

juce_add_plugin(JPurrr8000
  COMPANY_NAME "Studio 87"
  BUNDLE_ID com.studio87.jpurrr8000
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
  ICON_BIG ${JP_ICON_BIG}
  ICON_SMALL ${JP_ICON_SMALL}
  COPY_PLUGIN_AFTER_BUILD FALSE)

# JUCE makes its own .icns / .ico from the PNGs, only a few sizes. Swap in the hand tuned files
# (every size from 16 px up, simplified small cat) at the place JUCE uses.
if(APPLE OR WIN32)
  get_target_property(JP_GENERATED_ICON JPurrr8000 JUCE_ICON_FILE)
  if(JP_GENERATED_ICON)
    if(APPLE)
      set(JP_TUNED_ICON ${JP_INSTALLER_DIR}/macos/jpurrr-8000.icns)
    else()
      set(JP_TUNED_ICON ${JP_INSTALLER_DIR}/windows/jpurrr-8000.ico)
    endif()
    configure_file(${JP_TUNED_ICON} ${JP_GENERATED_ICON} COPYONLY)
  endif()
endif()

# The window icon of the Linux standalone (JUCE does not set one itself). Own header so it does not clash with the fonts.
juce_add_binary_data(JPurrrIcon HEADER_NAME IconData.h NAMESPACE IconData SOURCES ${JP_ICON_DIR}/jpurrr-8000-256.png)

file(GLOB JPURRR_FONT_FILES CONFIGURE_DEPENDS Resources/fonts/*.ttf)
juce_add_binary_data(JPurrrFonts SOURCES ${JPURRR_FONT_FILES})
file(GLOB_RECURSE JPURRR_PLUGIN_SRC CONFIGURE_DEPENDS Source/*.cpp)
target_sources(JPurrr8000 PRIVATE ${JPURRR_PLUGIN_SRC})
target_include_directories(JPurrr8000 PRIVATE Source)
target_compile_definitions(JPurrr8000 PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(JPurrr8000 PRIVATE jpurrr_core JPurrrFonts JPurrrIcon
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Headless verification tool (not built by default): writes PNGs of every page and dialog at design scale 1.
#   cmake --build <dir> --target jpurrr_snapshot && <dir>/plugin/jpurrr_snapshot <outdir> [sound chain master guide ...]
add_executable(jpurrr_snapshot EXCLUDE_FROM_ALL snapshot/snapshot.cpp)
# Reuse the compiled plugin library (JPurrr8000 is a static library underneath); nothing is compiled twice.
target_include_directories(jpurrr_snapshot PRIVATE Source $<TARGET_PROPERTY:JPurrr8000,INCLUDE_DIRECTORIES>)
target_compile_definitions(jpurrr_snapshot PRIVATE $<TARGET_PROPERTY:JPurrr8000,COMPILE_DEFINITIONS>)
target_link_libraries(jpurrr_snapshot PRIVATE JPurrr8000 jpurrr_core JPurrrFonts JPurrrIcon)

# Offline audio check of the real processor: every factory sound, a held chord, level and finite output.
add_executable(jpurrr_render EXCLUDE_FROM_ALL snapshot/render.cpp)
target_include_directories(jpurrr_render PRIVATE Source $<TARGET_PROPERTY:JPurrr8000,INCLUDE_DIRECTORIES>)
target_compile_definitions(jpurrr_render PRIVATE $<TARGET_PROPERTY:JPurrr8000,COMPILE_DEFINITIONS>)
target_link_libraries(jpurrr_render PRIVATE JPurrr8000 jpurrr_core JPurrrFonts JPurrrIcon)

# Loads the built .vst3 through JUCE's VST3 host code: instance, MIDI, audio, state round trip.
add_executable(jpurrr_hostcheck EXCLUDE_FROM_ALL ../tools/hostcheck.cpp)
target_compile_definitions(jpurrr_hostcheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(jpurrr_hostcheck PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)

# Headless check of the keyboard strip: strip notes make sound through the real processor, wheels, computer keys, PREVIEW off, editor close.
add_executable(jpurrr_stripcheck EXCLUDE_FROM_ALL snapshot/stripcheck.cpp)
target_include_directories(jpurrr_stripcheck PRIVATE Source $<TARGET_PROPERTY:JPurrr8000,INCLUDE_DIRECTORIES>)
target_compile_definitions(jpurrr_stripcheck PRIVATE $<TARGET_PROPERTY:JPurrr8000,COMPILE_DEFINITIONS>)
target_link_libraries(jpurrr_stripcheck PRIVATE JPurrr8000 jpurrr_core JPurrrFonts JPurrrIcon)
