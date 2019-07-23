#pragma once

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

#define array_count(array) (sizeof(array)/sizeof((array)[0]))

typedef struct V2 {
  F32 x, y;
} V2;

typedef struct Rect {
  V2 pos, dim;
} Rect;

typedef struct Image {
  U32 *memory;
  int width, height;
  int pitch;
} Image;

typedef struct GameMemory {
  U8 memory[4096];
} GameMemory;
