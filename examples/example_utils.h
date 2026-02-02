/*
 * Utility functions shared across format examples
 */

#pragma once

#include <olib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Read entire file into memory
static char* read_file(const char* filename) {
  FILE* file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, "Error: Could not open file '%s'\n", filename);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* buffer = (char*)malloc(size + 1);
  if (!buffer) {
    fclose(file);
    fprintf(stderr, "Error: Out of memory\n");
    return NULL;
  }

  size_t read = fread(buffer, 1, size, file);
  buffer[read] = '\0';
  fclose(file);

  return buffer;
}

// Print object recursively with indentation
static void print_object(olib_object_t* obj, int indent) {
  if (!obj) {
    printf("(null)");
    return;
  }

  olib_object_type_t type = olib_object_get_type(obj);

  switch (type) {
    case OLIB_OBJECT_TYPE_INT:
      printf("%lld", (long long)olib_object_get_int(obj));
      break;

    case OLIB_OBJECT_TYPE_UINT:
      printf("%llu", (unsigned long long)olib_object_get_uint(obj));
      break;

    case OLIB_OBJECT_TYPE_FLOAT:
      printf("%g", olib_object_get_float(obj));
      break;

    case OLIB_OBJECT_TYPE_STRING:
      printf("\"%s\"", olib_object_get_string(obj));
      break;

    case OLIB_OBJECT_TYPE_BOOL:
      printf("%s", olib_object_get_bool(obj) ? "true" : "false");
      break;

    case OLIB_OBJECT_TYPE_LIST: {
      size_t size = olib_object_list_size(obj);
      printf("[\n");
      for (size_t i = 0; i < size; i++) {
        for (int j = 0; j < indent + 1; j++) printf("  ");
        print_object(olib_object_list_get(obj, i), indent + 1);
        if (i < size - 1) printf(",");
        printf("\n");
      }
      for (int j = 0; j < indent; j++) printf("  ");
      printf("]");
      break;
    }

    case OLIB_OBJECT_TYPE_STRUCT: {
      size_t size = olib_object_struct_size(obj);
      printf("{\n");
      for (size_t i = 0; i < size; i++) {
        for (int j = 0; j < indent + 1; j++) printf("  ");
        printf("\"%s\": ", olib_object_struct_key_at(obj, i));
        print_object(olib_object_struct_value_at(obj, i), indent + 1);
        if (i < size - 1) printf(",");
        printf("\n");
      }
      for (int j = 0; j < indent; j++) printf("  ");
      printf("}");
      break;
    }

    default:
      printf("(unknown type)");
      break;
  }
}
