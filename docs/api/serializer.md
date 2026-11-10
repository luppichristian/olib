# Serializer Module

The serializer module (`olib/olib_serializer.h`) defines the serializer interface for converting objects to/from various formats.

## Overview

A serializer converts olib objects to a data format (writing) and parses data back into objects (reading). olib provides built-in serializers for common formats, and you can implement custom serializers.

## Why Use This?

- **Custom Formats**: Implement serializers for proprietary or specialized formats
- **Extended Features**: Add format-specific options not covered by built-in serializers
- **Integration**: Connect olib to existing serialization systems

## Types

| Type | Description |
|------|-------------|
| `olib_serializer_t` | Opaque serializer instance |
| `olib_serializer_config_t` | Configuration struct for custom serializers |

## Serializer Lifecycle

### `olib_serializer_new`

Create a serializer from a configuration.

**Signature:**
```c
olib_serializer_t* olib_serializer_new(olib_serializer_config_t* config);
```

**Parameters:**
- `config` — Configuration with callbacks and settings

**Returns:** New serializer, or NULL on failure

### `olib_serializer_free`

Free a serializer instance.

**Signature:**
```c
void olib_serializer_free(olib_serializer_t* serializer);
```

### `olib_serializer_is_text_based`

Check if a serializer produces text output.

**Signature:**
```c
bool olib_serializer_is_text_based(olib_serializer_t* serializer);
```

**Notes:** Text-based serializers should use `_string` functions; binary serializers should use the raw data functions.

## Writing Objects

### `olib_serializer_write`

Write an object to a binary buffer. For binary serializers only.

**Signature:**
```c
bool olib_serializer_write(olib_serializer_t* serializer, olib_object_t* obj, uint8_t** out_data, size_t* out_size);
```

**Parameters:**
- `serializer` — The serializer to use
- `obj` — Object to serialize
- `out_data` — Output: pointer to allocated buffer (caller must free with `olib_free`)
- `out_size` — Output: size of the buffer

**Returns:** true on success

### `olib_serializer_write_string`

Write an object to a null-terminated string. For text-based serializers only.

**Signature:**
```c
bool olib_serializer_write_string(olib_serializer_t* serializer, olib_object_t* obj, char** out_string);
```

**Parameters:**
- `serializer` — The serializer to use
- `obj` — Object to serialize
- `out_string` — Output: pointer to allocated string (caller must free with `olib_free`)

**Returns:** true on success

**Example:**
```c
olib_serializer_t* ser = olib_serializer_new_json_text();

char* json = NULL;
if (olib_serializer_write_string(ser, obj, &json)) {
    printf("%s\n", json);
    olib_free(json);
}

olib_serializer_free(ser);
```

### `olib_serializer_write_file`

Write an object to an open FILE*.

**Signature:**
```c
bool olib_serializer_write_file(olib_serializer_t* serializer, olib_object_t* obj, FILE* file);
```

### `olib_serializer_write_file_path`

Write an object to a file path.

**Signature:**
```c
bool olib_serializer_write_file_path(olib_serializer_t* serializer, olib_object_t* obj, const char* file_path);
```

**Notes:** Opens the file in the appropriate mode (text or binary) based on serializer type.

## Reading Objects

### `olib_serializer_read`

Read an object from a binary buffer. For binary serializers only.

**Signature:**
```c
olib_object_t* olib_serializer_read(olib_serializer_t* serializer, const uint8_t* data, size_t size);
```

**Returns:** Parsed object (caller must free with `olib_object_free`), or NULL on error

### `olib_serializer_read_string`

Read an object from a null-terminated string. For text-based serializers only.

**Signature:**
```c
olib_object_t* olib_serializer_read_string(olib_serializer_t* serializer, const char* string);
```

**Example:**
```c
olib_serializer_t* ser = olib_serializer_new_json_text();

const char* json = "{\"name\": \"Alice\", \"age\": 30}";
olib_object_t* obj = olib_serializer_read_string(ser, json);

if (obj) {
    // Use obj...
    olib_object_free(obj);
}

olib_serializer_free(ser);
```

### `olib_serializer_read_file`

Read an object from an open FILE*.

**Signature:**
```c
olib_object_t* olib_serializer_read_file(olib_serializer_t* serializer, FILE* file);
```

### `olib_serializer_read_file_path`

Read an object from a file path.

**Signature:**
```c
olib_object_t* olib_serializer_read_file_path(olib_serializer_t* serializer, const char* file_path);
```

## Custom Serializer Configuration

The `olib_serializer_config_t` structure defines callbacks for implementing custom serializers:

