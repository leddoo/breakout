#include "util.h"
#include "platform.h"

#include <windows.h>
#include <dsound.h>

typedef struct Win32ImageBuffer {
  BITMAPINFO info;
  U32 *memory;
  int width, height;
  int pitch;
} Win32ImageBuffer;

typedef struct Win32SoundBuffer {
  LPDIRECTSOUNDBUFFER ds_buffer;
  U32 sample_count;
  U32 size; // NOTE(leo): byte count
} Win32SoundBuffer;


global_variable bool global_running;
global_variable Win32ImageBuffer global_image_buffer;
global_variable Win32SoundBuffer global_sound_buffer;

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

internal void win32_sound_buffer_init(HWND window, U32 sample_count)
{
  typedef HRESULT WINAPI direct_sound_create_proc(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

  // NOTE(leo): Load the library
  HMODULE lib = LoadLibraryA("dsound.dll");
  if(!lib) {
    // TODO(leo): Better error handling
    MessageBoxA(window, "Direct Sound could not be loaded!", "Warning", MB_OK|MB_ICONWARNING);
  }

  //NOTE(leo): Load the procedures
  direct_sound_create_proc *create = (direct_sound_create_proc *)GetProcAddress(lib, "DirectSoundCreate");
  if(!create) {
    // TODO(leo): Better error handling
    OutputDebugStringA("direct_sound_init: A procedure was not found!\n");
    exit(1);
  }

  // NOTE(leo): Create direct sound object
  LPDIRECTSOUND ds;
  if(!SUCCEEDED(create(0, &ds, NULL))) {
    // TODO(leo): Better error handling
    OutputDebugStringA("direct_sound_init: create failed\n");
    exit(1);
  }
  if(!SUCCEEDED(ds->lpVtbl->SetCooperativeLevel(ds, window, DSSCL_PRIORITY))) {
    // TODO(leo): Better error handling
    OutputDebugStringA("direct_sound_init: failed to set cooperative level\n");
    exit(1);
  }

  // NOTE(leo): Create primary buffer (~handle to sound card)
  DSBUFFERDESC primary_buffer_description = {
    .dwSize = sizeof(primary_buffer_description),
    .dwFlags = DSBCAPS_PRIMARYBUFFER,
  };
  LPDIRECTSOUNDBUFFER primary_buffer;
  if(!SUCCEEDED(ds->lpVtbl->CreateSoundBuffer(ds, &primary_buffer_description, &primary_buffer, 0))) {
    // TODO(leo): Better error handling
    OutputDebugStringA("direct_sound_init: failed to create primary buffer\n");
    exit(1);
  }

  // NOTE(leo): Set primary buffer wave format
  WAVEFORMATEX wave_format = {
    .wFormatTag = WAVE_FORMAT_PCM,
    .nChannels = PLATFORM_SOUND_BUFFER_CHANNEL_COUNT,
    .nSamplesPerSec = PLATFORM_SOUND_BUFFER_SAMPLE_FREQUENCY,
    .wBitsPerSample = PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE*8,
    .cbSize = 0
  };
  wave_format.nBlockAlign = wave_format.nChannels*wave_format.wBitsPerSample/8;
  wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec*wave_format.nBlockAlign,
  primary_buffer->lpVtbl->SetFormat(primary_buffer, &wave_format);

  // NOTE(leo): Create global sound buffer
  DSBUFFERDESC sound_buffer_description = {
    .dwSize = sizeof(sound_buffer_description),
    .dwBufferBytes = PLATFORM_SOUND_BUFFER_SIZE(sample_count),
    .lpwfxFormat = &wave_format
  };
  if(!SUCCEEDED(ds->lpVtbl->CreateSoundBuffer(ds, &sound_buffer_description, &global_sound_buffer.ds_buffer, 0))) {
    // TODO(leo): Better error handling
    OutputDebugStringA("direct_sound_init: failed to create secondary buffer\n");
    exit(1);
  }
  global_sound_buffer.sample_count = sample_count;
  global_sound_buffer.size = PLATFORM_SOUND_BUFFER_SIZE(sample_count);
}

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
    case WM_KEYDOWN:
    case WM_KEYUP: {
      U32 vk = w_param;
      bool was_down = !!(l_param & (1<<30));
      bool is_down = !(l_param & (1<<31));
      if(was_down == is_down)
        break;
      if(vk == 'W' || vk == VK_UP) {
      }
      else if(vk == 'A' || vk == VK_LEFT) {
      }
      else if(vk == 'S' || vk == VK_DOWN) {
      }
      else if(vk == 'D' || vk == VK_RIGHT) {
      }
    } break;

    case WM_PAINT: {
      RECT rect;
      if(!GetUpdateRect(window, &rect, FALSE))
        break;
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(window, &paint);
      int x = rect.left;
      int y = rect.top;
      int w = rect.right - rect.left;
      int h = rect.bottom - rect.top;
      int ret = StretchDIBits(
        dc,
        x, y, w, h,
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

void game_update(F32 dt, PlatformImageBuffer *image, PlatformSoundBuffer *sound);

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

  win32_image_buffer_init(1280, 720);
  U32 sound_buffer_sample_count = PLATFORM_SOUND_BUFFER_SAMPLE_FREQUENCY*1;
  win32_sound_buffer_init(main_window, sound_buffer_sample_count);
  HRESULT ret = global_sound_buffer.ds_buffer->lpVtbl->Play(global_sound_buffer.ds_buffer, 0, 0, DSBPLAY_LOOPING);
  if(!SUCCEEDED(ret))
    exit(1);

  S16 *game_sound_buffer_memory = VirtualAlloc(NULL, sound_buffer_sample_count*PLATFORM_SOUND_BUFFER_CHANNEL_COUNT*sizeof(game_sound_buffer_memory[0]), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

  global_running = true;
  U32 sample_index = 0;
  U32 latency_sample_count = global_sound_buffer.sample_count;
  while(global_running) {
    MSG message;
    while(PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
      if(message.message==WM_QUIT) {
        global_running = false;
        break;
      }
      TranslateMessage(&message);
      DispatchMessageA(&message);
    }

    DWORD play_cursor, write_cursor;
    HRESULT ret = global_sound_buffer.ds_buffer->lpVtbl->GetCurrentPosition(global_sound_buffer.ds_buffer, &play_cursor, &write_cursor);
    if(!SUCCEEDED(ret))
      exit(1);
    U32 lock_pointer = (sample_index*PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE*PLATFORM_SOUND_BUFFER_CHANNEL_COUNT) % global_sound_buffer.size;
    U32 target_pointer = (play_cursor + latency_sample_count*PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE) % global_sound_buffer.size;
    U32 lock_byte_count;
    if(lock_pointer > target_pointer)
      lock_byte_count = (global_sound_buffer.size - lock_pointer) + target_pointer;
    else
      lock_byte_count = target_pointer - lock_pointer;


    U32 game_sound_sample_count = lock_byte_count/PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE/PLATFORM_SOUND_BUFFER_CHANNEL_COUNT;

    PlatformImageBuffer game_image = {
      .memory = global_image_buffer.memory,
      .width = global_image_buffer.width,
      .height = global_image_buffer.height,
      .pitch = global_image_buffer.pitch,
    };
    PlatformSoundBuffer game_sound = {
      .memory = game_sound_buffer_memory,
      .sample_count = game_sound_sample_count,
    };
    game_update(0.0f, &game_image, &game_sound);


    void *ptr1 = NULL, *ptr2 = NULL;
    DWORD size1 = 0, size2 = 0;
    if(lock_byte_count) {
      ret = global_sound_buffer.ds_buffer->lpVtbl->Lock(
        global_sound_buffer.ds_buffer,
        lock_pointer,
        lock_byte_count,
        &ptr1,
        &size1,
        &ptr2,
        &size2,
        0
      );
      if(!SUCCEEDED(ret))
        exit(1);
    }

    U32 sample1_count = size1/PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE/PLATFORM_SOUND_BUFFER_CHANNEL_COUNT;
    U32 sample2_count = size2/PLATFORM_SOUND_BUFFER_BYTES_PER_SAMPLE/PLATFORM_SOUND_BUFFER_CHANNEL_COUNT;
    S16 *sample = ptr1;
    for(U32 i = 0; i < sample1_count+sample2_count; i++) {
      if(i == sample1_count)
        sample = ptr2;
      *sample++ = game_sound.memory[2*i];
      *sample++ = game_sound.memory[2*i+1];
      sample_index++;
    }
    ret = global_sound_buffer.ds_buffer->lpVtbl->Unlock(global_sound_buffer.ds_buffer, ptr1, size1, ptr2, size2);
    if(!SUCCEEDED(ret))
      exit(1);
    //Sleep(5);
    RedrawWindow(main_window, 0, 0, RDW_INVALIDATE|RDW_INTERNALPAINT);
  }
  return 0;
}
