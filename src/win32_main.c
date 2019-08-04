#include "util.h"
#include "win32_breakout.h"

#include <windows.h>

#include <gl/GL.h>

#define GL_ARRAY_BUFFER 0x8892

typedef size_t GLsizeiptr;
typedef intptr_t GLintptr;
typedef void (GLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const void *data);

GLBUFFERSUBDATAPROC *glBufferSubData;

#define FLOATS_PER_POSITION 2
#define FLOATS_PER_COLOR 4
#define FLOATS_PER_VERTEX (FLOATS_PER_POSITION+FLOATS_PER_COLOR)
#define VERTICES_PER_TRIANGLE 3
#define FLOATS_PER_TRIANGLE (VERTICES_PER_TRIANGLE*FLOATS_PER_VERTEX)
#define TRIANGLES_PER_RECT 2
#define FLOATS_PER_RECT (TRIANGLES_PER_RECT*FLOATS_PER_TRIANGLE)

global_variable bool global_running;
global_variable Win32Input global_input;
global_variable GameMemory global_game_memory;
global_variable RectangleCmd global_rectangle_commands_data[RENDER_CMD_BUFFER_COUNT];
global_variable float global_vertex_buffer_data[FLOATS_PER_RECT*RENDER_CMD_BUFFER_COUNT];
global_variable bool global_active;

