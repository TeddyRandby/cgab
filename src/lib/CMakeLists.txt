cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

target_sources(
  cgab
  PRIVATE
  lib_io.c
  lib_str.c
  lib_obj.c
  lib_log.c
  lib_time.c
  lib_term.c
  lib_math.c
  lib_import.c
  lib_str.h
  lib_io.h
  lib_obj.h
  lib_log.h
  lib_time.h
  lib_term.h
  lib_math.h
  lib_import.h
)
