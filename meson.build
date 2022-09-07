project('Gab Scripting Language', 'c')

subdir('src/gab')
subdir('src/core')

dl = dependency('dl')

libcgab = library('cgab', sources, dependencies : dl)

executable('gab', 'src/cli/main.c', dependencies: [dl], link_with: libcgab)
