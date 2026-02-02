/*
 * JSON Text Format Example
 * 
 * This example demonstrates:
 * - Loading and parsing JSON text files
 * - Serializing objects to JSON format
 * - Round-trip conversion (read -> modify -> write)
 */

#include <olib.h>
#include "example_utils.h"

int main(void) {
    printf("=== JSON Text Format Example ===\n\n");

    // Create JSON text serializer
    olib_serializer_t* serializer = olib_serializer_new_json_text();
    if (!serializer) {
        fprintf(stderr, "Error: Failed to create JSON serializer\n");
        return 1;
    }

    // Read sample JSON file
    const char* filename = "../samples/example1.json";
    char* json_content = read_file(filename);
    if (!json_content) {
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Loading JSON from: %s\n\n", filename);

    // Parse JSON content
    olib_object_t* obj = olib_serializer_read_string(serializer, json_content);
    free(json_content);

    if (!obj) {
        fprintf(stderr, "Error: Failed to parse JSON\n");
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Parsed object structure:\n");
    print_object(obj, 0);
    printf("\n\n");

    // Access specific values
    printf("Accessing specific values:\n");
    if (olib_object_is_type(obj, OLIB_OBJECT_TYPE_STRUCT)) {
        olib_object_t* int_val = olib_object_struct_get(obj, "int_value");
        if (int_val) {
            printf("  int_value: %lld\n", (long long)olib_object_get_int(int_val));
        }

        olib_object_t* str_val = olib_object_struct_get(obj, "string_value");
        if (str_val) {
            printf("  string_value: \"%s\"\n", olib_object_get_string(str_val));
        }

        olib_object_t* list_simple = olib_object_struct_get(obj, "list_simple");
        if (list_simple && olib_object_is_type(list_simple, OLIB_OBJECT_TYPE_LIST)) {
            printf("  list_simple size: %zu\n", olib_object_list_size(list_simple));
        }
    }

    // Modify the object
    printf("\nModifying object...\n");
    if (olib_object_is_type(obj, OLIB_OBJECT_TYPE_STRUCT)) {
        olib_object_t* new_field = olib_object_new(OLIB_OBJECT_TYPE_STRING);
        olib_object_set_string(new_field, "Added by example program");
        olib_object_struct_set(obj, "modified", new_field);
    }

    // Serialize back to JSON
    char* output_json = NULL;
    if (!olib_serializer_write_string(serializer, obj, &output_json)) {
        fprintf(stderr, "Error: Failed to serialize to JSON\n");
        olib_object_free(obj);
        olib_serializer_free(serializer);
        return 1;
    }

    printf("\nModified JSON output:\n%s\n", output_json);

    // Cleanup
    olib_free(output_json);
    olib_object_free(obj);
    olib_serializer_free(serializer);

    printf("\n=== Example completed successfully ===\n");
    return 0;
}
