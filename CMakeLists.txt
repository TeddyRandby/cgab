cmake_minimum_required(VERSION 3.21.3)

project(gab LANGUAGES C VERSION 1.0)

include(GNUInstallDirs)

add_library(cgab "")

set_property(TARGET cgab PROPERTY C_STANDARD 17)

add_subdirectory(src/common)
add_subdirectory(src/gab)
add_subdirectory(src/lib)

set_target_properties(
  cgab
  PROPERTIES
  VERSION 1
  SOVERSION 1
  PUBLIC_HEADER src/gab/gab.h
)

configure_file(gab.pc.in gab.pc @ONLY)

add_executable(gab src/cli/main.c)
target_link_libraries(gab cgab)

find_library(check check)

add_executable(test src/tests/main.c)
target_link_libraries(test cgab check)

install(
  TARGETS cgab gab
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  FILES ${CMAKE_BINARY_DIR}/gab.pc
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)
