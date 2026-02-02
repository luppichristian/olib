---
title: Helpers Module
---

# Helpers Module

The helpers module (`olib/olib_helpers.h`) provides high-level convenience functions for common serialization operations.

## Overview

While you can use serializer instances directly, the helper functions provide a simpler API for common tasks:

- Read/write objects using a format enum (no serializer management)
- Convert between formats in a single call
- Work with files, strings, or raw data

## Why Use This?

- **Simplicity**: No need to create and manage serializer instances
- **One-liners**: Common operations become single function calls
- **Format conversion**: Convert between formats without intermediate steps

## Write Helpers

### `olib_format_write`

Write an object to a binary buffer.

**Signature:**
```c
bool olib_format_write(olib_format_t format, olib_object_t* obj, uint8_t** out_data, size_t* out_size);
```

**Parameters:**
- `format` — The format to use
- `obj` — Object to serialize
- `out_data` — Output: allocated buffer (caller frees with `olib_free`)
- `out_size` — Output: buffer size

**Returns:** true on success

### `olib_format_write_string`

Write an object to a null-terminated string. Use with text-based formats.

**Signature:**
```c
bool olib_format_write_string(olib_format_t format, olib_object_t* obj, char** out_string);
```

**Parameters:**
- `format` — The format to use (must be text-based)
- `obj` — Object to serialize
- `out_string` — Output: allocated string (caller frees with `olib_free`)

**Returns:** true on success

**Example:**
```c
olib_object_t* obj = /* ... */;
char* json = NULL;

if (olib_format_write_string(OLIB_FORMAT_JSON_TEXT, obj, &json)) {
    printf("%s\n", json);
    olib_free(json);
}
```

### `olib_format_write_file`

Write an object to an open FILE*.

**Signature:**
```c
bool olib_format_write_file(olib_format_t format, olib_object_t* obj, FILE* file);
```

### `olib_format_write_file_path`

Write an object to a file path.

**Signature:**
```c
bool olib_format_write_file_path(olib_format_t format, olib_object_t* obj, const char* file_path);
```

**Example:**
```c
olib_object_t* config = /* ... */;

// Save as JSON
olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, config, "config.json");

// Also save as YAML
olib_format_write_file_path(OLIB_FORMAT_YAML, config, "config.yaml");
```

## Read Helpers

### `olib_format_read`

Read an object from a binary buffer.

**Signature:**
```c
olib_object_t* olib_format_read(olib_format_t format, const uint8_t* data, size_t size);
```

**Returns:** Parsed object (caller frees with `olib_object_free`), or NULL on error

### `olib_format_read_string`

Read an object from a null-terminated string. Use with text-based formats.

**Signature:**
```c
olib_object_t* olib_format_read_string(olib_format_t format, const char* string);
```

**Example:**
```c
const char* json = "{\"key\": 42}";
olib_object_t* obj = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, json);

if (obj) {
    int64_t value = olib_object_get_int(olib_object_struct_get(obj, "key"));
    printf("key = %lld\n", (long long)value);  // key = 42
    olib_object_free(obj);
}
```

### `olib_format_read_file`

Read an object from an open FILE*.

**Signature:**
```c
olib_object_t* olib_format_read_file(olib_format_t format, FILE* file);
```

### `olib_format_read_file_path`

Read an object from a file path.

**Signature:**
```c
olib_object_t* olib_format_read_file_path(olib_format_t format, const char* file_path);
```

**Example:**
```c
olib_object_t* config = olib_format_read_file_path(OLIB_FORMAT_JSON_TEXT, "config.json");

if (config) {
    // Use config...
    olib_object_free(config);
}
```

## Conversion Helpers

### `olib_convert`

Convert binary data from one format to another.

**Signature:**
```c
bool olib_convert(
    olib_format_t src_format,
    const uint8_t* src_data,
    size_t src_size,
    olib_format_t dst_format,
    uint8_t** out_data,
    size_t* out_size);
```

**Parameters:**
- `src_format` — Source format
- `src_data` — Source data buffer
- `src_size` — Source data size
- `dst_format` — Destination format
- `out_data` — Output: converted data (caller frees with `olib_free`)
- `out_size` — Output: converted data size

### `olib_convert_string`

Convert between text-based formats.

**Signature:**
```c
bool olib_convert_string(
    olib_format_t src_format,
    const char* src_string,
    olib_format_t dst_format,
    char** out_string);
```

**Example:**
```c
const char* json = "{\"name\": \"Alice\", \"age\": 30}";
char* yaml = NULL;

if (olib_convert_string(OLIB_FORMAT_JSON_TEXT, json, OLIB_FORMAT_YAML, &yaml)) {
    printf("Converted to YAML:\n%s\n", yaml);
    olib_free(yaml);
}
```

### `olib_convert_file`

Convert between FILE* streams.

**Signature:**
```c
bool olib_convert_file(
    olib_format_t src_format,
    FILE* src_file,
    olib_format_t dst_format,
    FILE* dst_file);
```

### `olib_convert_file_path`

Convert between file paths.

**Signature:**
```c
bool olib_convert_file_path(
    olib_format_t src_format,
    const char* src_path,
    olib_format_t dst_format,
    const char* dst_path);
```

**Example:**
```c
// Convert JSON config to YAML
olib_convert_file_path(
    OLIB_FORMAT_JSON_TEXT, "config.json",
    OLIB_FORMAT_YAML, "config.yaml");

// Convert TOML to JSON
olib_convert_file_path(
    OLIB_FORMAT_TOML, "settings.toml",
    OLIB_FORMAT_JSON_TEXT, "settings.json");
```

## Getting a Serializer

### `olib_format_serializer`

Get a serializer instance for a format. Useful when you need the serializer directly.

**Signature:**
```c
olib_serializer_t* olib_format_serializer(olib_format_t format);
```

**Returns:** New serializer (caller must free with `olib_serializer_free`)

**Example:**
```c
olib_serializer_t* ser = olib_format_serializer(OLIB_FORMAT_JSON_TEXT);

// Use serializer...

olib_serializer_free(ser);
```

## Complete Example

```c
#include <olib.h>
#include <stdio.h>

int main(void) {
    // Parse JSON string
    const char* input = "{\"message\": \"Hello\", \"count\": 42}";
    olib_object_t* obj = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);

    if (!obj) {
        fprintf(stderr, "Failed to parse JSON\n");
        return 1;
    }

    // Modify the object
    olib_object_t* extra = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
    olib_object_set_bool(extra, true);
    olib_object_struct_set(obj, "modified", extra);

    // Output in multiple formats
    char* json = NULL;
    char* yaml = NULL;
    char* toml = NULL;

    olib_format_write_string(OLIB_FORMAT_JSON_TEXT, obj, &json);
    olib_format_write_string(OLIB_FORMAT_YAML, obj, &yaml);
    olib_format_write_string(OLIB_FORMAT_TOML, obj, &toml);

    printf("=== JSON ===\n%s\n\n", json);
    printf("=== YAML ===\n%s\n\n", yaml);
    printf("=== TOML ===\n%s\n", toml);

    // Save to files
    olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, obj, "output.json");
    olib_format_write_file_path(OLIB_FORMAT_YAML, obj, "output.yaml");

    // Cleanup
    olib_free(json);
    olib_free(yaml);
    olib_free(toml);
    olib_object_free(obj);

    return 0;
}
```
