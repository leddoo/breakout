#include "util.h"

typedef struct Image {
  U32 *memory;
  int width, height;
  int pitch;
} Image;


typedef struct ButtonState {
  bool is_down;
} ButtonState;

typedef struct PointerState {
  int x, y;
  bool is_hovering;
  bool is_pressing;
} PointerState;

typedef struct Input {
  ButtonState button_left;
  ButtonState button_right;
  ButtonState button_serve;
  PointerState pointer;
} Input;
