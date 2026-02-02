---
title: Getting Started
---

# Getting Started

This guide covers installation, basic concepts, and your first olib program.

## Installation

### Building from Source

```bash
# Clone the repository
git clone https://github.com/user/olib.git
cd olib

# Configure with CMake
cmake -B build

# Build
cmake --build build

# Run tests (optional)
ctest --test-dir build

# Install (may require sudo on Linux/macOS)
cmake --install build --prefix /usr/local
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `OLIB_BUILD_SHARED` | `OFF` | Build as shared library instead of static |
| `OLIB_BUILD_TESTS` | `ON` | Build the test suite |
| `OLIB_BUILD_EXAMPLES` | `ON` | Build example programs |
| `OLIB_GTEST_FETCH` | `ON` | Auto-fetch GoogleTest for testing |

Example with custom options:

```bash
cmake -B build -DOLIB_BUILD_SHARED=ON -DOLIB_BUILD_TESTS=OFF
```

### Using in Your Project

#### With CMake

```cmake
find_package(olib REQUIRED)
target_link_libraries(your_target PRIVATE olib::static)
```

#### Manual Linking

```bash
gcc -I/usr/local/include your_program.c -L/usr/local/lib -lolib -o your_program
```

## Basic Concepts

### Objects

olib represents all data as objects (`olib_object_t`). Each object has a type:

| Type | Description | C Type |
|------|-------------|--------|
| `OLIB_OBJECT_TYPE_STRUCT` | Key-value container (ordered) | - |
| `OLIB_OBJECT_TYPE_LIST` | Ordered collection | - |
| `OLIB_OBJECT_TYPE_INT` | Signed 64-bit integer | `int64_t` |
| `OLIB_OBJECT_TYPE_UINT` | Unsigned 64-bit integer | `uint64_t` |
| `OLIB_OBJECT_TYPE_FLOAT` | 64-bit floating point | `double` |
| `OLIB_OBJECT_TYPE_STRING` | UTF-8 string | `const char*` |
| `OLIB_OBJECT_TYPE_BOOL` | Boolean | `bool` |

### Object Ownership

When you add an object to a container (list or struct), the container takes ownership:

```c
olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
olib_object_t* item = olib_object_new(OLIB_OBJECT_TYPE_INT);
olib_object_set_int(item, 42);

olib_object_list_push(list, item);  // list now owns item

olib_object_free(list);  // frees list AND item
// Do NOT call olib_object_free(item) - already freed!
```

### Serializers

Serializers convert objects to/from data formats. olib provides built-in serializers for common formats:

- **Text-based**: JSON, YAML, XML, TOML, TXT
- **Binary**: JSON binary, compact binary

### Formats vs Serializers

**Format** (`olib_format_t`): An enum identifying a built-in format

**Serializer** (`olib_serializer_t*`): An instance that performs actual serialization

For built-in formats, use the helper functions that accept `olib_format_t`:

```c
// Easy way - use format helpers
olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, obj, "out.json");
```

For custom serializers or more control, create a serializer instance:

```c
// More control - use serializer directly
olib_serializer_t* ser = olib_serializer_new_json_text();
olib_serializer_write_file_path(ser, obj, "out.json");
olib_serializer_free(ser);
```

## Your First Program

Here's a complete program that creates an object and serializes it:

```c
#include <olib.h>
#include <stdio.h>

int main(void) {
    // Create a struct to hold our data
    olib_object_t* config = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    // Add a string field
    olib_object_t* app_name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(app_name, "MyApp");
    olib_object_struct_set(config, "name", app_name);

    // Add a version number
    olib_object_t* version = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    olib_object_set_float(version, 1.0);
    olib_object_struct_set(config, "version", version);

    // Add a list of features
    olib_object_t* features = olib_object_new(OLIB_OBJECT_TYPE_LIST);

    olib_object_t* f1 = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(f1, "fast");
    olib_object_list_push(features, f1);

    olib_object_t* f2 = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(f2, "lightweight");
    olib_object_list_push(features, f2);

    olib_object_struct_set(config, "features", features);

    // Serialize to JSON
    char* json = NULL;
    if (olib_format_write_string(OLIB_FORMAT_JSON_TEXT, config, &json)) {
        printf("JSON output:\n%s\n", json);
        olib_free(json);
    }

    // Write to YAML file
    if (olib_format_write_file_path(OLIB_FORMAT_YAML, config, "config.yaml")) {
        printf("Saved to config.yaml\n");
    }

    // Cleanup
    olib_object_free(config);
    return 0;
}
```

Build and run:

```bash
gcc -I/path/to/olib/include example.c -L/path/to/olib/lib -lolib -o example
./example
```

## Next Steps

- Read the [Object Module](api/object.md) documentation for all object operations
- See [Examples](examples.md) for more complete programs
- Learn about [Custom Serializers](api/serializer.md) for implementing new formats
