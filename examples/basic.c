/*
 * Basic Example - Demonstrates minimal olib usage
 * 
 * This example shows how to:
 * - Create simple objects (int, float, string, bool)
 * - Create containers (list, struct)
 * - Access values from objects
 */

#include <olib.h>
#include <stdio.h>

int main(void) {
    printf("=== Basic olib Example ===\n\n");

    // Create simple value objects
    olib_object_t* num = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(num, 42);
    
    olib_object_t* pi = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    olib_object_set_float(pi, 3.14159);
    
    olib_object_t* message = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(message, "Hello, olib!");
    
    // Create a list
    olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    olib_object_list_push(list, num);
    olib_object_list_push(list, pi);
    olib_object_list_push(list, message);
    
    // Create a struct
    olib_object_t* person = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    
    olib_object_t* name = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(name, "Alice");
    olib_object_struct_set(person, "name", name);
    
    olib_object_t* age = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(age, 30);
    olib_object_struct_set(person, "age", age);
    
    // Access and print values
    printf("List contents:\n");
    for (size_t i = 0; i < olib_object_list_size(list); i++) {
        olib_object_t* item = olib_object_list_get(list, i);
        olib_object_type_t type = olib_object_get_type(item);
        
        printf("  Item %zu (%s): ", i, olib_object_type_to_string(type));
        
        switch (type) {
            case OLIB_OBJECT_TYPE_INT:
                printf("%lld\n", (long long)olib_object_get_int(item));
                break;
            case OLIB_OBJECT_TYPE_FLOAT:
                printf("%f\n", olib_object_get_float(item));
                break;
            case OLIB_OBJECT_TYPE_STRING:
                printf("\"%s\"\n", olib_object_get_string(item));
                break;
            default:
                printf("(other type)\n");
                break;
        }
    }
    
    printf("\nStruct contents:\n");
    printf("  name: %s\n", olib_object_get_string(olib_object_struct_get(person, "name")));
    printf("  age: %lld\n", (long long)olib_object_get_int(olib_object_struct_get(person, "age")));
    
    // Cleanup
    olib_object_free(list);
    olib_object_free(person);
    
    printf("\n=== Example completed successfully ===\n");
    return 0;
}
