cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

target_sources(
  cgab
  PRIVATE
  types.h
  os.h
  os.c
  array.h
  slice.h
  vector.h
  dict.h
  core.h
  char.h
  char.c
  PUBLIC
)
