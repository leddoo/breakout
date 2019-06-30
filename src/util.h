#include <assert.h>
#include <stdbool.h>

#include <stdint.h>
typedef int8_t S8;
typedef uint8_t U8;
typedef int16_t S16;
typedef uint16_t U16;
typedef int32_t S32;
typedef uint32_t U32;
typedef int64_t S64;
typedef uint64_t U64;

// TODO(leo): Make sure these are actually the correct size
typedef float F32;
typedef double F64;

#define internal static
#define global_variable static
#define local_persist static
