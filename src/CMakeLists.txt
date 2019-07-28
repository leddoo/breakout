if(WIN32)
  add_executable(breakout
    breakout.c
    win32_main.c
    win32_breakout.c
  )
  target_link_libraries(breakout PRIVATE Winmm Opengl32)
  target_compile_options(breakout PRIVATE /MT)
  target_link_options(breakout PRIVATE /SUBSYSTEM:WINDOWS)
else()
  message(FATAL_ERROR "Platform not supported!")
endif()
