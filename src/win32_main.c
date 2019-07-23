#include "util.h"
#include "win32_breakout.h"

#include <windows.h>

typedef struct Win32Image {
  BITMAPINFO info;
  U32 *memory;
  int width, height;
  int pitch;
} Win32Image;

global_variable bool global_running;
global_variable Win32Image global_image;
global_variable Win32Input global_input;
global_variable GameMemory global_game_memory;

internal void win32_image_resize(U32 width, U32 height)
{
  assert(width && height);

  global_image.info.bmiHeader = (BITMAPINFOHEADER){
    .biSize = sizeof(global_image.info.bmiHeader),
    .biWidth = width,
    .biHeight = height,
    .biPlanes = 1,
    .biBitCount = 32,
    .biCompression = BI_RGB,
  };
  if(global_image.memory)
    VirtualFree(global_image.memory, 0, MEM_RELEASE);
  global_image.memory = VirtualAlloc(NULL, width*height*sizeof(global_image.memory[0]), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  global_image.width = width;
  global_image.height = height;
  global_image.pitch = width;
  assert(global_image.memory);
}


LRESULT CALLBACK main_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
  LRESULT result = 0;
  switch(message)
  {
    case WM_SIZE: {
      RECT client_rect;
      BOOL ret = GetClientRect(window, &client_rect);
      assert(ret);
      win32_image_resize(client_rect.right-client_rect.left, client_rect.bottom-client_rect.top);
    } break;
    case WM_CLOSE: {
      global_running = false;
    } break;
    case WM_DESTROY: {
      global_running = false;
    } break;
    case WM_KEYUP:
    case WM_KEYDOWN: {
      DWORD vk = w_param;
      bool is_down = !(l_param & (1<<31));
      if(vk == VK_ESCAPE) {
        global_input.key_escape.is_down = is_down;
      }
      else if(vk == VK_RETURN) {
        global_input.key_return.is_down = is_down;
      }
      else if(vk == VK_SPACE) {
        global_input.key_space.is_down = is_down;
      }
      else if(vk == VK_UP) {
        global_input.key_up.is_down = is_down;
      }
      else if(vk == VK_DOWN) {
        global_input.key_down.is_down = is_down;
      }
    } break;
    case WM_SETCURSOR : {
      if(win32_cursor_hidden(&global_game_memory)) {
        SetCursor(NULL);
        result = TRUE;
      }
      else {
        result = DefWindowProcA(window, message, w_param, l_param);
      }
    } break;
    case WM_ACTIVATEAPP: {
      if(w_param == FALSE)
        win32_on_lose_focus(&global_game_memory);
      result = DefWindowProcA(window, message, w_param, l_param);
    } break;
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(window, &paint);
      int ret = StretchDIBits(
        dc,
        0, 0, global_image.width, global_image.height,
        0, 0, global_image.width, global_image.height,
        global_image.memory,
        &global_image.info,
        DIB_RGB_COLORS,
        SRCCOPY
      );
      assert(ret);
      EndPaint(window, &paint);
    } break;
    default: {
      result = DefWindowProcA(window, message, w_param, l_param);
    }
  }
  return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmd_line, int cmd_show)
{
  HWND main_window;
  {
    WNDCLASSA main_window_class = {
      .style = CS_HREDRAW|CS_VREDRAW, // NOTE(leo): Redraw entire window on resize
      .lpfnWndProc = main_window_proc,
      .hInstance = instance,
      .lpszClassName = "main_window_class",
      .hCursor = LoadCursorA(NULL, IDC_ARROW),
    };

    ATOM main_window_atom = RegisterClassA(&main_window_class);
    assert(main_window_atom);

    main_window = CreateWindowExA(
      0,
      main_window_atom,
      "breakout",
      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT,
      CW_USEDEFAULT, CW_USEDEFAULT,
      0,
      0,
      instance,
      NULL
    );
  }

  LARGE_INTEGER last_time = { 0 };
  LARGE_INTEGER timer_frequency = { 0 };
  F32 dt = 0.0f;
  F32 target_dt = 1.0f/60.0f;
  QueryPerformanceCounter(&last_time);
  QueryPerformanceFrequency(&timer_frequency);

  // NOTE(leo): Set scheduler granularity to minimum
  UINT scheduler_granularity;
  {
    TIMECAPS time_caps;
    MMRESULT ret = timeGetDevCaps(&time_caps, sizeof(time_caps));
    assert(ret==MMSYSERR_NOERROR);
    scheduler_granularity = time_caps.wPeriodMin;
    ret = timeBeginPeriod(scheduler_granularity);
    assert(ret==TIMERR_NOERROR);
  }

  global_running = true;
  while(global_running) {
    global_input.key_escape.was_down = global_input.key_escape.is_down;
    global_input.key_return.was_down = global_input.key_return.is_down;
    global_input.key_space.was_down = global_input.key_space.is_down;
    global_input.key_up.was_down = global_input.key_up.is_down;
    global_input.key_down.was_down = global_input.key_down.is_down;

    // NOTE(leo): Handle messages
    MSG message;
    while(PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
      if(message.message==WM_QUIT) {
        global_running = false;
        break;
      }
      TranslateMessage(&message);
      DispatchMessageA(&message);
    }

    // NOTE(leo): Clear image
    for(int y = 0; y < global_image.height; y++) {
      for(int x = 0; x < global_image.width; x++)
        global_image.memory[y*global_image.pitch + x] = 0;
    }


    // NOTE(leo): Query mouse pos (required, as WM_MOUSEMOVE seems to be late when using SetCursorPos sometimes)
    POINT mouse;
    GetCursorPos(&mouse);
    ScreenToClient(main_window, &mouse);
    global_input.mouse = (V2){ mouse.x, mouse.y };

    // NOTE(leo): Update game
    Image game_image = {
      .memory = global_image.memory,
      .width = global_image.width,
      .height = global_image.height,
      .pitch = global_image.pitch,
    };
    bool keep_running = win32_game_update(&global_game_memory, dt, &global_input, &game_image, main_window);
    if(!keep_running)
      global_running = false;
    RedrawWindow(main_window, 0, 0, RDW_INVALIDATE|RDW_INTERNALPAINT);

    // NOTE(leo): Lock frame rate and calculate dt
    {
      LARGE_INTEGER now = { 0 };
      QueryPerformanceCounter(&now);
      dt = (F32)(((F64)now.QuadPart-(F64)last_time.QuadPart)/(F64)timer_frequency.QuadPart);
      int sleep_time = (int)((target_dt - dt)*1000.0f)-1;
      if(sleep_time > 0)
        Sleep(sleep_time);
      do {
        QueryPerformanceCounter(&now);
        dt = (F32)(((F64)now.QuadPart-(F64)last_time.QuadPart)/(F64)timer_frequency.QuadPart);
      } while(dt < target_dt);
      last_time = now;
    }
  }

  // NOTE(leo): Reset scheduler
  {
    MMRESULT ret = timeEndPeriod(scheduler_granularity);
    assert(ret==TIMERR_NOERROR);
  }

  return 0;
}
