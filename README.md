# olib

A lightweight C library for object serialization and format conversion.

olib provides a unified API for working with structured data objects and serializing them to multiple formats including JSON, YAML, XML, TOML, and binary representations. It features a clean, consistent interface with support for custom memory allocators and extensible serializer implementations.

## Features

- **Unified Object Model**: Work with structs, lists, and primitive types (int, uint, float, string, bool) through a consistent API
- **Multi-Format Support**: Built-in serializers for JSON (text/binary), YAML, XML, TOML, TXT, and compact binary formats
- **Format Conversion**: Convert between any supported formats with a single function call
- **Custom Memory Management**: Override memory allocation functions for embedded systems or custom allocators
- **Extensible Serializers**: Implement custom serializers by providing callback functions
- **C/C++ Compatible**: Clean C11 API with proper C++ linkage support

## Requirements

- CMake 3.19+
- C11 compiler (GCC, Clang, MSVC)
- GoogleTest (optional, for tests - auto-fetched by default)

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `OLIB_BUILD_SHARED` | `OFF` | Build as shared library |
| `OLIB_BUILD_TESTS` | `ON` | Build test suite |
| `OLIB_BUILD_EXAMPLES` | `ON` | Build example programs |
| `OLIB_GTEST_FETCH` | `ON` | Auto-fetch GoogleTest if not found |

## Documentation

See [docs](https://luppichristian.github.io/olib/) for comprehensive API documentation and usage patterns.

## Usage

### Basic Object Manipulation

```c
#include <olib.h>

int main(void) {
    // Create a struct object
    olib_object_t* person = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    // Add a string field
    olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(name, "Alice");
    olib_object_struct_set(person, "name", name);

    // Add an integer field
    olib_object_t* age = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(age, 30);
    olib_object_struct_set(person, "age", age);

    // Access values
    printf("Name: %s\n", olib_object_get_string(olib_object_struct_get(person, "name")));
    printf("Age: %lld\n", (long long)olib_object_get_int(olib_object_struct_get(person, "age")));

    // Cleanup
    olib_object_free(person);
    return 0;
}
```

### Serialization

```c
#include <olib.h>

int main(void) {
    // Create an object
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    // ... populate object ...

    // Serialize to JSON string
    char* json = NULL;
    olib_format_write_string(OLIB_FORMAT_JSON_TEXT, obj, &json);
    printf("%s\n", json);
    olib_free(json);

    // Or write directly to file
    olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, obj, "output.json");

    olib_object_free(obj);
    return 0;
}
```

### Format Conversion

```c
#include <olib.h>

int main(void) {
    // Convert JSON file to YAML
    olib_convert_file_path(
        OLIB_FORMAT_JSON_TEXT, "input.json",
        OLIB_FORMAT_YAML, "output.yaml"
    );

    // Convert string between formats
    const char* json = "{\"key\": \"value\"}";
    char* yaml = NULL;
    olib_convert_string(OLIB_FORMAT_JSON_TEXT, json, OLIB_FORMAT_YAML, &yaml);
    printf("%s\n", yaml);
    olib_free(yaml);

    return 0;
}
```

## License

MIT License - see [LICENSE](LICENSE)

Copyright (c) 2026 Christian Luppi
