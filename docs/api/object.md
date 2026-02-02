---
title: Object Module
---

# Object Module

The object module (`olib/olib_object.h`) defines the core object types and manipulation functions.

## Overview

All data in olib is represented as objects (`olib_object_t`). Objects can be:

- **Value types**: int, uint, float, string, bool
- **Container types**: struct, list

## Types

| Type | Enum | Description |
|------|------|-------------|
| Struct | `OLIB_OBJECT_TYPE_STRUCT` | Ordered key-value map |
| List | `OLIB_OBJECT_TYPE_LIST` | Ordered collection |
| Int | `OLIB_OBJECT_TYPE_INT` | Signed 64-bit integer |
| Uint | `OLIB_OBJECT_TYPE_UINT` | Unsigned 64-bit integer |
| Float | `OLIB_OBJECT_TYPE_FLOAT` | 64-bit floating point |
| String | `OLIB_OBJECT_TYPE_STRING` | UTF-8 null-terminated string |
| Bool | `OLIB_OBJECT_TYPE_BOOL` | Boolean (true/false) |

## Object Lifecycle

### `olib_object_new`

Create a new object of the specified type. Value objects are zero-initialized.

**Signature:**
```c
olib_object_t* olib_object_new(olib_object_type_t type);
```

**Parameters:**
- `type` — The object type to create

**Returns:** New object, or NULL on allocation failure

**Example:**
```c
olib_object_t* num = olib_object_new(OLIB_OBJECT_TYPE_INT);
olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
```

### `olib_object_dupe`

Create a deep copy of an object and all its contents.

**Signature:**
```c
olib_object_t* olib_object_dupe(olib_object_t* obj);
```

**Parameters:**
- `obj` — Object to duplicate

**Returns:** Deep copy of the object, or NULL on failure

### `olib_object_free`

Free an object and all objects it contains.

**Signature:**
```c
void olib_object_free(olib_object_t* obj);
```

**Parameters:**
- `obj` — Object to free (can be NULL)

**Notes:** When freeing a container, all contained objects are also freed. Do not free objects that have been added to containers.

## Type Inspection

### `olib_object_get_type`

Get the type of an object.

**Signature:**
```c
olib_object_type_t olib_object_get_type(olib_object_t* obj);
```

### `olib_object_is_type`

Check if an object is a specific type.

**Signature:**
```c
bool olib_object_is_type(olib_object_t* obj, olib_object_type_t type);
```

### `olib_object_is_value`

Check if an object is a value type (int, uint, float, string, bool).

**Signature:**
```c
bool olib_object_is_value(olib_object_t* obj);
```

### `olib_object_is_container`

Check if an object is a container type (struct, list).

**Signature:**
```c
bool olib_object_is_container(olib_object_t* obj);
```

### `olib_object_type_to_string`

Get a string representation of a type.

**Signature:**
```c
const char* olib_object_type_to_string(olib_object_type_t type);
```

**Returns:** String like "int", "string", "struct", etc.

## List Operations

### `olib_object_list_size`

Get the number of elements in a list.

**Signature:**
```c
size_t olib_object_list_size(olib_object_t* obj);
```

### `olib_object_list_get`

Get an element by index.

**Signature:**
```c
olib_object_t* olib_object_list_get(olib_object_t* obj, size_t index);
```

**Returns:** Pointer to element (owned by list), or NULL if out of bounds

### `olib_object_list_set`

Replace an element at an index.

**Signature:**
```c
bool olib_object_list_set(olib_object_t* obj, size_t index, olib_object_t* value);
```

**Notes:** The previous element at the index is freed.

### `olib_object_list_insert`

Insert an element at an index, shifting subsequent elements.

**Signature:**
```c
bool olib_object_list_insert(olib_object_t* obj, size_t index, olib_object_t* value);
```

### `olib_object_list_remove`

Remove an element at an index, shifting subsequent elements.

**Signature:**
```c
bool olib_object_list_remove(olib_object_t* obj, size_t index);
```

### `olib_object_list_push`

Add an element to the end of the list.

**Signature:**
```c
bool olib_object_list_push(olib_object_t* obj, olib_object_t* value);
```

**Example:**
```c
olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);

olib_object_t* item = olib_object_new(OLIB_OBJECT_TYPE_INT);
olib_object_set_int(item, 42);
olib_object_list_push(list, item);  // list owns item now

printf("Size: %zu\n", olib_object_list_size(list));  // 1
```

### `olib_object_list_pop`

Remove the last element from the list.

**Signature:**
```c
bool olib_object_list_pop(olib_object_t* obj);
```

## Struct Operations

### `olib_object_struct_size`

Get the number of key-value pairs.

