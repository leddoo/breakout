#include "util.h"
#include "platform.h"

#include <math.h>

void game_update(F32 dt, PlatformImageBuffer *image, PlatformSoundBuffer *sound)
{
  local_persist U32 row_index = 0;
  local_persist U32 col_index = 0;
  local_persist U16 color_index = 0;

  U8 red = (color_index & ((1<<4)-1))*255/16;
  U8 green = (color_index>>4 & ((1<<4)-1))*255/16;
  U8 blue = (color_index>>8 & ((1<<4)-1))*255/16;
  U8 alpha = 255;
  image->memory[(row_index+0)%image->height*image->pitch + col_index]
    = blue | (green << 8) | (red << 16) | (alpha << 24);
  color_index++;
  col_index++;
  if(col_index == image->width) {
    col_index = 0;
    row_index += 1;
  }

  local_persist F32 t = 0;
  U32 wave_period = PLATFORM_SOUND_BUFFER_SAMPLE_FREQUENCY/262;
  U16 *sample = sound->memory;
  for(U32 i = 0; i < sound->sample_count; i++) {
    S16 left = (S16)(800.0f*sinf(t));
    S16 right = left;
    *sample++ = left;
    *sample++ = right;
    t += 2.0f*3.14159f/wave_period;
  }
}
