cmake_minimum_required(VERSION 3.22)
project(NordPianoReader VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(npno_reader STATIC src/NpnoReader.cpp)
target_include_directories(npno_reader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(npno_decoder STATIC src/NpnoDecoder.cpp)
target_link_libraries(npno_decoder PUBLIC npno_reader)
add_library(piano_engine STATIC src/PianoEngine.cpp)
target_link_libraries(piano_engine PUBLIC npno_decoder)
add_executable(check_npno tests/check_npno.cpp)
add_executable(decode_npno tests/decode_npno.cpp)
target_link_libraries(decode_npno PRIVATE npno_decoder)
add_executable(render_piano tests/render_piano.cpp)
target_link_libraries(render_piano PRIVATE piano_engine)
add_executable(test_dual_layer tests/test_dual_layer.cpp)
target_link_libraries(test_dual_layer PRIVATE piano_engine)
add_executable(test_long_sustain tests/test_long_sustain.cpp)
target_link_libraries(test_long_sustain PRIVATE piano_engine)
target_link_libraries(check_npno PRIVATE npno_reader)
option(BUILD_PLUGIN "Build VST3/AU plugin using JUCE" ON)
if(BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  if(APPLE)
    set(PLUGIN_FORMATS VST3 Standalone AU)
  else()
    set(PLUGIN_FORMATS VST3 Standalone)
  endif()

  juce_add_plugin(NordPianoPrototype
    COMPANY_NAME "PersonalAudioResearch"
    BUNDLE_ID "org.personalaudioresearch.npno"
    PLUGIN_MANUFACTURER_CODE "PaRe"
    PLUGIN_CODE "Npn1"
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Nord Pianos"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/src/app_icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/src/app_icon.png"
    COPY_PLUGIN_AFTER_BUILD FALSE)
  juce_generate_juce_header(NordPianoPrototype)
  target_sources(NordPianoPrototype PRIVATE
    src/PluginProcessor.cpp src/PluginProcessor.h
    src/PluginEditor.cpp src/PluginEditor.h)
  target_link_libraries(NordPianoPrototype PRIVATE
    piano_engine juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
  target_compile_definitions(NordPianoPrototype PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

  juce_add_console_app(render_ui_snapshot PRODUCT_NAME "RenderUISnapshot")
  juce_generate_juce_header(render_ui_snapshot)
  target_sources(render_ui_snapshot PRIVATE tests/render_ui_snapshot.cpp)
  target_include_directories(render_ui_snapshot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(render_ui_snapshot PRIVATE
    NordPianoPrototype juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

  juce_add_console_app(test_processor_runtime
    PRODUCT_NAME "TestProcessorRuntime")
  juce_generate_juce_header(test_processor_runtime)
  target_sources(test_processor_runtime PRIVATE tests/test_processor_runtime.cpp)
  target_include_directories(test_processor_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(test_processor_runtime PRIVATE
    NordPianoPrototype juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

  juce_add_console_app(test_sustain_loop PRODUCT_NAME "test_sustain_loop")
  juce_generate_juce_header(test_sustain_loop)
  target_sources(test_sustain_loop PRIVATE tests/test_sustain_loop.cpp)
  target_include_directories(test_sustain_loop PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(test_sustain_loop PRIVATE
    NordPianoPrototype juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
endif()
