cmake_minimum_required(VERSION 3.21.3)

project(gab LANGUAGES C VERSION 1.0)

include(GNUInstallDirs)

add_library(cgab)

add_library(regex SHARED src/lib/regex.c)
add_library(socket SHARED src/lib/socket.c)

set_target_properties(
  cgab
  PROPERTIES
  C_STANDARD 17
  VERSION 1
  SOVERSION 1
  PUBLIC_HEADER src/gab/gab.h
  POSITION_INDEPENDENT_CODE ON
)

set_target_properties(
  regex socket
  PROPERTIES
  C_STANDARD 17
  VERSION 1
  SOVERSION 1
  POSITION_INDEPENDENT_CODE ON
)

add_executable(gab src/cli/main.c)

target_link_libraries(regex cgab)
target_link_libraries(socket cgab)
target_link_libraries(gab cgab)

add_subdirectory(src/common)
add_subdirectory(src/gab)
add_subdirectory(src/cli)

install(
  TARGETS cgab gab socket regex
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gab
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
