#include "util.h"
#include "platform.h"

#include <math.h>
#include <stdlib.h>

typedef struct V2 {
  F32 x, y;
} V2;

inline
V2 v2_add(V2 a, V2 b)
{
  return (V2) { a.x + b.x, a.y + b.y };
}

inline
V2 v2_sub(V2 a, V2 b)
{
  return (V2) { a.x - b.x, a.y - b.y };
}

inline
V2 v2_smul(F32 s, V2 v)
{
  return (V2) { s *v.x, s *v.y };
}

typedef struct Rect {
  V2 pos, dim;
} Rect;

typedef struct Segment {
  V2 p1, p2;
} Segment;

typedef struct Color {
  F32 r, g, b, a;
} Color;

internal
void draw_rectangle(V2 min, V2 max, F32 r, F32 g, F32 b, Image *image)
{
  int x0 = (int)roundf(min.x);
  int y0 = (int)roundf(min.y);
  int x1 = (int)roundf(max.x);
  int y1 = (int)roundf(max.y);

  if(x1 < 0 || y1 < 0 || x0 >= image->width || y0 >= image->height)
    return;

  if(x0 < 0)
    x0 = 0;
  if(y0 < 0)
    y0 = 0;
  if(x1 > image->width)
    x1 = image->width;
  if(y1 > image->height)
    y1 = image->height;

  U32 color = ((U8)(r*255.0f) << 16) | ((U8)(g*255.0f) << 8) | ((U8)(b*255.0f) << 0);
  for(int y = y0; y < y1; y++) {
    for(int x = x0; x < x1; x++)
      image->memory[y*image->pitch + x] = color;
  }
}

inline
bool point_inside_rect(V2 point, Rect rect)
{
  return (point.x >= rect.pos.x)
      && (point.x <= rect.pos.x + rect.dim.x)
      && (point.y >= rect.pos.y)
      && (point.y <= rect.pos.y + rect.dim.y);
}

