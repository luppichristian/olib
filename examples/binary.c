/*
 * Binary Format Example
 * 
 * This example demonstrates:
 * - Serializing objects to binary format
 * - Deserializing binary data back to objects
 * - Round-trip conversion with binary format
 * 
 * Note: Binary format is not human-readable but is efficient
 * for storage and transmission.
 */

#include <olib.h>
#include "example_utils.h"

int main(void) {
    printf("=== Binary Format Example ===\n\n");

    // Create binary serializer
    olib_serializer_t* serializer = olib_serializer_new_binary();
    if (!serializer) {
        fprintf(stderr, "Error: Failed to create binary serializer\n");
        return 1;
    }

    // Create a complex object to serialize
    printf("Creating object for binary serialization:\n");
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    
    olib_object_t* id = olib_object_new(OLIB_OBJECT_TYPE_UINT);
    olib_object_set_uint(id, 12345);
    olib_object_struct_set(obj, "id", id);
    
    olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    olib_object_set_float(value, 98.6);
    olib_object_struct_set(obj, "value", value);
    
    olib_object_t* active = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
    olib_object_set_bool(active, true);
    olib_object_struct_set(obj, "active", active);
    
    olib_object_t* items = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    for (int i = 1; i <= 5; i++) {
        olib_object_t* item = olib_object_new(OLIB_OBJECT_TYPE_INT);
        olib_object_set_int(item, i * 10);
        olib_object_list_push(items, item);
    }
    olib_object_struct_set(obj, "items", items);

    print_object(obj, 0);
    printf("\n\n");

    // Serialize to binary
    uint8_t* binary_data = NULL;
    size_t binary_size = 0;
    if (!olib_serializer_write(serializer, obj, &binary_data, &binary_size)) {
        fprintf(stderr, "Error: Failed to serialize to binary\n");
        olib_object_free(obj);
        olib_serializer_free(serializer);
        return 1;
    }

    printf("Serialized to binary format: %zu bytes\n", binary_size);
    printf("First 20 bytes (hex): ");
    for (size_t i = 0; i < 20 && i < binary_size; i++) {
        printf("%02x ", binary_data[i]);
    }
    printf("\n\n");

    // Deserialize from binary
    printf("Deserializing from binary:\n");
    olib_object_t* deserialized = olib_serializer_read(serializer, binary_data, binary_size);
    if (!deserialized) {
        fprintf(stderr, "Error: Failed to deserialize from binary\n");
        olib_free(binary_data);
        olib_object_free(obj);
        olib_serializer_free(serializer);
        return 1;
    }

    print_object(deserialized, 0);
    printf("\n\n");

    // Verify round-trip worked correctly
    printf("Verification:\n");
    if (olib_object_is_type(deserialized, OLIB_OBJECT_TYPE_STRUCT)) {
        olib_object_t* id_check = olib_object_struct_get(deserialized, "id");
        olib_object_t* value_check = olib_object_struct_get(deserialized, "value");
        
        printf("  id matches: %s\n", 
               olib_object_get_uint(id_check) == 12345 ? "yes" : "no");
        printf("  value matches: %s\n",
               olib_object_get_float(value_check) == 98.6 ? "yes" : "no");
    }

    // Cleanup
    olib_free(binary_data);
    olib_object_free(obj);
    olib_object_free(deserialized);
    olib_serializer_free(serializer);

    printf("\n=== Example completed successfully ===\n");
    return 0;
}
