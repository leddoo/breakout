add_executable(breakout "breakout.c")

if(WIN32)
  target_sources(breakout PRIVATE win32_main.c)
  target_link_libraries(breakout PRIVATE Winmm)
  target_link_options(breakout PRIVATE /SUBSYSTEM:WINDOWS)
else()
  message(FATAL_ERROR "Platform not supported!")
endif()
