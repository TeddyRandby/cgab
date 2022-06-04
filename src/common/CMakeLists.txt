cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

target_sources(
  cgab
  PRIVATE
  types.h
  string.c
  vector.c
  dict.c
  assert.c
  memory.c
  log.c
  io.h
  io.c
  common.h
  PUBLIC
)
