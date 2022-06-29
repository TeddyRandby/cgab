cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

target_sources(
  cgab
  PRIVATE
  bytecode.h
  value.h
  compiler.c
  module.h
  module.c
  object.h
  object.c
  compiler.h
  engine.h
  engine.c
)
