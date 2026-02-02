/*
 * TOML Format Example
 * 
 * This example demonstrates:
 * - Loading and parsing TOML files
 * - Serializing objects to TOML format
 * - Handling tables and arrays of tables
 */

#include <olib.h>
#include "example_utils.h"

int main(void) {
    printf("=== TOML Format Example ===\n\n");

    // Create TOML serializer
    olib_serializer_t* serializer = olib_serializer_new_toml();
    if (!serializer) {
        fprintf(stderr, "Error: Failed to create TOML serializer\n");
        return 1;
    }

    // Read sample TOML file
    const char* filename = "../samples/example1.toml";
    char* toml_content = read_file(filename);
    if (!toml_content) {
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Loading TOML from: %s\n\n", filename);

    // Parse TOML content
    olib_object_t* obj = olib_serializer_read_string(serializer, toml_content);
    free(toml_content);

    if (!obj) {
        fprintf(stderr, "Error: Failed to parse TOML\n");
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Parsed object structure:\n");
    print_object(obj, 0);
    printf("\n\n");

    // Access values
    printf("Accessing values:\n");
    if (olib_object_is_type(obj, OLIB_OBJECT_TYPE_STRUCT)) {
        olib_object_t* uint_val = olib_object_struct_get(obj, "uint_value");
        if (uint_val) {
            printf("  uint_value: %llu\n", (unsigned long long)olib_object_get_uint(uint_val));
        }

        // Access array of tables
        olib_object_t* list_mixed = olib_object_struct_get(obj, "list_mixed");
        if (list_mixed && olib_object_is_type(list_mixed, OLIB_OBJECT_TYPE_LIST)) {
            printf("  list_mixed has %zu entries\n", olib_object_list_size(list_mixed));
            
            olib_object_t* first = olib_object_list_get(list_mixed, 0);
            if (first && olib_object_is_type(first, OLIB_OBJECT_TYPE_STRUCT)) {
                olib_object_t* name = olib_object_struct_get(first, "name");
                olib_object_t* age = olib_object_struct_get(first, "age");
                if (name && age) {
                    printf("  First entry: name=\"%s\", age=%lld\n",
                           olib_object_get_string(name),
                           (long long)olib_object_get_int(age));
                }
            }
        }
    }

    // Serialize back to TOML
    char* output_toml = NULL;
    if (!olib_serializer_write_string(serializer, obj, &output_toml)) {
        fprintf(stderr, "Error: Failed to serialize to TOML\n");
        olib_object_free(obj);
        olib_serializer_free(serializer);
        return 1;
    }

    printf("\nTOML output:\n%s\n", output_toml);

    // Cleanup
    olib_free(output_toml);
    olib_object_free(obj);
    olib_serializer_free(serializer);

    printf("=== Example completed successfully ===\n");
    return 0;
}
