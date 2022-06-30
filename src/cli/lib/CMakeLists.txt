cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

target_sources(
  gab
  PRIVATE
  lib.h
  lib_log.c
  lib_require.c
  lib_obj.c
  lib_sock.c
  lib_str.c
  lib_regex.c
)
