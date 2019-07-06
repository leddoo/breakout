#include "util.h"
#include "platform.h"

#include <windows.h>

typedef struct Win32ImageBuffer {
  BITMAPINFO info;
  U32 *memory;
  int width, height;
  int pitch;
} Win32ImageBuffer;

global_variable bool global_running;
global_variable Win32ImageBuffer global_image_buffer;

internal void win32_image_buffer_init(U32 width, U32 height)
{
  assert(width && height);

  global_image_buffer.info.bmiHeader = (BITMAPINFOHEADER){
    .biSize = sizeof(global_image_buffer.info.bmiHeader),
    .biWidth = width,
    .biHeight = height,
    .biPlanes = 1,
    .biBitCount = 32,
    .biCompression = BI_RGB,
  };
  global_image_buffer.memory = VirtualAlloc(NULL, width*height*sizeof(global_image_buffer.memory[0]), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  global_image_buffer.width = width;
  global_image_buffer.height = height;
  global_image_buffer.pitch = width;
  assert(global_image_buffer.memory);
}

Input global_input;

LRESULT CALLBACK main_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
  LRESULT result = 0;
  switch(message)
  {
    case WM_SIZE: {
      int a;
    } break;
    case WM_CLOSE: {
      global_running = false;
    } break;
    case WM_DESTROY: {
      global_running = false;
    } break;
    case WM_KEYDOWN:
    case WM_KEYUP: {
      U32 vk = w_param;
      bool was_down = !!(l_param & (1<<30));
      bool is_down = !(l_param & (1<<31));
      if(was_down == is_down)
        break;
      if(vk == 'A' || vk == VK_LEFT) {
        global_input.button_left.is_down = is_down;
      }
      else if(vk == 'D' || vk == VK_RIGHT) {
        global_input.button_right.is_down = is_down;
      }
    } break;

    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(window, &paint);
      int ret = StretchDIBits(
        dc,
        0, 0, global_image_buffer.width, global_image_buffer.height,
        0, 0, global_image_buffer.width, global_image_buffer.height,
        global_image_buffer.memory,
        &global_image_buffer.info,
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

void game_update(F32 dt, Input *input, Image *image);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmd_line, int cmd_show)
{
  HWND main_window;
  {
    WNDCLASSA main_window_class = {
      .style = CS_HREDRAW|CS_VREDRAW, // NOTE(leo): Redraw entire window on resize
      .lpfnWndProc = main_window_proc,
      .hInstance = instance,
      .lpszClassName = "main_window_class",
    };

    ATOM main_window_atom = RegisterClassA(&main_window_class);
    assert(main_window_atom);

    RECT client_rect = { 0 };
    client_rect.right = 1280;
    client_rect.bottom = 720;
    BOOL ret = AdjustWindowRectEx(&client_rect, WS_OVERLAPPEDWINDOW|WS_VISIBLE, FALSE, 0);
    assert(ret);

    main_window = CreateWindowExA(
      0,
      main_window_atom,
      "breakout",
      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT,
      client_rect.right-client_rect.left, client_rect.bottom-client_rect.top,
      0,
      0,
      instance,
      NULL
    );
  }

  win32_image_buffer_init(1280, 720);

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
    for(int y = 0; y < global_image_buffer.height; y++) {
      for(int x = 0; x < global_image_buffer.width; x++)
        global_image_buffer.memory[y*global_image_buffer.pitch + x] = 0;
    }

    // NOTE(leo): Update game
    Image game_image = {
      .memory = global_image_buffer.memory,
      .width = global_image_buffer.width,
      .height = global_image_buffer.height,
      .pitch = global_image_buffer.pitch,
    };
    game_update(dt, &global_input, &game_image);
    RedrawWindow(main_window, 0, 0, RDW_INVALIDATE|RDW_INTERNALPAINT);

    // NOTE(leo): Lock frame rate
    {
      LARGE_INTEGER now = { 0 };
      QueryPerformanceCounter(&now);
      F32 dt = (F32)(((F64)now.QuadPart-(F64)last_time.QuadPart)/(F64)timer_frequency.QuadPart);
      int sleep_time = (int)((target_dt - dt)*1000.0f)-1;
      if(sleep_time > 0)
        Sleep(sleep_time);
      do {
        QueryPerformanceCounter(&now);
        dt = (F32)(((F64)now.QuadPart-(F64)last_time.QuadPart)/(F64)timer_frequency.QuadPart);
      } while(dt < target_dt);
    }

    local_persist int frame_count;
    local_persist LARGE_INTEGER last_sec = { 0 };
    frame_count++;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    F32 accu = (F32)(((F64)now.QuadPart-(F64)last_sec.QuadPart)/(F64)timer_frequency.QuadPart);
    if(accu >= 1.0f) {
      F32 fps = frame_count/accu;
      frame_count = 0;
      last_sec = now;
      char buffer[128];
      wsprintfA(buffer, "%d.%d\n", (int)fps, (int)((fps-(int)fps)*1000));
      OutputDebugStringA(buffer);
    }

    LARGE_INTEGER end_time = { 0 };
    QueryPerformanceCounter(&end_time);
    dt = (F32)(((F64)end_time.QuadPart-(F64)last_time.QuadPart)/(F64)timer_frequency.QuadPart);
    last_time = end_time;
  }

  // NOTE(leo): Reset scheduler
  {
    MMRESULT ret = timeEndPeriod(scheduler_granularity);
    assert(ret==TIMERR_NOERROR);
  }

  return 0;
}
