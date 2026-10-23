cmake_minimum_required(VERSION 3.19.2)

set(CMAKE_CXX_STANDARD 17)

option(GIT_ACTION "Is Git Action" OFF)
option(BUILD_17 "Building as C++17" ON)
option(LINE_BUILD_VST3 "Build the Line VST3 instrument" OFF)

if(APPLE AND NOT GIT_ACTION)
  if(BUILD_17)
    set(CMAKE_C_COMPILER /opt/local/libexec/llvm-10/bin/clang)
    set(CMAKE_CXX_COMPILER /opt/local/libexec/llvm-10/bin/clang++)
  else()  
    set(CMAKE_C_COMPILER /usr/bin/clang)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++)
  endif()
endif()

if(LINUX AND GIT_ACTION)
  set(CMAKE_C_COMPILER /usr/bin/gcc)
  set(CMAKE_CXX_COMPILER /usr/bin/g++)
endif()

project(line VERSION 0.7.1)

include(CTest)

add_library(line_pattern STATIC plugin/line_pattern.cpp)
target_compile_features(line_pattern PUBLIC cxx_std_17)
target_include_directories(line_pattern PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/plugin)

add_library(line_midi_file STATIC midi_file.cpp)
target_compile_features(line_midi_file PUBLIC cxx_std_17)
target_include_directories(line_midi_file PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
  add_executable(line_pattern_tests tests/line_pattern_tests.cpp)
  target_link_libraries(line_pattern_tests PRIVATE line_pattern)
  add_test(NAME line_pattern_tests COMMAND line_pattern_tests)

  add_executable(line_midi_file_tests tests/line_midi_file_tests.cpp)
  target_link_libraries(line_midi_file_tests PRIVATE line_midi_file)
  add_test(NAME line_midi_file_tests COMMAND line_midi_file_tests)
endif()

add_executable("${PROJECT_NAME}" line.cpp)
target_link_libraries("${PROJECT_NAME}" PRIVATE line_midi_file)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/externals/lua)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/externals/lpeg)
set(RTMIDI_API_JACK OFF CACHE BOOL "Do not require JACK for MIDI output" FORCE)
set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "Link RtMidi statically" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/externals/rtmidi)

include(${CMAKE_CURRENT_SOURCE_DIR}/externals/link/AbletonLinkConfig.cmake)

if(NOT WIN32)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(READLINE REQUIRED IMPORTED_TARGET readline)
endif()

target_include_directories(
  ${PROJECT_NAME} PUBLIC
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/externals/lua/
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/externals/lpeg/
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/externals/rtmidi/
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/externals/link/include/ableton/
)

target_link_directories(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/externals/lua/
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/externals/lpeg/
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/build/externals/rtmidi/
  #PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/build/externals/link/src/ableton/link/
)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE -DLINKHUT_AUDIO_PLATFORM_DUMMY=1
)

target_link_libraries(
  ${PROJECT_NAME}
  LINK_PRIVATE lua
  LINK_PRIVATE lpeg
  LINK_PRIVATE rtmidi
  Ableton::Link
)

if(NOT WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::READLINE)
endif()

if(BUILD_TESTING)
  add_test(
    NAME line_cli_note_phrase_tests
    COMMAND $<TARGET_FILE:${PROJECT_NAME}> --check-phrase
            "c3 - e3 - g3 - c4"
  )
  add_test(
    NAME line_cli_chord_phrase_tests
    COMMAND $<TARGET_FILE:${PROJECT_NAME}> --check-phrase
            "(c3 e3 g3)~.7 - d3~.5 .g3 b3."
  )
  add_test(
    NAME line_cli_compact_rest_phrase_tests
    COMMAND $<TARGET_FILE:${PROJECT_NAME}> --check-phrase
            "c3 - e3 - g3 -c4"
  )
  add_test(
    NAME line_cli_export_first_phrase
    COMMAND $<TARGET_FILE:${PROJECT_NAME}> --export-midi
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/export_test.line
            ${CMAKE_CURRENT_BINARY_DIR}/line_cli_export_first.mid 15
  )
  add_test(
    NAME line_cli_export_all_phrases
    COMMAND $<TARGET_FILE:${PROJECT_NAME}> --export-midi
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/export_test.line
            ${CMAKE_CURRENT_BINARY_DIR}/line_cli_export_all.mid 15
            --all-phrases --bpm 120
  )
  set_tests_properties(
    line_cli_note_phrase_tests
    line_cli_chord_phrase_tests
    line_cli_compact_rest_phrase_tests
    line_cli_export_first_phrase
    line_cli_export_all_phrases
    PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  set_tests_properties(
    line_cli_export_first_phrase
    PROPERTIES PASS_REGULAR_EXPRESSION "Exported 1 saved phrase"
  )
  set_tests_properties(
    line_cli_export_all_phrases
    PROPERTIES PASS_REGULAR_EXPRESSION "Exported 2 saved phrases"
  )
endif()

if(LINE_BUILD_VST3)
  include(FetchContent)
  set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.7.9_build_61
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
  )
  FetchContent_MakeAvailable(vst3sdk)
  set(public_sdk_SOURCE_DIR "${vst3sdk_SOURCE_DIR}/public.sdk")
  add_subdirectory(plugin/vst3)
endif()

install(TARGETS ${PROJECT_NAME})
