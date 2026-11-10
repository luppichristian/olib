#include <olib.h>

/* Exercise each reader with the same bounded data. The public text API takes
 * a NUL-terminated string; binary readers receive the original bytes and size. */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  enum { MAX_INPUT = 4096 };
  if (size > MAX_INPUT) return 0;

  char text[MAX_INPUT + 1];
  if (size != 0) memcpy(text, data, size);
  text[size] = '\0';

  const olib_format_t text_formats[] = {
      OLIB_FORMAT_JSON_TEXT, OLIB_FORMAT_YAML, OLIB_FORMAT_XML, OLIB_FORMAT_TOML, OLIB_FORMAT_TXT};
  for (size_t i = 0; i < sizeof(text_formats) / sizeof(text_formats[0]); ++i) {
    olib_object_t* object = olib_format_read_string(text_formats[i], text);
    olib_object_free(object);
  }

  const olib_format_t binary_formats[] = {OLIB_FORMAT_JSON_BINARY, OLIB_FORMAT_BINARY};
  for (size_t i = 0; i < sizeof(binary_formats) / sizeof(binary_formats[0]); ++i) {
    olib_object_t* object = olib_format_read(binary_formats[i], data, size);
    olib_object_free(object);
  }
  return 0;
}