LRESULT CALLBACK main_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
  LRESULT result = 0;
  switch(message)
  {
    case WM_SIZE: {
      WORD mask = -1;
      WORD width = l_param & mask;
      WORD height = (l_param >> (sizeof(WORD)*8)) & mask;
      glViewport(0, 0, width, height);
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
      if(w_param == FALSE) {
        win32_on_lose_focus(&global_game_memory);
        global_active = false;
      }
      else {
        global_active = true;
      }
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
  // NOTE(leo): Create gl context
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
  }

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

  #define GL_STREAM_DRAW 0x88E0
  #define GL_VERTEX_SHADER 0x8B31
  #define GL_FRAGMENT_SHADER 0x8B30
  #define GL_COMPILE_STATUS 0x8B81
  #define GL_LINK_STATUS 0x8B82

  typedef char GLchar;

  typedef void (GLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
  typedef void (GLBINDBUFFERPROC) (GLenum target, GLuint buffer);
  typedef void (GLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
  typedef GLuint(GLCREATESHADERPROC) (GLenum type);
  typedef void (GLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
  typedef void (GLCOMPILESHADERPROC) (GLuint shader);
  typedef void (GLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
  typedef void (GLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
  typedef GLuint(GLCREATEPROGRAMPROC) (void);
  typedef void (GLATTACHSHADERPROC) (GLuint program, GLuint shader);
  typedef void (GLLINKPROGRAMPROC) (GLuint program);
  typedef void (GLUSEPROGRAMPROC) (GLuint program);
  typedef void (GLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
  typedef void (GLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
  typedef GLint(GLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
  typedef void (GLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
  typedef void (GLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

  GLGENBUFFERSPROC *glGenBuffers = NULL;
  GLBINDBUFFERPROC *glBindBuffer = NULL;
  GLBUFFERDATAPROC *glBufferData = NULL;
  GLCREATESHADERPROC *glCreateShader = NULL;
  GLSHADERSOURCEPROC *glShaderSource = NULL;
  GLCOMPILESHADERPROC *glCompileShader = NULL;
  GLGETSHADERIVPROC *glGetShaderiv = NULL;
  GLGETSHADERINFOLOGPROC *glGetShaderInfoLog = NULL;
  GLCREATEPROGRAMPROC *glCreateProgram = NULL;
  GLATTACHSHADERPROC *glAttachShader = NULL;
  GLLINKPROGRAMPROC *glLinkProgram = NULL;
  GLUSEPROGRAMPROC *glUseProgram = NULL;
  GLVERTEXATTRIBPOINTERPROC *glVertexAttribPointer = NULL;
  GLENABLEVERTEXATTRIBARRAYPROC *glEnableVertexAttribArray = NULL;
  GLGETATTRIBLOCATIONPROC *glGetAttribLocation = NULL;
  GLGETPROGRAMIVPROC *glGetProgramiv = NULL;
  GLGETPROGRAMINFOLOGPROC *glGetProgramInfoLog = NULL;

  // NOTE(leo): Load required functions
  {
    #define LOAD(fun) do {                                                                  \
      fun = wglGetProcAddress(#fun);                                                        \
      if(!fun) {                                                                            \
        MessageBoxA(NULL, "Missing OpenGL function: " #fun, "Error!", MB_OK|MB_ICONERROR);  \
        exit(1);                                                                            \
      }                                                                                     \
    } while(false)

    LOAD(glGenBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glBufferSubData);
    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glUseProgram);
    LOAD(glVertexAttribPointer);
    LOAD(glEnableVertexAttribArray);
    LOAD(glGetAttribLocation);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);

    #undef LOAD
  }

  // NOTE(leo): Create vertex buffer
  {
    GLuint vbo;

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(global_vertex_buffer_data), NULL, GL_STREAM_DRAW);
  }

  // NOTE(leo): Create shaders
  {
    const char *vertex_shader_code =
      "#version 110\n"
      "attribute vec2 position;"
      "attribute vec4 color;"
      "void main()"
      "{"
      "  gl_Position = vec4(position, 0.0, 1.0);"
      "  gl_FrontColor = color;"
      "}";

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_code, NULL);
    glCompileShader(vertex_shader);

    GLint compile_status;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);

    char buffer[512];
    glGetShaderInfoLog(vertex_shader, sizeof(buffer), NULL, buffer);
    OutputDebugStringA("Vertex shader compile log:\n");
    OutputDebugStringA(buffer);

    if(compile_status != GL_TRUE)
      exit(1);

    const char *fragment_shader_code =
      "#version 110\n"
      "void main()"
      "{"
      "  gl_FragColor = gl_Color;"
      "}";

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_code, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);

    glGetShaderInfoLog(fragment_shader, sizeof(buffer), NULL, buffer);
    OutputDebugStringA("Fragment shader compile log:\n");
    OutputDebugStringA(buffer);

    if(compile_status != GL_TRUE)
      exit(1);

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    GLint link_status;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

    glGetProgramInfoLog(shader_program, sizeof(buffer), NULL, buffer);
    OutputDebugStringA("Shader program link log:\n");
    OutputDebugStringA(buffer);

    if(link_status != GL_TRUE)
      exit(1);

    glUseProgram(shader_program);


    GLint position_attr = glGetAttribLocation(shader_program, "position");
    glVertexAttribPointer(position_attr, FLOATS_PER_POSITION, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX*sizeof(float), 0);
    glEnableVertexAttribArray(position_attr);

    GLint color_attr = glGetAttribLocation(shader_program, "color");
    glVertexAttribPointer(color_attr, FLOATS_PER_COLOR, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX*sizeof(float), (const void *)(FLOATS_PER_POSITION*sizeof(float)));
    glEnableVertexAttribArray(color_attr);
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
    if(!global_running)
      break;

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
    RenderCmdBuffer cmd_buffer = {
      .commands = &global_rectangle_commands_data[0],
      .count = 0,
      .capacity = RENDER_CMD_BUFFER_COUNT,
    };
    bool keep_running = win32_game_update(&global_game_memory, dt, &global_input, main_window, &cmd_buffer);
    if(!keep_running)
      global_running = false;

    // NOTE(leo): Draw game
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    {
      int rect_count = cmd_buffer.count;
      float *cursor = &global_vertex_buffer_data[0];
      for(int rect_index = 0; rect_index < rect_count; rect_index++) {
        RectangleCmd rect_cmd = cmd_buffer.commands[rect_index];
        Rect rect = rect_cmd.rect;
        Color color = rect_cmd.color;

        #define PUT_VERTEX(pos, color) do {\
          *cursor++ = (pos).x; \
          *cursor++ = (pos).y; \
          *cursor++ = (color).r; \
          *cursor++ = (color).g; \
          *cursor++ = (color).b; \
          *cursor++ = (color).a; \
        } while(false)

        V2 bottom_left = rect.pos;
        V2 bottom_right = { rect.pos.x + rect.dim.x, rect.pos.y };
        V2 top_left = { rect.pos.x, rect.pos.y + rect.dim.y };
        V2 top_right = { rect.pos.x + rect.dim.x, rect.pos.y + rect.dim.y };

        PUT_VERTEX(bottom_left, color);
        PUT_VERTEX(bottom_right, color);
        PUT_VERTEX(top_left, color);
        PUT_VERTEX(bottom_right, color);
        PUT_VERTEX(top_left, color);
        PUT_VERTEX(top_right, color);

        #undef PUT_VERTEX
      }

      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*FLOATS_PER_RECT*rect_count, &global_vertex_buffer_data[0]);
      glDrawArrays(GL_TRIANGLES, 0, TRIANGLES_PER_RECT*VERTICES_PER_TRIANGLE*rect_count);
    }

    SwapBuffers(main_window_dc);

    if(!global_active)
      Sleep(100);

    char buffer[42];
    wsprintfA(buffer, "%d\n", (int)(1000.0f * dt));
    OutputDebugStringA(buffer);
  }

  return 0;
}
