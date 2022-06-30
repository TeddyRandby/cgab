cmake_minimum_required(VERSION 3.21.3)

project(gab LANGUAGES C VERSION 1.0)

include(GNUInstallDirs)

configure_file(gab.pc.in gab.pc @ONLY)

add_library(cgab "")

set_target_properties(
  cgab
  PROPERTIES
  VERSION 1
  SOVERSION 1
  PUBLIC_HEADER src/gab/gab.h
  C_STANDARD 17
)

add_executable(gab src/cli/main.c)

target_link_libraries(gab cgab)

add_subdirectory(src/common)
add_subdirectory(src/gab)
add_subdirectory(src/cli)


install(
  TARGETS cgab gab
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES ${CMAKE_BINARY_DIR}/gab.pc
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)