**Signature:**
```c
size_t olib_object_struct_size(olib_object_t* obj);
```

### `olib_object_struct_has`

Check if a key exists.

**Signature:**
```c
bool olib_object_struct_has(olib_object_t* obj, const char* key);
```

### `olib_object_struct_get`

Get a value by key.

**Signature:**
```c
olib_object_t* olib_object_struct_get(olib_object_t* obj, const char* key);
```

**Returns:** Pointer to value (owned by struct), or NULL if key not found

### `olib_object_struct_key_at`

Get the key at an index (for iteration).

**Signature:**
```c
const char* olib_object_struct_key_at(olib_object_t* obj, size_t index);
```

### `olib_object_struct_value_at`

Get the value at an index (for iteration).

**Signature:**
```c
olib_object_t* olib_object_struct_value_at(olib_object_t* obj, size_t index);
```

### `olib_object_struct_add`

Add a new key-value pair. Fails if key already exists.

**Signature:**
```c
bool olib_object_struct_add(olib_object_t* obj, const char* key, olib_object_t* value);
```

### `olib_object_struct_set`

Set a key-value pair. Creates key if it doesn't exist, overwrites if it does.

**Signature:**
```c
bool olib_object_struct_set(olib_object_t* obj, const char* key, olib_object_t* value);
```

**Example:**
```c
olib_object_t* person = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
olib_object_set_string(name, "Alice");
olib_object_struct_set(person, "name", name);

// Iteration
for (size_t i = 0; i < olib_object_struct_size(person); i++) {
    const char* key = olib_object_struct_key_at(person, i);
    olib_object_t* val = olib_object_struct_value_at(person, i);
    printf("%s: (type=%s)\n", key, olib_object_type_to_string(olib_object_get_type(val)));
}
```

### `olib_object_struct_remove`

Remove a key-value pair.

**Signature:**
```c
bool olib_object_struct_remove(olib_object_t* obj, const char* key);
```

## Value Getters

All getters return default values (0, NULL, false) if the object is NULL or wrong type.

### `olib_object_get_int`
```c
int64_t olib_object_get_int(olib_object_t* obj);
```

### `olib_object_get_uint`
```c
uint64_t olib_object_get_uint(olib_object_t* obj);
```

### `olib_object_get_float`
```c
double olib_object_get_float(olib_object_t* obj);
```

### `olib_object_get_string`

Returns internal string pointer. Do not free. Valid until object is modified or freed.

```c
const char* olib_object_get_string(olib_object_t* obj);
```

### `olib_object_get_bool`
```c
bool olib_object_get_bool(olib_object_t* obj);
```

## Value Setters

All setters return false if the object is NULL or wrong type.

### `olib_object_set_int`
```c
bool olib_object_set_int(olib_object_t* obj, int64_t value);
```

### `olib_object_set_uint`
```c
bool olib_object_set_uint(olib_object_t* obj, uint64_t value);
```

### `olib_object_set_float`
```c
bool olib_object_set_float(olib_object_t* obj, double value);
```

### `olib_object_set_string`

Makes an internal copy of the string.

```c
bool olib_object_set_string(olib_object_t* obj, const char* value);
```

### `olib_object_set_bool`
```c
bool olib_object_set_bool(olib_object_t* obj, bool value);
```

## Complete Example

```c
#include <olib.h>
#include <stdio.h>

int main(void) {
    // Create a complex structure
    olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    // Add primitive values
    olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(name, "Test Config");
    olib_object_struct_set(root, "name", name);

    olib_object_t* enabled = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
    olib_object_set_bool(enabled, true);
    olib_object_struct_set(root, "enabled", enabled);

    // Add a nested list
    olib_object_t* ports = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    int port_values[] = {80, 443, 8080};
    for (int i = 0; i < 3; i++) {
        olib_object_t* port = olib_object_new(OLIB_OBJECT_TYPE_INT);
        olib_object_set_int(port, port_values[i]);
        olib_object_list_push(ports, port);
    }
    olib_object_struct_set(root, "ports", ports);

    // Access values
    printf("Name: %s\n", olib_object_get_string(olib_object_struct_get(root, "name")));
    printf("Enabled: %s\n", olib_object_get_bool(olib_object_struct_get(root, "enabled")) ? "yes" : "no");

    olib_object_t* ports_list = olib_object_struct_get(root, "ports");
    printf("Ports: ");
    for (size_t i = 0; i < olib_object_list_size(ports_list); i++) {
        printf("%lld ", (long long)olib_object_get_int(olib_object_list_get(ports_list, i)));
    }
    printf("\n");

    // Cleanup - frees entire tree
    olib_object_free(root);
    return 0;
}
```
