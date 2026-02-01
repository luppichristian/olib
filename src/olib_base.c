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

#include <olib/olib_base.h>
#include <stdlib.h>

// Stored function pointers for memory functions
// They default to the standard library functions
static olib_malloc_fn g_malloc_fn = malloc;
static olib_free_fn g_free_fn = free;
static olib_calloc_fn g_calloc_fn = calloc;
static olib_realloc_fn g_realloc_fn = realloc;

OLIB_API void* olib_malloc(size_t size) {
    return g_malloc_fn(size);
}

OLIB_API void  olib_free(void* ptr) {
    g_free_fn(ptr);
}

OLIB_API void* olib_calloc(size_t num, size_t size) {
    return g_calloc_fn(num, size);
}

OLIB_API void* olib_realloc(void* ptr, size_t new_size) {
    return g_realloc_fn(ptr, new_size);
}

OLIB_API void olib_set_memory_fns(
  olib_malloc_fn malloc_fn, 
  olib_free_fn free_fn, 
  olib_calloc_fn calloc_fn, 
  olib_realloc_fn realloc_fn) {
    if (malloc_fn) {
        g_malloc_fn = malloc_fn;
    }
    if (free_fn) {
        g_free_fn = free_fn;
    }
    if (calloc_fn) {
        g_calloc_fn = calloc_fn;
    }
    if (realloc_fn) {
        g_realloc_fn = realloc_fn;
    }
  }