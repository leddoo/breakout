#pragma once

#include "util.h"

typedef struct RectangleCmd {
  Rect rect;
  Color color;
} RectangleCmd;

typedef struct RenderCmdBuffer {
  RectangleCmd *commands;
  int count;
  int capacity;
} RenderCmdBuffer;
