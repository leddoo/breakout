#include "util.h"
#include "win32_breakout.h"

#include <windows.h>

#include <gl/GL.h>

global_variable bool global_running;
global_variable Win32Input global_input;
global_variable GameMemory global_game_memory;

LRESULT CALLBACK main_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
  LRESULT result = 0;
  switch(message)
  {
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
    default: {
      result = DefWindowProcA(window, message, w_param, l_param);
    }
  }
  return result;
}

void win32_opengl_init(HDC dc)
{
  PIXELFORMATDESCRIPTOR pfd = {
    .nSize = sizeof(pfd),
    .nVersion = 1,
    .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    .iPixelType = PFD_TYPE_RGBA,
    .cColorBits = 32,
    .cRedShift = 16,
    .cGreenShift = 8,
    .cBlueShift = 0,
    .cAlphaShift = 24,
  };
  int pixel_format = ChoosePixelFormat(dc, &pfd);
  assert(pixel_format);

  BOOL ret = SetPixelFormat(dc, pixel_format, &pfd);
  assert(ret == TRUE);

  HGLRC glrc = wglCreateContext(dc);
  assert(glrc);

  ret = wglMakeCurrent(dc, glrc);
  assert(ret == TRUE);

  // NOTE(leo): Enable vsync
  {
    const char *extensions = (char *)glGetString(GL_EXTENSIONS);

    if(strstr(extensions, "WGL_EXT_swap_control") != NULL) {
      typedef BOOL(APIENTRY *PFNWGLSWAPINTERVALPROC)(int);
      PFNWGLSWAPINTERVALPROC wglSwapIntervalEXT = 0;
      wglSwapIntervalEXT = (PFNWGLSWAPINTERVALPROC)wglGetProcAddress("wglSwapIntervalEXT");

      if(wglSwapIntervalEXT)
        wglSwapIntervalEXT(1);
    }
  }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmd_line, int cmd_show)
{
  HWND main_window;
  {
    WNDCLASSA main_window_class = {
      .style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC, // NOTE(leo): Redraw entire window on resize
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

  HDC main_window_dc = GetDC(main_window);
  assert(main_window_dc);

  win32_opengl_init(main_window_dc);

  LARGE_INTEGER last_time = { 0 };
  LARGE_INTEGER timer_frequency = { 0 };
  F32 dt = 0.0f;
  QueryPerformanceCounter(&last_time);
  QueryPerformanceFrequency(&timer_frequency);

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

    // NOTE(leo): Query mouse pos (required, as WM_MOUSEMOVE seems to be late when using SetCursorPos sometimes)
    POINT mouse;
    GetCursorPos(&mouse);
    ScreenToClient(main_window, &mouse);
    global_input.mouse = (V2){ mouse.x, mouse.y };

    // NOTE(leo): Calculate dt
    {
      LARGE_INTEGER now = { 0 };
      QueryPerformanceCounter(&now);
      dt = (F32)(((F64)now.QuadPart-(F64)last_time.QuadPart)/(F64)timer_frequency.QuadPart);
      last_time = now;
    }

    // NOTE(leo): Update game
    bool keep_running = win32_game_update(&global_game_memory, dt, &global_input, main_window);
    if(!keep_running)
      global_running = false;

    // NOTE(leo): Draw image to window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SwapBuffers(main_window_dc);

    char buffer[42];
    wsprintfA(buffer, "%d\n", (int)(1000.0f * dt));
    OutputDebugStringA(buffer);
  }

  return 0;
}
