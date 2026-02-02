---
title: Base Module
---

# Base Module

The base module (`olib/olib_base.h`) provides memory allocation functions and C/C++ compatibility macros.

## Overview

This module defines:

- Custom memory allocation interface for integration with custom allocators
- Macros for shared library symbol visibility
- C/C++ header compatibility macros

## Why Use This?

- **Custom Allocators**: Integrate olib with your memory management system (pools, arenas, tracking allocators)
- **Embedded Systems**: Use custom allocation for constrained environments
- **Debugging**: Hook allocations for leak detection or profiling

## Memory Functions

### `olib_malloc`

Allocate memory. Default implementation calls standard `malloc`.

**Signature:**
```c
void* olib_malloc(size_t size);
```

**Parameters:**
- `size` — Number of bytes to allocate

**Returns:** Pointer to allocated memory, or NULL on failure

### `olib_free`

Free previously allocated memory. Default implementation calls standard `free`.

**Signature:**
```c
void olib_free(void* ptr);
```

**Parameters:**
- `ptr` — Pointer to memory to free (can be NULL)

### `olib_calloc`

Allocate and zero-initialize memory. Default implementation calls standard `calloc`.

**Signature:**
```c
void* olib_calloc(size_t num, size_t size);
```

**Parameters:**
- `num` — Number of elements
- `size` — Size of each element

**Returns:** Pointer to zero-initialized memory, or NULL on failure

### `olib_realloc`

Resize previously allocated memory. Default implementation calls standard `realloc`.

**Signature:**
```c
void* olib_realloc(void* ptr, size_t new_size);
```

**Parameters:**
- `ptr` — Pointer to existing allocation (can be NULL)
- `new_size` — New size in bytes

**Returns:** Pointer to resized memory, or NULL on failure

## Custom Allocator Setup

### `olib_set_memory_fns`

Replace the default memory functions with custom implementations.

**Signature:**
```c
void olib_set_memory_fns(
    olib_malloc_fn malloc_fn,
    olib_free_fn free_fn,
    olib_calloc_fn calloc_fn,
    olib_realloc_fn realloc_fn);
```

**Parameters:**
- `malloc_fn` — Custom malloc function
- `free_fn` — Custom free function
- `calloc_fn` — Custom calloc function
- `realloc_fn` — Custom realloc function

**Notes:**
- Call once at program startup, before any olib functions
- Not thread-safe; do not call while other threads use olib
- All four functions must be provided

**Example:**
```c
#include <olib.h>

// Custom allocator tracking total allocations
static size_t total_allocated = 0;

void* my_malloc(size_t size) {
    total_allocated += size;
    return malloc(size);
}

void my_free(void* ptr) {
    free(ptr);
}

void* my_calloc(size_t num, size_t size) {
    total_allocated += num * size;
    return calloc(num, size);
}

void* my_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

int main(void) {
    // Set custom allocators before using olib
    olib_set_memory_fns(my_malloc, my_free, my_calloc, my_realloc);

    // Now use olib normally...
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_free(obj);

    printf("Total allocated: %zu bytes\n", total_allocated);
    return 0;
}
```

## Function Pointer Types

```c
typedef void* (*olib_malloc_fn)(size_t size);
typedef void (*olib_free_fn)(void* ptr);
typedef void* (*olib_calloc_fn)(size_t num, size_t size);
typedef void* (*olib_realloc_fn)(void* ptr, size_t new_size);
```

## Macros

### `OLIB_API`

Applied to all public API functions for proper symbol visibility in shared libraries.

- Windows: `__declspec(dllexport)` or `__declspec(dllimport)`
- GCC/Clang: `__attribute__((visibility("default")))`
- Static builds: No decoration

### `OLIB_HEADER_BEGIN` / `OLIB_HEADER_END`

Wraps header content for C++ compatibility:

```c
OLIB_HEADER_BEGIN;  // extern "C" { in C++

// C declarations here

OLIB_HEADER_END;    // } in C++
```

## Defines

| Define | Description |
|--------|-------------|
| `OLIB_SHARED` | Define when building/using olib as a shared library |
| `OLIB_EXPORTS` | Define when building olib shared library (sets dllexport) |
