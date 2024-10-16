cmake_minimum_required(VERSION 3.3)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

project(cgab VERSION 0.1 LANGUAGES C)

add_library(cgab STATIC
    src/cgab/engine.c
    src/cgab/parser.c
    src/cgab/object.c
    src/cgab/lexer.c
    src/cgab/gc.c
    src/cgab/vm.c
)

target_include_directories(cgab PRIVATE include)

add_executable(gab src/gab/main.c src/os/os.c)

target_include_directories(gab PRIVATE include)
target_link_libraries(gab cgab -static)

install(TARGETS gab cgab)
