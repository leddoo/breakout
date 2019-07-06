#include "util.h"

typedef struct Image {
  U32 *memory;
  int width, height;
  int pitch;
} Image;


typedef struct ButtonState {
  bool is_down;
} ButtonState;

typedef struct Input {
  ButtonState button_left;
  ButtonState button_right;
} Input;
