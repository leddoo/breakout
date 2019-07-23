#pragma once

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


typedef struct Brick {
  Rect rect;
  U32 type;
} Brick;

enum {
  GAME_STATE_UNINITIALIZED = 0,

  GAME_STATE_MAIN_MENU,

  GAME_STATE_WAIT_SERVE,

  GAME_STATE_PLAYING,

  GAME_STATE_BALL_LOST,
  GAME_STATE_GAME_OVER,
  GAME_STATE_PAUSE,
};

/*
                                       .-----------------------.-------------------------.
                                       v                       |                         |
    uninitialized -> main_menu -> wait_serve -> playing .-> ball_lost (move paddle back) |
                                                   ^    |-> game_over -------------------'
                                                   |    '-> pause
                                                   |          |
                                                   '----------'
*/

typedef struct GameState {
  int state;

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
} GameState;

typedef struct Input {
  F32 paddle_control; // NOTE(leo): Ranges 0-1; negative value indicates "no user input"
} Input;

void game_update(GameState *game_state, F32 dt, Input *input, Image *image, Rect playing_area);

void game_start(GameState *game_state);
void game_serve(GameState *game_state);

Rect compute_playing_area(V2 image_size);

Rect compute_paddle_rect_in_image(GameState *game_state, Rect playing_area);
Rect compute_paddle_motion_rect_in_image(GameState *game_state, Rect playing_area);
