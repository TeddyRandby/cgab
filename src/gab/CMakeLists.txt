cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

add_subdirectory(lexer)
add_subdirectory(compiler)
add_subdirectory(vm)

target_sources(
  cgab
  PRIVATE
  gab.c
  log.c
  PUBLIC
  gab.h
)
