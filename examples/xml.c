/*
 * XML Format Example
 * 
 * This example demonstrates:
 * - Loading and parsing XML files
 * - Serializing objects to XML format
 * - Handling typed attributes in XML
 */

#include <olib.h>
#include "example_utils.h"

int main(void) {
    printf("=== XML Format Example ===\n\n");

    // Create XML serializer
    olib_serializer_t* serializer = olib_serializer_new_xml();
    if (!serializer) {
        fprintf(stderr, "Error: Failed to create XML serializer\n");
        return 1;
    }

    // Read sample XML file
    const char* filename = "../samples/example1.xml";
    char* xml_content = read_file(filename);
    if (!xml_content) {
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Loading XML from: %s\n\n", filename);

    // Parse XML content
    olib_object_t* obj = olib_serializer_read_string(serializer, xml_content);
    free(xml_content);

    if (!obj) {
        fprintf(stderr, "Error: Failed to parse XML\n");
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Parsed object structure:\n");
    print_object(obj, 0);
    printf("\n\n");

    // Access values
    printf("Accessing values:\n");
    if (olib_object_is_type(obj, OLIB_OBJECT_TYPE_STRUCT)) {
        olib_object_t* bool_val = olib_object_struct_get(obj, "bool_value");
        if (bool_val) {
            printf("  bool_value: %s\n", olib_object_get_bool(bool_val) ? "true" : "false");
        }

        olib_object_t* float_val = olib_object_struct_get(obj, "float_value");
        if (float_val) {
            printf("  float_value: %g\n", olib_object_get_float(float_val));
        }
    }

    // Serialize back to XML
    char* output_xml = NULL;
    if (!olib_serializer_write_string(serializer, obj, &output_xml)) {
        fprintf(stderr, "Error: Failed to serialize to XML\n");
        olib_object_free(obj);
        olib_serializer_free(serializer);
        return 1;
    }

    printf("\nXML output:\n%s\n", output_xml);

    // Cleanup
    olib_free(output_xml);
    olib_object_free(obj);
    olib_serializer_free(serializer);

    printf("=== Example completed successfully ===\n");
    return 0;
}
