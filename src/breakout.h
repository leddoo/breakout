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

#define INITIAL_BALL_POS ((V2){ARENA_WIDTH/2.0f - BALL_WIDTH/2.0f, PADDLE_Y + 10.0f})
#define INITIAL_PADDLE_POS ((V2){ARENA_WIDTH/2.0f - PADDLE_WIDTH/2.0f, PADDLE_Y})

#define BALL_SPEED_1 50.0f
#define BALL_SPEED_2 75.0f
#define BALL_SPEED_3 100.0f
#define BALL_SPEED_4 125.0f

#define BALL_SPEED_2 50.0f
#define BALL_SPEED_3 50.0f
#define BALL_SPEED_4 50.0f

typedef struct V2 {
  F32 x, y;
} V2;

typedef struct Rect {
  V2 pos, dim;
} Rect;


typedef struct Brick {
  Rect rect;
  U32 type;
} Brick;

typedef struct GameState {
  bool initialized;
  Rect ball;
  V2 ball_direction;
  F32 ball_speed;
  F32 target_ball_speed;

  Rect paddle;

  Brick bricks[BRICK_COUNT_X*BRICK_COUNT_Y];
  int bricks_remaining;

  int score;
  int hit_count;
  int balls_remaining;
  bool waiting_for_serve;
} GameState;

typedef struct Input {
  F32 paddle_control; // NOTE(leo): Ranges 0-1; negative value indicates "no user input"
  bool serve;
} Input;

typedef struct Image {
  U32 *memory;
  int width, height;
  int pitch;
} Image;

void game_update(GameState *state, F32 dt, Input *input, Image *image, Rect playing_area);
