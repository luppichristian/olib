# olib

[![Release](https://img.shields.io/github/v/release/luppichristian/olib)](https://github.com/luppichristian/olib/releases)
[![Language](https://img.shields.io/badge/language-C11-555555)](https://github.com/luppichristian/olib)
[![CI](https://img.shields.io/github/actions/workflow/status/luppichristian/olib/ci.yml?branch=main&label=ci)](https://github.com/luppichristian/olib/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

olib is a C library for building structured data, serializing it, and converting between formats. It provides an object tree, built-in serializers, and the `olib-convert` command-line tool.

Objects can contain structs, lists, signed and unsigned integers, floating-point numbers, strings, and booleans. The library includes serializers for JSON, YAML, XML, TOML, a simple key-value text format, binary JSON, and a compact binary format. You can supply custom allocators and serializers; the public API also supports C++ callers.

Formats have different representations and limitations, so conversion is not necessarily lossless. In particular, the object model has no null type, and the plain text format has limited support for nested structures.

## Build

You need CMake 3.19 or later and C and C++ compilers. The library uses C11; the tests use C++20 and GoogleTest. By default CMake fetches GoogleTest when tests are enabled.

```sh
git clone https://github.com/luppichristian/olib.git
cd olib
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake builds a static library, examples, tests, and the converter by default. To build without fetching GoogleTest, disable tests with `-DOLIB_BUILD_TESTS=OFF`, or set `-DOLIB_GTEST_FETCH=OFF` to use an installed GoogleTest package.

| CMake option | Default | Purpose |
| --- | --- | --- |
| `OLIB_BUILD_SHARED` | `OFF` | Build a shared library instead of a static library. |
| `OLIB_BUILD_TESTS` | `ON` | Build the test suite. |
| `OLIB_BUILD_EXAMPLES` | `ON` | Build the example programs. |
| `OLIB_BUILD_CLI` | `ON` | Build `olib-convert`. |
| `OLIB_GTEST_FETCH` | `ON` | Fetch GoogleTest when tests are enabled. |

To use the source tree in another CMake project, add it as a subdirectory and link the target. This example uses the default static build:

```cmake
set(OLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/olib)
target_link_libraries(your_target PRIVATE olib::static)
```

## Library usage

The snippets below use `#include <olib.h>` and, where they print output, `#include <stdio.h>`. They omit error checks to keep the main operations visible: check object pointers and boolean return values in your own code.

### Create and serialize an object

```c
olib_object_t* person = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
olib_object_set_string(name, "Alice");
olib_object_struct_set(person, "name", name);

char* json = NULL;
olib_format_write_string(OLIB_FORMAT_JSON_TEXT, person, &json);
puts(json);

olib_free(json);
olib_object_free(person);
```

A successful insertion transfers ownership of `name` to `person`. Freeing the parent frees its children; if insertion fails, the caller still owns the child. Free serialized output with `olib_free()`.

### Read an object and write a file

```c
const char* json = "{\"name\": \"Alice\", \"age\": 30}";
olib_object_t* person = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, json);

olib_object_t* name = olib_object_struct_get(person, "name");
printf("Name: %s\n", olib_object_get_string(name));

olib_format_write_file_path(OLIB_FORMAT_YAML, person, "person.yaml");
olib_object_free(person);
```

The child returned by `olib_object_struct_get()` belongs to its parent. `olib_object_get_string()` returns a borrowed pointer that remains valid until the object is modified or freed. Check that a field exists and has the expected type before using it.

### Convert a string

```c
const char* json = "{\"name\": \"Alice\", \"age\": 30}";
char* yaml = NULL;

olib_convert_string(OLIB_FORMAT_JSON_TEXT, json, OLIB_FORMAT_YAML, &yaml);
puts(yaml);
olib_free(yaml);
```

### Convert a file

```c
bool converted = olib_convert_file_path(
    OLIB_FORMAT_JSON_TEXT, "samples/example1.json",
    OLIB_FORMAT_XML, "output.xml"
);
```

Run this example from the repository root to use the sample file. The return value indicates whether conversion succeeded.

### Work with binary data

String helpers use text formats. For binary formats, use a buffer and its size:

```c
olib_object_t* original = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, "{\"value\": 42}");
uint8_t* data = NULL;
size_t size = 0;

olib_format_write(OLIB_FORMAT_BINARY, original, &data, &size);
olib_object_t* copy = olib_format_read(OLIB_FORMAT_BINARY, data, size);

olib_object_free(copy);
olib_free(data);
olib_object_free(original);
```

For conversion involving binary formats, `olib_convert()` accepts buffers and sizes.

The [examples](examples/) directory contains complete programs for each built-in format. See the [documentation](docs/index.md) for the API reference and custom serializers.

## Command-line conversion

`olib-convert` reads an input file and writes an output file:

```sh
olib-convert [options] <input-file> <output-file>
olib-convert samples/example1.json output.yaml
olib-convert -i json -o xml input.txt output.txt
```

The tool detects formats from file extensions when options are omitted. Recognized extensions are `.json`, `.jsonb`, `.yaml`, `.yml`, `.xml`, `.toml`, `.txt`, and `.bin`. An unrecognized extension defaults to binary; a path without an extension requires an explicit format.

| Option | Purpose |
| --- | --- |
| `-i`, `--input-format <format>` | Set the input format. |
| `-o`, `--output-format <format>` | Set the output format. |
| `-h`, `--help` | Show usage. |
| `-v`, `--version` | Show version information. |

Format names are `json`, `json-binary`, `yaml`, `xml`, `toml`, `txt`, and `binary`. Run the executable from the build's `bin` directory (or its configuration subdirectory with a multi-configuration generator).

## License

[MIT](LICENSE) © 2026 Christian Luppi
