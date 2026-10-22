cmake_minimum_required(VERSION 3.19)

project(lpeg C)

if(WIN32)
  set(LPEG_LIBRARY_TYPE STATIC)
else()
  set(LPEG_LIBRARY_TYPE SHARED)
endif()

add_library("${PROJECT_NAME}" ${LPEG_LIBRARY_TYPE}
  lpcap.c
  lpcode.c
  lpcset.c
  lpprint.c
  lptree.c
  lpvm.c
  lptypes.h
)

target_include_directories(
  ${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lua/
)

target_link_libraries(
  ${PROJECT_NAME}
  LINK_PUBLIC lua
)
