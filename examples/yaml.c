/*
 * YAML Format Example
 * 
 * This example demonstrates:
 * - Loading and parsing YAML files
 * - Serializing objects to YAML format
 * - Handling nested structures in YAML
 */

#include <olib.h>
#include "example_utils.h"

int main(void) {
    printf("=== YAML Format Example ===\n\n");

    // Create YAML serializer
    olib_serializer_t* serializer = olib_serializer_new_yaml();
    if (!serializer) {
        fprintf(stderr, "Error: Failed to create YAML serializer\n");
        return 1;
    }

    // Read sample YAML file
    const char* filename = "../samples/example1.yaml";
    char* yaml_content = read_file(filename);
    if (!yaml_content) {
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Loading YAML from: %s\n\n", filename);

    // Parse YAML content
    olib_object_t* obj = olib_serializer_read_string(serializer, yaml_content);
    free(yaml_content);

    if (!obj) {
        fprintf(stderr, "Error: Failed to parse YAML\n");
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Parsed object structure:\n");
    print_object(obj, 0);
    printf("\n\n");

    // Access nested values
    printf("Accessing nested values:\n");
    if (olib_object_is_type(obj, OLIB_OBJECT_TYPE_STRUCT)) {
        olib_object_t* nested = olib_object_struct_get(obj, "nested_struct");
        if (nested && olib_object_is_type(nested, OLIB_OBJECT_TYPE_STRUCT)) {
            olib_object_t* nested_int = olib_object_struct_get(nested, "nested_int");
            if (nested_int) {
                printf("  nested_struct.nested_int: %lld\n", 
                       (long long)olib_object_get_int(nested_int));
            }
        }

        olib_object_t* list_mixed = olib_object_struct_get(obj, "list_mixed");
        if (list_mixed && olib_object_is_type(list_mixed, OLIB_OBJECT_TYPE_LIST)) {
            printf("  list_mixed[0].name: \"%s\"\n",
                   olib_object_get_string(
                       olib_object_struct_get(
                           olib_object_list_get(list_mixed, 0), "name")));
        }
    }

    // Serialize back to YAML
    char* output_yaml = NULL;
    if (!olib_serializer_write_string(serializer, obj, &output_yaml)) {
        fprintf(stderr, "Error: Failed to serialize to YAML\n");
        olib_object_free(obj);
        olib_serializer_free(serializer);
        return 1;
    }

    printf("\nYAML output:\n%s\n", output_yaml);

    // Cleanup
    olib_free(output_yaml);
    olib_object_free(obj);
    olib_serializer_free(serializer);

    printf("=== Example completed successfully ===\n");
    return 0;
}
