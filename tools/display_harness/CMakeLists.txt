# Display harness: renders every cv_* display of the editor to PNG with given parameter values.
# Not part of the default build. Standalone:
#   cmake -S tools/display_harness -B /home/claude/build-displays -G Ninja -DCMAKE_BUILD_TYPE=Debug -DJUCE_DIR=<path to JUCE>
#   cmake --build /home/claude/build-displays -j2 --target jpurrr_displays
cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
  project(jpurrr_displays_harness VERSION 0.1.0 LANGUAGES C CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
set(JPURRR_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
if(NOT TARGET juce::juce_gui_basics)
  if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
  else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
  endif()
endif()

# only the parameter tables and the factory presets of the core (no audio code)
add_library(jpurrr_displays_core STATIC
  ${JPURRR_ROOT}/core/src/params_gen.cpp ${JPURRR_ROOT}/core/src/factory_gen.cpp
  ${JPURRR_ROOT}/core/src/patch.cpp ${JPURRR_ROOT}/core/src/patch_json_stub.cpp)
target_include_directories(jpurrr_displays_core PUBLIC ${JPURRR_ROOT}/core/include)

file(GLOB FONT_FILES CONFIGURE_DEPENDS ${JPURRR_ROOT}/plugin/Resources/fonts/*.ttf)
juce_add_binary_data(JPurrrDisplayFonts SOURCES ${FONT_FILES})

file(GLOB DISPLAY_SRC CONFIGURE_DEPENDS ${JPURRR_ROOT}/plugin/Source/Editor/Displays/*.cpp)
juce_add_console_app(jpurrr_displays PRODUCT_NAME "jpurrr_displays")
target_sources(jpurrr_displays PRIVATE main.cpp ${DISPLAY_SRC} ${JPURRR_ROOT}/plugin/Source/Editor/Fonts.cpp)
target_include_directories(jpurrr_displays PRIVATE ${JPURRR_ROOT}/plugin/Source/Editor)
target_compile_definitions(jpurrr_displays PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALLOW_STATIC_NULL_VARIABLES=0)
target_link_libraries(jpurrr_displays PRIVATE jpurrr_displays_core JPurrrDisplayFonts juce::juce_gui_basics)
set_target_properties(jpurrr_displays PROPERTIES EXCLUDE_FROM_ALL ON)
