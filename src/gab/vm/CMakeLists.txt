cmake_minimum_required(VERSION 3.21.3)
project(gab LANGUAGES C VERSION 0.1)

target_sources(
  cgab
  PRIVATE
  gc.c
  gc.h
  vm.c
  vm.h
)
