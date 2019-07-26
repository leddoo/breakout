#pragma once

#include "util.h"

#define BRICK_COUNT_X 14
#define BRICK_COUNT_Y 8
#define BRICK_COUNT (BRICK_COUNT_X*BRICK_COUNT_Y)
#define FIRST_BRICK_HEIGHT 90.0f
#define BRICK_WIDTH 7.0f
#define BRICK_HEIGHT 2.0f
#define BRICK_DELTA_X 1.0f
#define BRICK_DELTA_Y 0.8f

#define PADDLE_WIDTH(difficulty_factor) (7.0f/difficulty_factor)
#define PADDLE_HEIGTH 3.0f
#define PADDLE_Y 6.0f

#define PADDLE_COLOR ((Color){ 0.0f, 0.5f, 0.78f, 1.0f })

#define BALL_WIDTH 2.0f
#define BALL_HEIGHT 1.5f

#define BALL_COLOR ((Color){ 0.82f, 0.82f, 0.82f, 1.0f })

#define ARENA_WIDTH (BRICK_COUNT_X*BRICK_WIDTH + (BRICK_COUNT_X+1)*BRICK_DELTA_X)
#define ARENA_HEIGHT 140.0f
#define PLAYING_AREA_WIDTH (ARENA_WIDTH + 4.0f)
#define PLAYING_AREA_HEIGHT (ARENA_HEIGHT + 2.0f + 20.0f)

#define INITIAL_BALL_POS ((V2){ARENA_WIDTH/2.0f - BALL_WIDTH/2.0f, PADDLE_Y + 10.0f})
#define INITIAL_PADDLE_POS(paddle_width) (ARENA_WIDTH/2.0f - paddle_width/2.0f)

#define BALL_SPEED_1 50.0f
#define BALL_SPEED_2 75.0f
#define BALL_SPEED_3 100.0f
#define BALL_SPEED_4 125.0f

enum {
  GAME_STATE_UNINITIALIZED = 0,

  GAME_STATE_MAIN_MENU,
  GAME_STATE_DIFFICULTY_SELECT,

  GAME_STATE_WAIT_SERVE,
  GAME_STATE_PLAYING,

  GAME_STATE_GAME_OVER,
  GAME_STATE_PAUSE,

  GAME_STATE_RESET_PADDLE,
  GAME_STATE_RESET_GAME,

  GAME_STATE_COUNT,
};

/*
  Transitions: (k) Key, (b) Menu button, (a) Automatic (eg: Gameplay event or animation finished)


                  a            b
    uninitialized -> main_menu -> difficulty_select
                         ^              b|
                        a|               V     a             b
                         '--------- reset_game -> wait_serve -> playing <---------------------.
                                         |            ^            |                          |
                                         |           a|      .-----'---.------.---------.     |
                                         |            |     a|        a|     a|        k|     |
                                         |            |      V         |      V         V     |k/b
                                         |            '- reset_paddle  |  game_over   paused -'
                                         |                             |     b|        b|
                                         '-----------------------------'------'---------'

    NOTE(leo): reset_game goes to main menu if game_state->is_switching_to_main_menu is true
*/

typedef struct GameState {
  int state;

  // NOTE(leo): "Objects"
  Rect ball;
  V2 ball_direction;
  F32 ball_speed;
  F32 target_ball_speed;

  Rect paddle;
  bool is_paddle_shrunk;

  bool is_brick_broken[BRICK_COUNT];
  int bricks_remaining;

  // NOTE(leo): Gameplay
  F32 difficulty_factor;
  int score;
  int hit_count;
  int balls_remaining;
  bool has_cleared_bricks;

  // NOTE(leo): Animation
  F32 brick_alpha[BRICK_COUNT];
  bool is_switching_to_main_menu;
  bool is_erasing_score;
} GameState;

typedef struct Input {
  F32 paddle_control; // NOTE(leo): Ranges 0-1; negative value indicates "no user input"
} Input;

void game_update(GameState *game_state, F32 dt, Input *input, Image *image, Rect playing_area);

void game_serve(GameState *game_state);

Rect compute_playing_area(V2 image_size);

Rect compute_paddle_rect_in_image(GameState *game_state, Rect playing_area);
Rect compute_paddle_motion_rect_in_image(GameState *game_state, Rect playing_area);

typedef struct Color {
  F32 r, g, b, a;
} Color;

#define COLOR_WHITE ((Color){1.0f, 1.0f, 1.0f, 1.0f})

void draw_text(char *text, V2 bottom_left, F32 pixel_size, Color color, Image *image);
void draw_text_centered(char *text, V2 center, F32 pixel_size, Color color, Image *image);

void switch_to_reset_game(GameState *game_state, bool then_switch_to_main_menu, bool erase_score);

#define SYMBOL_WIDTH 5
#define SYMBOL_HEIGHT 7
#define SYMBOL_SPACING 1
#define LINE_HEIGHT (SYMBOL_HEIGHT+2*SYMBOL_SPACING)
