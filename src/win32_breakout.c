#include "win32_breakout.h"

#include "breakout.h"

#include <math.h>

enum {
  MAIN_PLAY,
  MAIN_QUIT,
};

enum {
  WAIT_SERVE_SERVE,
};

enum {
  PAUSE_CONTINUE,
  PAUSE_RESTART,
  PAUSE_QUIT,
};

enum {
  GAME_OVER_RESTART,
  GAME_OVER_QUIT,
};

typedef struct Win32GameState {
  GameState game_state;
  int selected;
} Win32GameState;

bool button_just_pressed(Button button)
{
  return button.is_down && !button.was_down;
}

bool win32_game_update(GameMemory *game_memory, F32 dt, Win32Input *input, Image *game_image, HWND win32_window)
{
  assert(sizeof(Win32GameState) <= sizeof(game_memory->memory));
  Win32GameState *win32_game_state = (Win32GameState *)&game_memory->memory;
  GameState *game_state = &win32_game_state->game_state;

  Rect playing_area = compute_playing_area((V2) { game_image->width, game_image->height });

  Rect paddle_motion_rect = compute_paddle_motion_rect_in_image(game_state, playing_area);
  paddle_motion_rect.pos.y = game_image->height - paddle_motion_rect.pos.y;
  Rect paddle_rect = compute_paddle_rect_in_image(game_state, playing_area);
  paddle_rect.pos.y = game_image->height - paddle_rect.pos.y;
  POINT paddle_center_screen = { paddle_rect.pos.x + paddle_rect.dim.x/2.0f, paddle_rect.pos.y - paddle_rect.dim.y/2.0f };
  ClientToScreen(win32_window, &paddle_center_screen);

  // NOTE(leo): Constrain mouse
  if(game_state->state == GAME_STATE_PLAYING) {
    POINT top_left = { (int)roundf(paddle_motion_rect.pos.x), (int)roundf(paddle_motion_rect.pos.y) };
    ClientToScreen(win32_window, &top_left);
    RECT cursor_rect = {
      .left = top_left.x, .top = top_left.y,
      .right = top_left.x + ceilf(paddle_motion_rect.dim.x) + 1, .bottom = top_left.y + 1,
    };
    ClipCursor(&cursor_rect);
  }
  else {
    ClipCursor(NULL);
  }

  // NOTE(leo): Handle input
  Input game_input = { .paddle_control = -1.0f };

  bool interact = button_just_pressed(input->key_return) || button_just_pressed(input->key_space);

  if(interact) {
    if((game_state->state == GAME_STATE_MAIN_MENU && win32_game_state->selected == MAIN_PLAY)
      || (game_state->state == GAME_STATE_PAUSE && win32_game_state->selected == PAUSE_RESTART)
      || (game_state->state == GAME_STATE_GAME_OVER && win32_game_state->selected == GAME_OVER_RESTART))
    {
      game_start(game_state);
    }
    else if((game_state->state == GAME_STATE_MAIN_MENU && win32_game_state->selected == MAIN_QUIT)
      || (game_state->state == GAME_STATE_PAUSE && win32_game_state->selected == PAUSE_QUIT)
      || (game_state->state == GAME_STATE_GAME_OVER && win32_game_state->selected == GAME_OVER_QUIT))
    {
      return false;
    }
    else if(game_state->state == GAME_STATE_WAIT_SERVE && win32_game_state->selected == WAIT_SERVE_SERVE) {
      game_serve(game_state);
      SetCursorPos(paddle_center_screen.x, paddle_center_screen.y);
      POINT client_pos = paddle_center_screen;
      ScreenToClient(win32_window, &client_pos);
      input->mouse = (V2){ client_pos.x, client_pos.y };
    }
    else if(game_state->state == GAME_STATE_PAUSE && win32_game_state->selected == PAUSE_CONTINUE) {
      game_state->state = GAME_STATE_PLAYING;
      SetCursorPos(paddle_center_screen.x, paddle_center_screen.y);
      POINT client_pos = paddle_center_screen;
      ScreenToClient(win32_window, &client_pos);
      input->mouse = (V2){ client_pos.x, client_pos.y };
    }

    win32_game_state->selected = 0;
  }

  if(button_just_pressed(input->key_escape)) {
    if(game_state->state == GAME_STATE_PLAYING)
      game_state->state = GAME_STATE_PAUSE;
    else if(game_state->state == GAME_STATE_PAUSE)
      game_state->state = GAME_STATE_PLAYING;
  }

  if(game_state->state == GAME_STATE_PLAYING) {
    game_input.paddle_control = (input->mouse.x - paddle_motion_rect.pos.x) / paddle_motion_rect.dim.x;
  }

  game_update(&win32_game_state->game_state, dt, &game_input, game_image, playing_area);

  return true;
}

bool win32_cursor_hidden(GameMemory *game_memory)
{
  Win32GameState *win32_game_state = (Win32GameState *)&game_memory->memory;
  GameState *game_state = &win32_game_state->game_state;
  return false;
  return game_state->state == GAME_STATE_PLAYING;
}

void win32_on_lose_focus(GameMemory *game_memory)
{
  Win32GameState *win32_game_state = (Win32GameState *)&game_memory->memory;
  GameState *game_state = &win32_game_state->game_state;

  if(game_state->state == GAME_STATE_PLAYING)
    game_state->state = GAME_STATE_PAUSE;
}
