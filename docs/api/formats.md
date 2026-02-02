---
title: Formats Module
---

# Formats Module

The formats module (`olib/olib_formats.h`) defines the built-in serialization formats and their serializer constructors.

## Overview

olib includes ready-to-use serializers for common data formats. This module provides an enum to identify formats and factory functions to create serializer instances.

## Format Enum

```c
typedef enum olib_format_t {
    OLIB_FORMAT_JSON_TEXT,    // Human-readable JSON
    OLIB_FORMAT_JSON_BINARY,  // Compact binary JSON (MessagePack-like)
    OLIB_FORMAT_YAML,         // YAML format
    OLIB_FORMAT_XML,          // XML format
    OLIB_FORMAT_BINARY,       // Compact binary encoding
    OLIB_FORMAT_TOML,         // TOML format
    OLIB_FORMAT_TXT,          // Plain text (simple key=value)
    OLIB_FORMAT_MAX,          // Number of formats (for iteration)
} olib_format_t;
```

## Format Characteristics

| Format | Type | Description | Best For |
|--------|------|-------------|----------|
| JSON Text | Text | Human-readable JSON | Config files, APIs, debugging |
| JSON Binary | Binary | Compact binary representation | Network protocols, storage |
| YAML | Text | Human-friendly markup | Config files, documents |
| XML | Text | Extensible Markup Language | Legacy systems, documents |
| TOML | Text | Minimal config language | Config files |
| TXT | Text | Simple key=value format | Simple configs, debugging |
| Binary | Binary | Compact binary encoding | Performance, minimal size |

## Serializer Constructors

Each function creates a new serializer instance. The caller must free it with `olib_serializer_free()`.

### `olib_serializer_new_json_text`

Create a JSON text serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_json_text();
```

**Output format:**
```json
{
  "name": "Alice",
  "age": 30,
  "tags": ["developer", "musician"]
}
```

### `olib_serializer_new_json_binary`

Create a binary JSON serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_json_binary();
```

**Notes:** Produces compact binary output. Not human-readable but more efficient for storage and transmission.

### `olib_serializer_new_yaml`

Create a YAML serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_yaml();
```

**Output format:**
```yaml
name: Alice
age: 30
tags:
  - developer
  - musician
```

### `olib_serializer_new_xml`

Create an XML serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_xml();
```

**Output format:**
```xml
<root>
  <name>Alice</name>
  <age>30</age>
  <tags>
    <item>developer</item>
    <item>musician</item>
  </tags>
</root>
```

### `olib_serializer_new_toml`

Create a TOML serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_toml();
```

**Output format:**
```toml
name = "Alice"
age = 30
tags = ["developer", "musician"]
```

### `olib_serializer_new_txt`

Create a plain text serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_txt();
```

**Output format:**
```
name = Alice
age = 30
```

**Notes:** Simple format for basic key-value data. Limited support for nested structures.

### `olib_serializer_new_binary`

Create a compact binary serializer.

**Signature:**
```c
olib_serializer_t* olib_serializer_new_binary();
```

**Notes:** Produces minimal binary output. Best for performance-critical applications.

## Usage Example

```c
#include <olib.h>

int main(void) {
    // Create an object
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(name, "Test");
    olib_object_struct_set(obj, "name", name);

    // Serialize with different formats
    olib_serializer_t* json_ser = olib_serializer_new_json_text();
    olib_serializer_t* yaml_ser = olib_serializer_new_yaml();

    char* json = NULL;
    char* yaml = NULL;

    olib_serializer_write_string(json_ser, obj, &json);
    olib_serializer_write_string(yaml_ser, obj, &yaml);

    printf("JSON:\n%s\n\n", json);
    printf("YAML:\n%s\n", yaml);

    // Cleanup
    olib_free(json);
    olib_free(yaml);
    olib_serializer_free(json_ser);
    olib_serializer_free(yaml_ser);
    olib_object_free(obj);

    return 0;
}
```

## Choosing a Format

**Use JSON Text when:**
- Interoperating with web APIs
- Human readability is important
- Debugging or logging

**Use YAML when:**
- Configuration files for humans to edit
- Complex nested data with good readability

**Use TOML when:**
- Simple configuration files
- Flat or shallow data structures

**Use XML when:**
- Integrating with legacy systems
- Document-oriented data

**Use Binary formats when:**
- Performance is critical
- Storage size matters
- Network transmission efficiency
- Data doesn't need to be human-readable