void game_update(F32 dt, Input *input, Image *image)
{
  /*
      - wall collision like previously, make thick walls part of surrounding graphics -> arena where ball can move is only the black part of the image
      - inner arena: 1150x1390      => 111x140
      - paddle: 72x30               => 7x3
      - ball: 20x16                 => 2x1.5
      - brick: 72x20 delta: 12x10   => 7x2, 1x0.8
      - first brick height: 932     => 90
      - paddle height: 62           => 6
      - num bricks: 14x8
      - brick colors: (0.77, 0.78, 0.09), (0, 0.5, 0.13), (0.76, 0.51, 0), (0.63, 0.04, 0)
  */

#define BRICK_COUNT_X 14
#define BRICK_COUNT_Y 8
#define FIRST_BRICK_HEIGHT 90.0f
#define BRICK_WIDTH 7.0f
#define BRICK_HEIGHT 2.0f
#define BRICK_DELTA_X 1.0f
#define BRICK_DELTA_Y 0.8f

#define PADDLE_WIDTH 7.0f
#define PADDLE_HEIGTH 3.0f
#define PADDLE_Y 6.0f

#define PADDLE_COLOR_R 0.0f
#define PADDLE_COLOR_G 0.5f
#define PADDLE_COLOR_B 0.78f

#define BALL_WIDTH 2.0f
#define BALL_HEIGHT 1.5f

#define BALL_COLOR_R 0.82f
#define BALL_COLOR_G 0.82f
#define BALL_COLOR_B 0.82f

#define ARENA_WIDTH 111.0f
#define ARENA_HEIGHT 140.0f

  typedef struct Brick {
    Rect rect;
    U32 type;
  } Brick;

  local_persist Rect ball;
  local_persist V2 ball_direction;
  local_persist F32 ball_speed;

  local_persist Rect paddle;
  local_persist F32 paddle_speed;

  local_persist Brick bricks[BRICK_COUNT_X*BRICK_COUNT_Y];

  // NOTE(leo): initialization
  local_persist bool initialized = false;
  if(!initialized)
  {
    initialized = true;

    // NOTE(leo): Paddle
    paddle = (Rect){
      .pos = { ARENA_WIDTH/2.0f - PADDLE_WIDTH/2.0f, PADDLE_Y },
      .dim = { PADDLE_WIDTH, PADDLE_HEIGTH }
    };

    // NOTE(leo): Ball
    ball = (Rect){
      .pos = { ARENA_WIDTH/2.0f - BALL_WIDTH/2.0f, PADDLE_Y + 10.0f },
      .dim = { BALL_WIDTH, BALL_HEIGHT }
    };
    ball_direction.x = 1.5f* ((F32)rand()/RAND_MAX) - 1.5f/2.0f;
    ball_direction.y = sqrtf(1.0f - ball_direction.x*ball_direction.x);
    ball_speed = 200.0f;

    // TEMP
    ball_direction.x = -0.3f;
    ball_direction.y = -sqrtf(1.0f - ball_direction.x*ball_direction.x);
    paddle_speed = 30.0f;

    // NOTE(leo): Bricks
    {
      F32 ypos = FIRST_BRICK_HEIGHT;
      for(int y = 0; y < BRICK_COUNT_Y; y++) {
        F32 xpos = 0.0f;
        for(int x = 0; x < BRICK_COUNT_X; x++) {
          bricks[y*BRICK_COUNT_X + x] = (Brick){
            .rect = {
              .pos = (V2){xpos, ypos},
              .dim = (V2){BRICK_WIDTH, BRICK_HEIGHT} },
            .type = y/2
          };

          xpos += BRICK_WIDTH + BRICK_DELTA_X;
        }
        ypos += BRICK_HEIGHT + BRICK_DELTA_Y;
      }
    }
  }


  // NOTE(leo): Physics
  local_persist bool was_down;
  if(input->button_left.is_down && !was_down)
  {
    /*
      Physics iteration:
        - find potential collisions: ball sweep vs paddle sweep, bricks, walls(not bottom)
        - find contact time for each potential collision
        - advance to first contact time (or to end of iteration)
        - compute impatc position, bounce ball, record bounce at position
    */

    // NOTE(leo): Compute new paddle speed
    // TEMP
    if(false)
    {
      // Friction
      do {
        F32 stop_speed = 2.0f;
        F32 friction = 0.1f;

        F32 speed = fabsf(paddle_speed);
        if(speed < 0.5f) {
          paddle_speed = 0.0f;
          break;
        }

        if(speed < stop_speed)
          speed = stop_speed;
        F32 drop = speed*friction*dt;
        F32 new_speed = speed - drop;
        if(new_speed < 0.0f)
          new_speed = 0.0f;
        paddle_speed *= new_speed/speed;
      } while(false);

      // Paddle Acceleration
      do {
        F32 acceleration = 0.2f;
        F32 wish_dir = 0.0f;
        F32 paddle_max_speed = 10.0f;
        if(input->button_left.is_down)
          wish_dir -= 1.0f;
        if(input->button_right.is_down)
          wish_dir += 1.0f;

        F32 current_speed = paddle_speed * wish_dir;
        F32 add_speed = paddle_max_speed - current_speed;
        if(add_speed <= 0.0f)
          break;
        F32 accelerate_speed = acceleration*paddle_max_speed*dt;
        if(accelerate_speed > add_speed)
          accelerate_speed = add_speed;

        paddle_speed += accelerate_speed*wish_dir;
      } while(false);
    }

    F32 elapsed = 0.0f;
    while(elapsed < dt) {
      F32 step = dt-elapsed;

      V2 ball_delta = v2_smul(step*ball_speed, ball_direction);
      // NOTE(leo): Compute toi_paddle
      F32 toi_paddle = 1.0f;
      int paddle_edge = -1;
      {
        V2 paddle_delta = { step*paddle_speed, 0.0f };

        V2 point = v2_add(ball.pos, v2_smul(0.5f, ball.dim));
        V2 point_delta = v2_sub(ball_delta, paddle_delta);

        Rect rect = (Rect){
          .pos = v2_sub(paddle.pos, v2_smul(0.5f, ball.dim)),
          .dim = v2_add(paddle.dim, ball.dim),
        };

        // 0-3: left, bottom, right, top
        F32 ts[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        if(point_delta.x != 0.0f) {
          ts[0] = (rect.pos.x-point.x)/point_delta.x;
          ts[2] = (rect.pos.x+rect.dim.x - point.x)/point_delta.x;
        }
        if(point_delta.y != 0.0f) {
          ts[1] = (rect.pos.y-point.y)/point_delta.y;
          ts[3] = (rect.pos.y+rect.dim.y - point.y)/point_delta.y;
        }
        for(int i = 0; i < 4; i++) {
          F32 t = ts[i];
          if(t > 0.0f && t < toi_paddle) {
            toi_paddle = t;
            paddle_edge = i;
          }
        }
        V2 hit = v2_add(point, v2_smul(toi_paddle, point_delta));
        if(!point_inside_rect(hit, rect)) {
          toi_paddle = 1.0f;
          paddle_edge = -1;
        }
      }
      step *= toi_paddle;

      // NOTE(leo): integrate by step
      ball.pos = v2_add(ball.pos, v2_smul(step*ball_speed, ball_direction));
      paddle.pos = v2_add(paddle.pos, (V2){ step*paddle_speed, 0.0f });

      if(paddle_edge == 0 || paddle_edge == 2)
        ball_direction.x = -ball_direction.x;
      else if(paddle_edge == 1 || paddle_edge == 3)
        ball_direction.y = -ball_direction.y;

      elapsed += step;
      // TEMP
      break;
    }
  }
  // TEMP
  was_down = input->button_left.is_down;

  // NOTE(leo): Draw bricks
  Color brick_colors[4] = { (Color){ 0.77f, 0.78f, 0.09f, 1.0f }, (Color){ 0.0f, 0.5f, 0.13f, 1.0f }, (Color){ 0.76f, 0.51f, 0.0f, 1.0f }, (Color){ 0.63f, 0.04f, 0.0f, 1.0f } };
  for(int y = 0; y < BRICK_COUNT_Y; y++) {
    for(int x = 0; x < BRICK_COUNT_X; x++) {
      Brick *brick = &bricks[y*BRICK_COUNT_X + x];
      Color *color = &brick_colors[brick->type];
      draw_rectangle(v2_smul(5.0f, brick->rect.pos), v2_smul(5.0f, v2_add(brick->rect.pos, brick->rect.dim)), color->r, color->g, color->b, image);
    }
  }

  // NOTE(leo): Draw paddle
  draw_rectangle(v2_smul(5.0f, paddle.pos), v2_smul(5.0f, v2_add(paddle.pos, paddle.dim)), PADDLE_COLOR_R, PADDLE_COLOR_G, PADDLE_COLOR_B, image);

  // NOTE(leo): Draw ball
  draw_rectangle(v2_smul(5.0f, ball.pos), v2_smul(5.0f, v2_add(ball.pos, ball.dim)), BALL_COLOR_R, BALL_COLOR_G, BALL_COLOR_B, image);
}
