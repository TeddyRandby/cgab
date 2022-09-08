project('Gab Scripting Language',
  'c',
  default_options: 'default_library=static'
  )

subdir('src/gab')
subdir('src/core')

dl = dependency('dl')

libcgab = library('cgab',
  sources,
  dependencies : dl,
  install: true,
)

executable('gab', 'src/cli/main.c', dependencies: [dl], link_with: libcgab, install: true)
