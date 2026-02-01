/*
MIT License

Copyright (c) 2026 Christian Luppi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

// #############################################################################
// OLIB_HEADER utilities
// #############################################################################

#ifdef __cplusplus
#  define OLIB_HEADER_BEGIN extern "C" {
#  define OLIB_HEADER_END   }
#else
#  define OLIB_HEADER_BEGIN
#  define OLIB_HEADER_END
#endif

// #############################################################################
OLIB_HEADER_BEGIN;
// #############################################################################

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #############################################################################
// OLIB_API macro for shared library symbol visibility
// #############################################################################

#if defined(OLIB_SHARED)
#  if defined(_WIN32) || defined(_WIN64)
#    if defined(OLIB_EXPORTS)
#      define OLIB_API __declspec(dllexport)
#    else
#      define OLIB_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) && __GNUC__ >= 4
#    define OLIB_API __attribute__((visibility("default")))
#  else
#    define OLIB_API
#  endif
#else
#  define OLIB_API
#endif

// #############################################################################
// Memory allocator
// #############################################################################

// Memory functions called within the library, they can be overriden by the user.
OLIB_API void* olib_malloc(size_t size);
OLIB_API void olib_free(void* ptr);
OLIB_API void* olib_calloc(size_t num, size_t size);
OLIB_API void* olib_realloc(void* ptr, size_t new_size);

// Function pointer types for custom memory functions.
typedef void* (*olib_malloc_fn)(size_t size);
typedef void (*olib_free_fn)(void* ptr);
typedef void* (*olib_calloc_fn)(size_t num, size_t size);
typedef void* (*olib_realloc_fn)(void* ptr, size_t new_size);

// Set custom memory functions to be used by the library.
// Not thread safe, should be called only once at the start of the program.
OLIB_API void olib_set_memory_fns(
    olib_malloc_fn malloc_fn,
    olib_free_fn free_fn,
    olib_calloc_fn calloc_fn,
    olib_realloc_fn realloc_fn);

// #############################################################################
OLIB_HEADER_END;
// #############################################################################