cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

add_subdirectory(lib)

target_sources(
  gab
  PRIVATE
  os.c
  os.h
  main.c
)
