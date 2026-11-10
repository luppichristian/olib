#include <olib.h>
#include <string.h>

int main(void) {
  olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_INT);
  char* output = NULL;
  if (!object || !olib_object_set_int(object, 42) ||
      !olib_format_write_string(OLIB_FORMAT_JSON_TEXT, object, &output)) {
    olib_free(output);
    olib_object_free(object);
    return 1;
  }
  int ok = output && strcmp(output, "42\n") == 0;
  olib_free(output);
  olib_object_free(object);
  return ok ? 0 : 1;
}
