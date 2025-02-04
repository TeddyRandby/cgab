#ifndef GAB_PLATFORM_H
#define GAB_PLATFORM_H

/**
 * C11 Threads are not well supported cross-platforms.
 *
 * Even the c-standard-required feature macro __STDC_NOTHREADS__
 * isn't well supported. 
 *
 * Instead of mucking about, just check for the standard threads with
 * a good 'ol __has_include.
 *
 * As of 2025, I believe only linux GNU is shipping this (at least in zig's cross compiling toolchain).
 *
 * In the other cases, use our vendored, cthreads submodule.
 */
#if __has_include("threads.h")
#include <threads.h>
#else
#include <cthreads.h>
#endif

#ifdef GAB_PLATFORM_UNIX
#include <unistd.h>

#define gab_fisatty(f) isatty(fileno(f))

#elifdef GAB_PLATFORM_WIN
#include <io.h>

#define gab_fisatty(f) _isatty(_fileno(f))

#endif

#endif
