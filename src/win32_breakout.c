#include "win32_breakout.h"

#include "breakout.h"
#include "renderer.h"

#include <math.h>

enum {
  MAIN_PLAY,
  MAIN_QUIT,
  MAIN_COUNT,
};

enum {
  DIFFICULTY_EASY,
  DIFFICULTY_NORMAL,
  DIFFICULTY_HARD,
  DIFFICULTY_COUNT,
};

enum {
  WAIT_SERVE_SERVE,
  WAIT_SERVE_COUNT,
};

enum {
  PAUSE_CONTINUE,
  PAUSE_RESTART,
  PAUSE_MAIN_MENU,
  PAUSE_COUNT,
};

enum {
  GAME_OVER_RESTART,
  GAME_OVER_MAIN_MENU,
  GAME_OVER_COUNT
};

#define MAX_MENU_ENTRY_COUNT (max(max(max(max(MAIN_COUNT, DIFFICULTY_COUNT), WAIT_SERVE_COUNT), PAUSE_COUNT), GAME_OVER_COUNT))

typedef struct Win32GameState {
  GameState game_state;
  int selected;
} Win32GameState;

bool button_just_pressed(Button button)
{
  return button.is_down && !button.was_down;
}

bool win32_game_update(GameMemory *game_memory, F32 dt, Win32Input *input, HWND win32_window, RenderCmdBuffer *cmd_buffer)
{
  assert(sizeof(Win32GameState) <= sizeof(game_memory->memory));
  Win32GameState *win32_game_state = (Win32GameState *)&game_memory->memory;
  GameState *game_state = &win32_game_state->game_state;

  V2 window_client_dim;
  {
    RECT client_rect;
    GetClientRect(win32_window, &client_rect);
    window_client_dim.x = (F32)client_rect.right - client_rect.left;
    window_client_dim.y = (F32)client_rect.bottom - client_rect.top;
  }

  Rect playing_area = compute_playing_area(window_client_dim);

  Rect paddle_motion_rect = compute_paddle_motion_rect_in_image(game_state, playing_area);
  paddle_motion_rect.pos.y = window_client_dim.y - paddle_motion_rect.pos.y;
  Rect paddle_rect = compute_paddle_rect_in_image(game_state, playing_area);
  paddle_rect.pos.y = window_client_dim.y - paddle_rect.pos.y;
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
  if(button_just_pressed(input->key_return) || button_just_pressed(input->key_space)) {
    if(game_state->state == GAME_STATE_MAIN_MENU && win32_game_state->selected == MAIN_QUIT) {
      return false;
    }
    else if((game_state->state == GAME_STATE_MAIN_MENU && win32_game_state->selected == MAIN_PLAY)) {
      game_state->state = GAME_STATE_DIFFICULTY_SELECT;
      win32_game_state->selected = DIFFICULTY_NORMAL;
    }
    else if(game_state->state == GAME_STATE_DIFFICULTY_SELECT) {
      game_state->difficulty_factor = 1.0f;
      if(win32_game_state->selected == DIFFICULTY_EASY)
        game_state->difficulty_factor = 0.5f;
      else if(win32_game_state->selected == DIFFICULTY_HARD)
        game_state->difficulty_factor = 2.0f;
      switch_to_reset_game(game_state, false, true);
    }
    else if((game_state->state == GAME_STATE_PAUSE && win32_game_state->selected == PAUSE_RESTART)
      || (game_state->state == GAME_STATE_GAME_OVER && win32_game_state->selected == GAME_OVER_RESTART))
    {
      switch_to_reset_game(game_state, false, true);
    }
    else if(game_state->state == GAME_STATE_WAIT_SERVE && win32_game_state->selected == WAIT_SERVE_SERVE) {
      game_serve(game_state);
      SetCursorPos(paddle_center_screen.x, paddle_center_screen.y);
      POINT client_pos = paddle_center_screen;
      ScreenToClient(win32_window, &client_pos);
      input->mouse = (V2){ client_pos.x, client_pos.y };
      SetCursor(NULL);
    }
    else if(game_state->state == GAME_STATE_PAUSE && win32_game_state->selected == PAUSE_CONTINUE) {
      game_state->state = GAME_STATE_PLAYING;
      SetCursorPos(paddle_center_screen.x, paddle_center_screen.y);
      POINT client_pos = paddle_center_screen;
      ScreenToClient(win32_window, &client_pos);
      input->mouse = (V2){ client_pos.x, client_pos.y };
      SetCursor(NULL);
    }
    else if((game_state->state == GAME_STATE_PAUSE && win32_game_state->selected == PAUSE_MAIN_MENU)
      || (game_state->state == GAME_STATE_GAME_OVER && win32_game_state->selected == GAME_OVER_MAIN_MENU))
    {
      game_state->difficulty_factor = 1.0f;
      switch_to_reset_game(game_state, true, true);
    }

    if(game_state->state != GAME_STATE_DIFFICULTY_SELECT)
      win32_game_state->selected = 0;
  }

  if(button_just_pressed(input->key_escape)) {
    if(game_state->state == GAME_STATE_PLAYING) {
      game_state->state = GAME_STATE_PAUSE;
    }
    else if(game_state->state == GAME_STATE_PAUSE) {
      game_state->state = GAME_STATE_PLAYING;
      SetCursorPos(paddle_center_screen.x, paddle_center_screen.y);
      POINT client_pos = paddle_center_screen;
      ScreenToClient(win32_window, &client_pos);
      input->mouse = (V2){ client_pos.x, client_pos.y };
      SetCursor(NULL);
    }
  }

  int menu_entry_count = 0;
  if(game_state->state == GAME_STATE_MAIN_MENU)
    menu_entry_count = MAIN_COUNT;
  else if(game_state->state == GAME_STATE_DIFFICULTY_SELECT)
    menu_entry_count = DIFFICULTY_COUNT;
  else if(game_state->state == GAME_STATE_WAIT_SERVE)
    menu_entry_count = WAIT_SERVE_COUNT;
  else if(game_state->state == GAME_STATE_PAUSE)
    menu_entry_count = PAUSE_COUNT;
  else if(game_state->state == GAME_STATE_GAME_OVER)
    menu_entry_count = GAME_OVER_COUNT;

  if(menu_entry_count && button_just_pressed(input->key_up))
    win32_game_state->selected = (win32_game_state->selected - 1 + menu_entry_count) % menu_entry_count;
  if(menu_entry_count && button_just_pressed(input->key_down))
    win32_game_state->selected = (win32_game_state->selected + 1) % menu_entry_count;

  Input game_input = { .paddle_control = -1.0f };
  if(game_state->state == GAME_STATE_PLAYING) {
    game_input.paddle_control = (input->mouse.x - paddle_motion_rect.pos.x) / paddle_motion_rect.dim.x;
  }

  game_update(&win32_game_state->game_state, dt, &game_input, cmd_buffer);


  char *header = NULL;
  char *texts[MAX_MENU_ENTRY_COUNT] = { NULL };
  int text_count = 0;

  if(game_state->state == GAME_STATE_MAIN_MENU) {
    header = "BREAKOUT";
    texts[0] = "PLAY";
    texts[1] = "QUIT";
    if(win32_game_state->selected == MAIN_PLAY)
      texts[0] = "> PLAY <";
    else if(win32_game_state->selected == MAIN_QUIT)
      texts[1] = "> QUIT <";
    text_count = MAIN_COUNT;
  }
  else if(game_state->state == GAME_STATE_DIFFICULTY_SELECT) {
    header = "DIFFICULTY";
    texts[0] = "EASY";
    texts[1] = "NORMAL";
    texts[2] = "HARD";
    if(win32_game_state->selected == DIFFICULTY_EASY)
      texts[0] = "> EASY <";
    else if(win32_game_state->selected == DIFFICULTY_NORMAL)
      texts[1] = "> NORMAL <";
    else if(win32_game_state->selected == DIFFICULTY_HARD)
      texts[2] = "> HARD <";
    text_count = DIFFICULTY_COUNT;
  }
  else if(game_state->state == GAME_STATE_WAIT_SERVE) {
    texts[0] = "SERVE";
    if(win32_game_state->selected == WAIT_SERVE_SERVE)
      texts[0] = "> SERVE <";
    text_count = WAIT_SERVE_COUNT;
  }
  else if(game_state->state == GAME_STATE_PAUSE) {
    header = "PAUSED";
    texts[0] = "CONTINUE";
    texts[1] = "RESTART";
    texts[2] = "MAIN MENU";
    if(win32_game_state->selected == PAUSE_CONTINUE)
      texts[0] = "> CONTINUE <";
    else if(win32_game_state->selected == PAUSE_RESTART)
      texts[1] = "> RESTART <";
    else if(win32_game_state->selected == PAUSE_MAIN_MENU)
      texts[2] = "> MAIN MENU <";
    text_count = PAUSE_COUNT;
  }
  else if(game_state->state == GAME_STATE_GAME_OVER) {
    if(game_state->bricks_remaining == 0 && game_state->has_cleared_bricks == true)
      header = "YOU WIN";
    else
      header = "GAME OVER";
    texts[0] = "RESTART";
    texts[1] = "MAIN MENU";
    if(win32_game_state->selected == GAME_OVER_RESTART)
      texts[0] = "> RESTART <";
    else if(win32_game_state->selected == GAME_OVER_MAIN_MENU)
      texts[1] = "> MAIN MENU <";
    text_count = GAME_OVER_COUNT;
  }

  {
    V2 cursor = { PLAYING_AREA_WIDTH/2.0f, PLAYING_AREA_HEIGHT/2.0f };
    if(header) {
      draw_text_centered(header, cursor, 1.5f, COLOR_WHITE, cmd_buffer);
      cursor.y -= LINE_HEIGHT*1.5f * 1.5f;
    }
    for(int i = 0; i < text_count; i++) {
      draw_text_centered(texts[i], cursor, 1.0f, COLOR_WHITE, cmd_buffer);
      cursor.y -= LINE_HEIGHT*1.0f;
    }
  }

  // NOTE(leo): Adjust rect commands to playing area
  {
    V2 playing_area_offset = v2_sub(v2_smul(2.0f, (V2) { playing_area.pos.x/window_client_dim.x, playing_area.pos.y/window_client_dim.y }), (V2) { 1.0f, 1.0f });
    V2 playing_area_dim = v2_smul(2.0f, (V2) { playing_area.dim.x/window_client_dim.x, playing_area.dim.y/window_client_dim.y });
    for(int rect_index = 0; rect_index < cmd_buffer->count; rect_index++) {
      Rect *rect = &cmd_buffer->commands[rect_index].rect;
      rect->pos = v2_add(playing_area_offset, (V2) { rect->pos.x/PLAYING_AREA_WIDTH*playing_area_dim.x, rect->pos.y/PLAYING_AREA_HEIGHT*playing_area_dim.y });
      rect->dim = (V2){ rect->dim.x/PLAYING_AREA_WIDTH*playing_area_dim.x, rect->dim.y/PLAYING_AREA_HEIGHT*playing_area_dim.y };
    }
  }

  return true;
}

bool win32_cursor_hidden(GameMemory *game_memory)
{
  Win32GameState *win32_game_state = (Win32GameState *)&game_memory->memory;
  GameState *game_state = &win32_game_state->game_state;
  return game_state->state == GAME_STATE_PLAYING;
}

void win32_on_lose_focus(GameMemory *game_memory)
{
  Win32GameState *win32_game_state = (Win32GameState *)&game_memory->memory;
  GameState *game_state = &win32_game_state->game_state;

  if(game_state->state == GAME_STATE_PLAYING)
    game_state->state = GAME_STATE_PAUSE;
}
