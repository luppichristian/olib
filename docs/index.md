# olib Documentation

Welcome to the olib documentation. olib is a lightweight C library for object serialization and format conversion.

## Overview

olib provides a unified API for:

- **Structured Data Objects**: Create and manipulate objects with structs, lists, and primitive types
- **Multi-Format Serialization**: Serialize objects to JSON, YAML, XML, TOML, and binary formats
- **Format Conversion**: Convert between any supported formats with a single function call

## Quick Navigation

### Getting Started

- [Getting Started Guide](getting-started.md) - Installation, basic concepts, and your first program

### API Reference

- [Base Module](api/base.md) - Memory allocation and C/C++ compatibility
- [Object Module](api/object.md) - Object types and manipulation
- [Serializer Module](api/serializer.md) - Serializer interface and custom implementations
- [Formats Module](api/formats.md) - Built-in format serializers
- [Helpers Module](api/helpers.md) - High-level read/write/convert functions

### Examples

- [Examples Guide](examples.md) - Walkthrough of example programs

## Quick Example

```c
#include <olib.h>

int main(void) {
    // Create an object
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(name, "olib");
    olib_object_struct_set(obj, "name", name);

    // Serialize to JSON
    char* json = NULL;
    olib_format_write_string(OLIB_FORMAT_JSON_TEXT, obj, &json);
    printf("%s\n", json);  // {"name": "olib"}

    // Cleanup
    olib_free(json);
    olib_object_free(obj);
    return 0;
}
```

## Supported Formats

| Format | Type | Enum Value |
|--------|------|------------|
| JSON (text) | Text | `OLIB_FORMAT_JSON_TEXT` |
| JSON (binary) | Binary | `OLIB_FORMAT_JSON_BINARY` |
| YAML | Text | `OLIB_FORMAT_YAML` |
| XML | Text | `OLIB_FORMAT_XML` |
| TOML | Text | `OLIB_FORMAT_TOML` |
| Plain Text | Text | `OLIB_FORMAT_TXT` |
| Binary | Binary | `OLIB_FORMAT_BINARY` |

## License

MIT License - Copyright (c) 2026 Christian Luppi
