# Examples

The `examples/` directory contains complete working examples demonstrating various olib features.

## Building Examples

Examples are built by default with CMake:

```bash
cmake -B build
cmake --build build
```

Executables are placed in `build/bin/`:

```bash
./build/bin/olib-example-basic
./build/bin/olib-example-json
# etc.
```

To disable building examples:

```bash
cmake -B build -DOLIB_BUILD_EXAMPLES=OFF
```

## Example Programs

### basic.c

**Purpose:** Demonstrates fundamental object creation and manipulation.

**Covers:**
- Creating value objects (int, float, string)
- Creating container objects (list, struct)
- Adding items to lists and structs
- Accessing values from objects
- Type inspection
- Memory cleanup

**Key functions used:**
- `olib_object_new()`
- `olib_object_set_*()` / `olib_object_get_*()`
- `olib_object_list_push()` / `olib_object_list_get()`
- `olib_object_struct_set()` / `olib_object_struct_get()`
- `olib_object_free()`

[View source](../examples/basic.c)

### json.c

**Purpose:** Demonstrates JSON serialization and parsing.

**Covers:**
- Creating a JSON serializer
- Parsing JSON from a string
- Accessing parsed data
- Modifying parsed objects
- Serializing objects back to JSON

**Key functions used:**
- `olib_serializer_new_json_text()`
- `olib_serializer_read_string()`
- `olib_serializer_write_string()`
- `olib_serializer_free()`

[View source](../examples/json.c)

### yaml.c

**Purpose:** Demonstrates YAML format handling.

**Covers:**
- Loading YAML files
- YAML parsing
- Writing YAML output

**Key functions used:**
- `olib_serializer_new_yaml()`
- `olib_serializer_read_file_path()`
- `olib_serializer_write_string()`

[View source](../examples/yaml.c)

### xml.c

**Purpose:** Demonstrates XML format handling.

**Covers:**
- XML serialization
- XML parsing
- Working with XML document structure

**Key functions used:**
- `olib_serializer_new_xml()`
- `olib_serializer_read_string()`
- `olib_serializer_write_file_path()`

[View source](../examples/xml.c)

### toml.c

**Purpose:** Demonstrates TOML configuration file handling.

**Covers:**
- TOML parsing
- TOML serialization
- Configuration file workflows

**Key functions used:**
- `olib_serializer_new_toml()`
- `olib_format_read_file_path()`
- `olib_format_write_string()`

[View source](../examples/toml.c)

### text.c

**Purpose:** Demonstrates plain text format handling.

**Covers:**
- Simple key-value text format
- Text parsing and generation

**Key functions used:**
- `olib_serializer_new_txt()`
- `olib_format_write_string()`

[View source](../examples/text.c)

### binary.c

**Purpose:** Demonstrates compact binary serialization.

**Covers:**
- Binary serialization for efficiency
- Reading/writing binary data
- Comparing binary vs text sizes

**Key functions used:**
- `olib_serializer_new_binary()`
- `olib_serializer_write()`
- `olib_serializer_read()`

[View source](../examples/binary.c)

## Common Patterns

### Error Handling

All examples follow this pattern:

```c
olib_object_t* obj = olib_format_read_file_path(OLIB_FORMAT_JSON_TEXT, "file.json");
if (!obj) {
    fprintf(stderr, "Failed to parse file\n");
    return 1;
}

// Use obj...

olib_object_free(obj);
```

### Memory Management

Objects added to containers are owned by the container:

```c
olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
olib_object_t* item = olib_object_new(OLIB_OBJECT_TYPE_INT);
olib_object_set_int(item, 42);

olib_object_list_push(list, item);  // list now owns item

olib_object_free(list);  // frees list AND item
```

Serialized output must be freed:

```c
char* output = NULL;
olib_format_write_string(OLIB_FORMAT_JSON_TEXT, obj, &output);
// Use output...
olib_free(output);  // Use olib_free, not free()
```

### Format Conversion

Convert between formats in one step:

```c
olib_convert_file_path(
    OLIB_FORMAT_JSON_TEXT, "input.json",
    OLIB_FORMAT_YAML, "output.yaml"
);
```

Or load, modify, and save:

```c
olib_object_t* obj = olib_format_read_file_path(OLIB_FORMAT_JSON_TEXT, "config.json");

// Modify obj...

olib_format_write_file_path(OLIB_FORMAT_YAML, obj, "config.yaml");
olib_object_free(obj);
```

## Sample Data Files

The `samples/` directory contains example data files used by the examples:

- `example1.json`, `example2.json` - JSON samples
- `example1.yaml`, `example2.yaml` - YAML samples
- `example1.toml`, `example2.toml` - TOML samples

These files demonstrate various data structures and can be used to test format conversion.
