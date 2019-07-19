#include "util.h"

#define BRICK_COUNT_X 14
#define BRICK_COUNT_Y 8
#define FIRST_BRICK_HEIGHT 90.0f
#define BRICK_WIDTH 7.0f
#define BRICK_HEIGHT 2.0f
#define BRICK_DELTA_X 1.0f
#define BRICK_DELTA_Y 0.8f

#define PADDLE_WIDTH 7.0f
#define PADDLE_HEIGTH 3.0f
#define PADDLE_Y 6.0f

#define PADDLE_COLOR_R 0.0f
#define PADDLE_COLOR_G 0.5f
#define PADDLE_COLOR_B 0.78f

#define BALL_WIDTH 2.0f
#define BALL_HEIGHT 1.5f

#define BALL_COLOR_R 0.82f
#define BALL_COLOR_G 0.82f
#define BALL_COLOR_B 0.82f

#define ARENA_WIDTH (BRICK_COUNT_X*BRICK_WIDTH + (BRICK_COUNT_X+1)*BRICK_DELTA_X)
#define ARENA_HEIGHT 140.0f
#define PLAYING_AREA_WIDTH (ARENA_WIDTH + 4.0f)
#define PLAYING_AREA_HEIGHT (ARENA_HEIGHT + 2.0f + 20.0f)

typedef struct Input {
  F32 paddle_control; // NOTE(leo): Ranges 0-1; negative value indicates "no user input"
  bool serve;
} Input;

typedef struct Image {
  U32 *memory;
  int width, height;
  int pitch;
} Image;

typedef struct V2 {
  F32 x, y;
} V2;

typedef struct Rect {
  V2 pos, dim;
} Rect;

void game_update(F32 dt, Input *input, Image *image, Rect playing_area);
