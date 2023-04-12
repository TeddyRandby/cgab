project('Gab Scripting Language',
  'c',
  default_options: ['c_std=c2x']
)

sources = files()

subdir('src/gab')
subdir('src/core')
subdir('src/cli')

nc = dependency('ncursesw')

libcgab = static_library('cgab',
  sources,
  install: true,
)

shared_library('cgabfiber',
  'src/lib/fiber.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabterm',
  'src/lib/term.c',
  dependencies: [nc],
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabsocket',
  'src/lib/socket.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabtime',
  'src/lib/time.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabmap',
  'src/lib/map.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgablist',
  'src/lib/list.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabstring',
  'src/lib/string.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabrecord',
  'src/lib/rec.c',
  'src/lib/map.h',
  'src/lib/list.h',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabnum',
  'src/lib/num.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabio',
  'src/lib/io.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabdis',
  'src/lib/dis.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

shared_library('cgabpry',
  'src/lib/pry.c',
  link_with: [libcgab],
  install: true,
  install_dir: '/usr/local/lib/gab/'
)

install_headers(
  'include/gab.h',
  'include/object.h',
  'include/value.h',
  'include/core.h',
  'include/types.h',
  'include/slice.h',
  'include/array.h',
  'include/vector.h',
  'include/dict.h',
  'include/hash.h',
  install_dir: '/usr/local/include/gab/'
  )

install_data(
  'std/mod.gab',
  'std/Monad.gab',
  'std/Range.gab',
  'std/String.gab',
  'std/Number.gab',
  'std/Time.gab',
  'std/List.gab',
  'std/Map.gab',
  'std/Pry.gab',
  'std/Test.gab',
  'std/Record.gab',
  install_dir: '/usr/local/share/gab/std/'
  )

executable('gab',
  'src/cli/main.c',
  link_with: [libcgab],
  install: true,
)