```c
typedef struct olib_serializer_config_t {
    void* user_data;          // Custom context data
    bool text_based;          // true for text formats, false for binary

    // Lifecycle
    void (*init_ctx)(void* ctx);
    void (*free_ctx)(void* ctx);
    bool (*init_write)(void* ctx);
    bool (*finish_write)(void* ctx, uint8_t** out_data, size_t* out_size);
    bool (*init_read)(void* ctx, const uint8_t* data, size_t size);
    bool (*finish_read)(void* ctx);

    // Write callbacks
    bool (*write_int)(void* ctx, int64_t value);
    bool (*write_uint)(void* ctx, uint64_t value);
    bool (*write_float)(void* ctx, double value);
    bool (*write_string)(void* ctx, const char* value);
    bool (*write_bool)(void* ctx, bool value);
    bool (*write_list_begin)(void* ctx, size_t size);
    bool (*write_list_end)(void* ctx);
    bool (*write_struct_begin)(void* ctx);
    bool (*write_struct_key)(void* ctx, const char* key);
    bool (*write_struct_end)(void* ctx);

    // Read callbacks
    olib_object_type_t (*read_peek)(void* ctx);
    bool (*read_int)(void* ctx, int64_t* value);
    bool (*read_uint)(void* ctx, uint64_t* value);
    bool (*read_float)(void* ctx, double* value);
    bool (*read_string)(void* ctx, const char** value);
    bool (*read_bool)(void* ctx, bool* value);
    bool (*read_list_begin)(void* ctx, size_t* size);
    bool (*read_list_end)(void* ctx);
    bool (*read_struct_begin)(void* ctx);
    bool (*read_struct_key)(void* ctx, const char** key);
    bool (*read_struct_end)(void* ctx);
} olib_serializer_config_t;
```

### Callback Descriptions

**Lifecycle Callbacks:**
- `init_ctx`: Called after serializer creation (optional initialization)
- `free_ctx`: Called when serializer is freed
- `init_write`: Prepare for a write operation (reset buffers)
- `finish_write`: Complete write and return the output buffer
- `init_read`: Set up for reading from input data
- `finish_read`: Clean up after reading

**Write Callbacks:**
- `write_*`: Write primitive values
- `write_list_begin`: Start a list (size provided)
- `write_list_end`: End a list
- `write_struct_begin`: Start a struct
- `write_struct_key`: Write a struct key (value follows)
- `write_struct_end`: End a struct

**Read Callbacks:**
- `read_peek`: Return the type of the next value without consuming it
- `read_*`: Read primitive values
- `read_list_begin`: Start reading a list (returns size)
- `read_list_end`: Finish reading a list
- `read_struct_begin`: Start reading a struct
- `read_struct_key`: Read next key (return false when no more keys)
- `read_struct_end`: Finish reading a struct

### Custom Serializer Example

Here's a minimal example of a custom serializer that outputs debug info:

```c
#include <olib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
} debug_ctx_t;

static void debug_append(debug_ctx_t* ctx, const char* str) {
    size_t len = strlen(str);
    if (ctx->size + len >= ctx->capacity) {
        ctx->capacity = (ctx->size + len + 1) * 2;
        ctx->buffer = olib_realloc(ctx->buffer, ctx->capacity);
    }
    memcpy(ctx->buffer + ctx->size, str, len + 1);
    ctx->size += len;
}

static void debug_init(void* ctx) {
    debug_ctx_t* d = (debug_ctx_t*)ctx;
    d->buffer = NULL;
    d->size = 0;
    d->capacity = 0;
}

static void debug_free(void* ctx) {
    debug_ctx_t* d = (debug_ctx_t*)ctx;
    olib_free(d->buffer);
}

static bool debug_init_write(void* ctx) {
    debug_ctx_t* d = (debug_ctx_t*)ctx;
    olib_free(d->buffer);
    d->buffer = NULL;
    d->size = 0;
    d->capacity = 0;
    return true;
}

static bool debug_finish_write(void* ctx, uint8_t** out, size_t* size) {
    debug_ctx_t* d = (debug_ctx_t*)ctx;
    *out = (uint8_t*)d->buffer;
    *size = d->size;
    d->buffer = NULL;  // Transfer ownership
    return true;
}

static bool debug_write_int(void* ctx, int64_t value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "INT(%lld)", (long long)value);
    debug_append((debug_ctx_t*)ctx, buf);
    return true;
}

static bool debug_write_string(void* ctx, const char* value) {
    debug_append((debug_ctx_t*)ctx, "STRING(\"");
    debug_append((debug_ctx_t*)ctx, value);
    debug_append((debug_ctx_t*)ctx, "\")");
    return true;
}

// ... implement other callbacks ...

olib_serializer_t* create_debug_serializer(void) {
    debug_ctx_t* ctx = olib_calloc(1, sizeof(debug_ctx_t));

    olib_serializer_config_t config = {
        .user_data = ctx,
        .text_based = true,
        .init_ctx = debug_init,
        .free_ctx = debug_free,
        .init_write = debug_init_write,
        .finish_write = debug_finish_write,
        .write_int = debug_write_int,
        .write_string = debug_write_string,
        // ... other callbacks ...
    };

    return olib_serializer_new(&config);
}
```
