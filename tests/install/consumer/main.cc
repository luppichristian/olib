#include <olib/olib_object.h>
#include <olib.h>

int main() {
  olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
  if (!object) return 1;
  const bool ok = olib_object_set_bool(object, true) && olib_object_get_bool(object);
  olib_object_free(object);
  return ok ? 0 : 1;
}
