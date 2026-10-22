set(line_vst3_sources
  line_controller.cpp
  line_controller.hpp
  line_cids.hpp
  line_entry.cpp
  line_processor.cpp
  line_processor.hpp
  ../line_pattern.cpp
  ../line_pattern.hpp
  resource/line.uidesc
)

smtg_add_vst3plugin(line_instrument PACKAGE_NAME "Line"
  SOURCES_LIST ${line_vst3_sources}
)
smtg_target_configure_version_file(line_instrument)

target_compile_features(line_instrument PRIVATE cxx_std_17)
target_include_directories(line_instrument PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(line_instrument PRIVATE sdk vstgui_support)
smtg_target_add_plugin_resources(line_instrument
  RESOURCES
    resource/line.uidesc
)

if(BUILD_TESTING)
  add_executable(line_audio_output_tests
    line_processor.cpp
    ../line_pattern.cpp
    ../../tests/line_audio_output_tests.cpp
  )
  target_compile_features(line_audio_output_tests PRIVATE cxx_std_17)
  target_include_directories(line_audio_output_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
  )
  target_link_libraries(line_audio_output_tests PRIVATE sdk)
  add_test(NAME line_audio_output_tests COMMAND line_audio_output_tests)
endif()

set_target_properties(line_instrument PROPERTIES
  OUTPUT_NAME "Line"
  VERSION "0.1.0"
)
