#ifndef OLIB_H
#define OLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OLIB_SHARED
    #ifdef _WIN32
        #ifdef OLIB_EXPORTS
            #define OLIB_API __declspec(dllexport)
        #else
            #define OLIB_API __declspec(dllimport)
        #endif
    #else
        #define OLIB_API __attribute__((visibility("default")))
    #endif
#else
    #define OLIB_API
#endif

// Library version
#define OLIB_VERSION_MAJOR 1
#define OLIB_VERSION_MINOR 0
#define OLIB_VERSION_PATCH 0

// Example function
OLIB_API int olib_add(int a, int b);

#ifdef __cplusplus
}
#endif

#endif // OLIB_H
