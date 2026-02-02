/**
 * @file main.c
 * @brief CLI utility for converting between olib-supported formats
 */

#include <olib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

static const char *format_names[] = {
    "json",
    "json-binary",
    "yaml",
    "xml",
    "toml",
    "txt",
    "binary"
};

static const char *format_extensions[] = {
    ".json",
    ".jsonb",
    ".yaml",
    ".xml",
    ".toml",
    ".txt",
    ".bin"
};

static void print_usage(const char *program_name) {
    printf("olib-convert - Convert between serialization formats\n\n");
    printf("Usage: %s [options] <input-file> <output-file>\n\n", program_name);
    printf("Options:\n");
    printf("  -i, --input-format <format>   Input format (auto-detected from extension if not specified)\n");
    printf("  -o, --output-format <format>  Output format (auto-detected from extension if not specified)\n");
    printf("  -h, --help                    Show this help message\n");
    printf("  -v, --version                 Show version information\n\n");
    printf("Supported formats:\n");
    printf("  json        JSON text format (.json)\n");
    printf("  json-binary JSON binary format (.jsonb)\n");
    printf("  yaml        YAML format (.yaml, .yml)\n");
    printf("  xml         XML format (.xml)\n");
    printf("  toml        TOML format (.toml)\n");
    printf("  txt         Plain text format (.txt)\n");
    printf("  binary      Compact binary format (.bin)\n\n");
    printf("Examples:\n");
    printf("  %s data.json data.yaml\n", program_name);
    printf("  %s -i json -o xml input.txt output.txt\n", program_name);
    printf("  %s config.toml config.json\n", program_name);
}

static void print_version(void) {
    printf("olib-convert version 1.0.0\n");
    printf("Part of the olib serialization library\n");
}

static olib_format_t parse_format(const char *format_str) {
    if (strcasecmp(format_str, "json") == 0 || strcasecmp(format_str, "json-text") == 0) {
        return OLIB_FORMAT_JSON_TEXT;
    } else if (strcasecmp(format_str, "json-binary") == 0 || strcasecmp(format_str, "jsonb") == 0) {
        return OLIB_FORMAT_JSON_BINARY;
    } else if (strcasecmp(format_str, "yaml") == 0 || strcasecmp(format_str, "yml") == 0) {
        return OLIB_FORMAT_YAML;
    } else if (strcasecmp(format_str, "xml") == 0) {
        return OLIB_FORMAT_XML;
    } else if (strcasecmp(format_str, "toml") == 0) {
        return OLIB_FORMAT_TOML;
    } else if (strcasecmp(format_str, "txt") == 0 || strcasecmp(format_str, "text") == 0) {
        return OLIB_FORMAT_TXT;
    } else if (strcasecmp(format_str, "binary") == 0 || strcasecmp(format_str, "bin") == 0) {
        return OLIB_FORMAT_BINARY;
    }
    return (olib_format_t)-1;
}

static olib_format_t detect_format_from_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (dot == NULL) {
        return (olib_format_t)-1;
    }

    if (strcasecmp(dot, ".json") == 0) {
        return OLIB_FORMAT_JSON_TEXT;
    } else if (strcasecmp(dot, ".jsonb") == 0) {
        return OLIB_FORMAT_JSON_BINARY;
    } else if (strcasecmp(dot, ".yaml") == 0 || strcasecmp(dot, ".yml") == 0) {
        return OLIB_FORMAT_YAML;
    } else if (strcasecmp(dot, ".xml") == 0) {
        return OLIB_FORMAT_XML;
    } else if (strcasecmp(dot, ".toml") == 0) {
        return OLIB_FORMAT_TOML;
    } else if (strcasecmp(dot, ".txt") == 0) {
        return OLIB_FORMAT_TXT;
    } else {
        return OLIB_FORMAT_BINARY;
    }

    return (olib_format_t)-1;
}

static const char *format_to_string(olib_format_t format) {
    if (format >= 0 && format < sizeof(format_names) / sizeof(format_names[0])) {
        return format_names[format];
    }
    return "unknown";
}

int main(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    olib_format_t input_format = (olib_format_t)-1;
    olib_format_t output_format = (olib_format_t)-1;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input-format") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
            input_format = parse_format(argv[++i]);
            if ((int)input_format == -1) {
                fprintf(stderr, "Error: Unknown input format '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-format") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing argument for %s\n", argv[i]);
                return 1;
            }
            output_format = parse_format(argv[++i]);
            if ((int)output_format == -1) {
                fprintf(stderr, "Error: Unknown output format '%s'\n", argv[i]);
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return 1;
        } else {
            // Positional argument
            if (input_file == NULL) {
                input_file = argv[i];
            } else if (output_file == NULL) {
                output_file = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                return 1;
            }
        }
    }

    // Validate arguments
    if (input_file == NULL || output_file == NULL) {
        fprintf(stderr, "Error: Both input and output files are required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    // Auto-detect formats if not specified
    if ((int)input_format == -1) {
        input_format = detect_format_from_extension(input_file);
        if ((int)input_format == -1) {
            fprintf(stderr, "Error: Cannot detect input format from extension. Use -i to specify format.\n");
            return 1;
        }
    }

    if ((int)output_format == -1) {
        output_format = detect_format_from_extension(output_file);
        if ((int)output_format == -1) {
            fprintf(stderr, "Error: Cannot detect output format from extension. Use -o to specify format.\n");
            return 1;
        }
    }

    // Perform conversion
    printf("Converting %s (%s) -> %s (%s)\n",
           input_file, format_to_string(input_format),
           output_file, format_to_string(output_format));

    bool success = olib_convert_file_path(input_format, input_file, output_format, output_file);

    if (!success) {
        fprintf(stderr, "Error: Conversion failed\n");
        return 1;
    }

    printf("Conversion successful!\n");
    return 0;
}
