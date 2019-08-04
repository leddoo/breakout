#pragma once

#include "util.h"
#include "renderer.h"

#include <Windows.h>

typedef struct Button {
  bool is_down, was_down;
} Button;

typedef struct Win32Input {
  V2 mouse;
  Button key_escape;
  Button key_return;
  Button key_space;
  Button key_up;
  Button key_down;
} Win32Input;

#define RENDER_CMD_BUFFER_COUNT 1234

bool win32_game_update(GameMemory *game_memory, F32 dt, Win32Input *input, HWND win32_window, RenderCmdBuffer *cmd_buffer);

bool win32_cursor_hidden(GameMemory *game_memory);

void win32_on_lose_focus(GameMemory *game_memory);
