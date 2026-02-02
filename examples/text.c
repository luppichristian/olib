/*
 * Text Format Example
 * 
 * This example demonstrates:
 * - Loading and parsing simple text format files
 * - Serializing objects to text format
 * - Understanding the text format structure
 */

#include <olib.h>
#include "example_utils.h"

int main(void) {
    printf("=== Text Format Example ===\n\n");

    // Create text serializer
    olib_serializer_t* serializer = olib_serializer_new_txt();
    if (!serializer) {
        fprintf(stderr, "Error: Failed to create text serializer\n");
        return 1;
    }

    // Read sample text file
    const char* filename = "../samples/example1.txt";
    char* text_content = read_file(filename);
    if (!text_content) {
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Loading text from: %s\n\n", filename);
    printf("Raw file content:\n%s\n", text_content);
    printf("---\n\n");

    // Parse text content
    olib_object_t* obj = olib_serializer_read_string(serializer, text_content);
    free(text_content);

    if (!obj) {
        fprintf(stderr, "Error: Failed to parse text\n");
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Parsed object structure:\n");
    print_object(obj, 0);
    printf("\n\n");

    // Create a new simple object and serialize it
    printf("Creating and serializing a new object:\n");
    olib_object_t* new_obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    
    olib_object_t* greeting = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(greeting, "Hello from text format!");
    olib_object_struct_set(new_obj, "message", greeting);
    
    olib_object_t* count = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(count, 123);
    olib_object_struct_set(new_obj, "count", count);

    char* output_text = NULL;
    if (!olib_serializer_write_string(serializer, new_obj, &output_text)) {
        fprintf(stderr, "Error: Failed to serialize to text\n");
        olib_object_free(obj);
        olib_object_free(new_obj);
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Text output:\n%s\n", output_text);

    // Cleanup
    olib_free(output_text);
    olib_object_free(obj);
    olib_object_free(new_obj);
    olib_serializer_free(serializer);

    printf("=== Example completed successfully ===\n");
    return 0;
}
