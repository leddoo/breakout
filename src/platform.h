#include "util.h"

#define PLATFORM_SOUND_BUFFER_CHANNEL_COUNT 2
#define PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE sizeof(S16)
#define PLATFORM_SOUND_BUFFER_SAMPLE_FREQUENCY 48000
#define PLATFORM_SOUND_BUFFER_SIZE(sample_count) (sample_count*PLATFORM_SOUND_BUFFER_CHANNEL_COUNT*PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE)
typedef struct PlatformSoundBuffer {
  S16 *memory;
  U32 sample_count;
} PlatformSoundBuffer;

typedef struct PlatformImageBuffer {
  U32 *memory;
  int width, height;
  int pitch;
} PlatformImageBuffer;
