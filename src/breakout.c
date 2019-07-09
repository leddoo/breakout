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

  F32 ball_speeds[4] = { 50.0f, 75.0f, 100.0f, 125.0f };

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
  local_persist int bricks_remaining;

  local_persist int score;
  local_persist int hit_count;

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
    ball_speed = ball_speeds[0];

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
      bricks_remaining = sizeof(bricks)/sizeof(bricks[0]);
    }

    score = 0;
    hit_count = 0;
  }


  // NOTE(leo): Physics
  {
    // NOTE(leo): Compute new paddle speed
    {
      F32 stop_speed = 2.0f;
      F32 friction = 18.0f;
      F32 acceleration = 6.0f;
      F32 paddle_max_speed = 200.0f;

      F32 wish_dir = 0.0f;
      if(input->button_left.is_down)
        wish_dir -= 1.0f;
      if(input->button_right.is_down)
        wish_dir += 1.0f;

      // Friction
      do {
        // NOTE(leo): No friction while user is pressing buttons.
        if(wish_dir != 0.0f)
          break;

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

      enum {
        EDGE_LEFT = 0,
        EDGE_BOTTOM,
        EDGE_RIGHT,
        EDGE_TOP,
      };

      // NOTE(leo): First toi brick
      F32 toi_brick = 1.0f;
      int hit_brick_index = -1;
      int brick_edge = -1;
      {
        for(int brick_index = 0; brick_index < bricks_remaining; brick_index++) {
          Brick *brick = &bricks[brick_index];

          V2 point = v2_add(ball.pos, v2_smul(0.5f, ball.dim));
          V2 point_delta = ball_delta;

          Rect rect = (Rect){
            .pos = v2_sub(brick->rect.pos, v2_smul(0.5f, ball.dim)),
            .dim = v2_add(brick->rect.dim, ball.dim),
          };

          // 0-3: left, bottom, right, top
          F32 ts[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
          if(point_delta.x != 0.0f) {
            ts[EDGE_LEFT] = (rect.pos.x-point.x)/point_delta.x;
            ts[EDGE_RIGHT] = (rect.pos.x+rect.dim.x - point.x)/point_delta.x;
          }
          if(point_delta.y != 0.0f) {
            ts[EDGE_BOTTOM] = (rect.pos.y-point.y)/point_delta.y;
            ts[EDGE_TOP] = (rect.pos.y+rect.dim.y - point.y)/point_delta.y;
          }
          for(int i = 0; i < 4; i++) {
            F32 t = ts[i];
            if(t > 0.0f && t < toi_brick) {
              V2 hit = v2_add(point, v2_smul(t, point_delta));
              if(point_inside_rect(hit, rect)) {
                toi_brick = t;
                hit_brick_index = brick_index;
                brick_edge = i;
              }
            }
          }
        }
      }

      // NOTE(leo): Compute toi for walls
      F32 toi_left_wall = 1.0f;
      F32 toi_top_wall = 1.0f;
      F32 toi_right_wall = 1.0f;
      {
        if(ball_delta.x != 0.0f) {
          toi_left_wall = (0.0f - ball.pos.x)/ball_delta.x;
          if(toi_left_wall <= 0.0f || toi_left_wall > 1.0f)
            toi_left_wall = 1.0f;
          toi_right_wall = (ARENA_WIDTH-ball.dim.x - ball.pos.x)/ball_delta.x;
          if(toi_right_wall <= 0.0f || toi_right_wall > 1.0f)
            toi_right_wall = 1.0f;
        }
        if(ball_delta.y != 0.0f) {
          toi_top_wall = (ARENA_HEIGHT-ball.dim.y - ball.pos.y)/ball_delta.y;
          if(toi_top_wall <= 0.0f || toi_top_wall > 1.0f)
            toi_top_wall = 1.0f;
        }
      }

      // NOTE(leo): Compute toi_paddle
      F32 toi_paddle = 1.0f;
      int paddle_edge = -1;
      {
        V2 paddle_delta = { step*paddle_speed, 0.0f };
        if(paddle.pos.x + paddle_delta.x < 0.0f)
          paddle_delta.x = 0.0f - paddle.pos.x;
        else if(paddle.pos.x + paddle_delta.x > ARENA_WIDTH - paddle.dim.x)
          paddle_delta.x = ARENA_WIDTH - paddle.dim.x - paddle.pos.x;

        V2 point = v2_add(ball.pos, v2_smul(0.5f, ball.dim));
        V2 point_delta = v2_sub(ball_delta, paddle_delta);

        Rect rect = (Rect){
          .pos = v2_sub(paddle.pos, v2_smul(0.5f, ball.dim)),
          .dim = v2_add(paddle.dim, ball.dim),
        };

        // 0-3: left, bottom, right, top
        F32 ts[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        if(point_delta.x != 0.0f) {
          ts[EDGE_LEFT] = (rect.pos.x-point.x)/point_delta.x;
          ts[EDGE_RIGHT] = (rect.pos.x+rect.dim.x - point.x)/point_delta.x;
        }
        if(point_delta.y != 0.0f) {
          ts[EDGE_BOTTOM] = (rect.pos.y-point.y)/point_delta.y;
          ts[EDGE_TOP] = (rect.pos.y+rect.dim.y - point.y)/point_delta.y;
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

      // TODO(leo): choose smallest toi
      int collision_edge = -1;
      F32 toi_min = 1.0f;
      bool hit_brick = false;
      bool hit_paddle = false;
      if(toi_left_wall < toi_min) {
        toi_min = toi_left_wall;
        collision_edge = EDGE_RIGHT; // NOTE(leo): Left wall is like a right edge
      }
      if(toi_top_wall < toi_min) {
        toi_min = toi_top_wall;
        collision_edge = EDGE_BOTTOM;
      }
      if(toi_right_wall < toi_min) {
        toi_min = toi_right_wall;
        collision_edge = EDGE_LEFT;
      }
      if(toi_paddle < toi_min) {
        toi_min = toi_paddle;
        collision_edge = paddle_edge;
        hit_paddle = true;
      }

      if(toi_brick < toi_min) {
        toi_min = toi_brick;
        collision_edge = brick_edge;
        hit_brick = true;
        hit_paddle = false;
      }

      step *= toi_min;

      // NOTE(leo): integrate by step
      ball.pos = v2_add(ball.pos, v2_smul(step*ball_speed, ball_direction));

      V2 paddle_delta = { step*paddle_speed, 0.0f };
      if(paddle.pos.x + paddle_delta.x < 0.0f)
        paddle_delta.x = 0.0f - paddle.pos.x;
      else if(paddle.pos.x + paddle_delta.x > ARENA_WIDTH - paddle.dim.x)
        paddle_delta.x = ARENA_WIDTH - paddle.dim.x - paddle.pos.x;
      paddle.pos = v2_add(paddle.pos, paddle_delta);

      // NOTE(leo): Change ball direction and move away from edge. (Movement is
      // for corners: if ball moves down and left and hits right bottom corner,
      // ball now would move up and collide at step = 0 in next iteration. This
      // would be ignored and step = 1, the ball moves into the paddle)
      if(collision_edge == EDGE_LEFT) {
        ball_direction.x = -ball_direction.x;
        ball.pos.x += -0.001f;
      }
      else if(collision_edge == EDGE_BOTTOM) {
        ball_direction.y = -ball_direction.y;
        ball.pos.y += -0.001f;
      }
      else if(collision_edge == EDGE_RIGHT) {
        ball_direction.x = -ball_direction.x;
        ball.pos.x += 0.001f;
      }
      else if(collision_edge == EDGE_TOP) {
        ball_direction.y = -ball_direction.y;
        ball.pos.y += 0.001f;
      }

      // NOTE(leo): Check if ball is past bottom edge
      if(ball.pos.y < 0.0f - ball.dim.y) {
        // TEMP
        ball.pos = (V2){ ARENA_WIDTH/2.0f - BALL_WIDTH/2.0f, PADDLE_Y + 10.0f };
        ball_direction.x = 1.5f* ((F32)rand()/RAND_MAX) - 1.5f/2.0f;
        ball_direction.y = sqrtf(1.0f - ball_direction.x*ball_direction.x);
      }

      // NOTE(leo): Hit counting and ball speed
      if(collision_edge >= 0) {
        hit_count++;
        if(hit_count == 4 && ball_speed < ball_speeds[1])
          ball_speed = ball_speeds[1];
        else if(hit_count == 12 && ball_speed < ball_speeds[2])
          ball_speed = ball_speeds[2];
      }

      // NOTE(leo): Paddle shrinking
      if(collision_edge == EDGE_BOTTOM && !hit_brick && !hit_paddle) {
        if(paddle.dim.x == PADDLE_WIDTH) {
          F32 new_width = PADDLE_WIDTH/2.0f;
          paddle.pos.x += PADDLE_WIDTH/2.0f - new_width/2.0f;
          paddle.dim.x = new_width;
        }
      }

      // NOTE(leo): Attribute score for hitting brick; Max ball speed if orange or red brick
      if(hit_brick) {
        Brick *brick = &bricks[hit_brick_index];
        if(brick->type == 0) {
          score += 1;
        }
        else if(brick->type == 1) {
          score += 3;
        }
        else if(brick->type == 2) {
          score += 5;
          ball_speed = ball_speeds[3];
        }
        else if(brick->type == 3) {
          score += 7;
          ball_speed = ball_speeds[3];
        }
      }

      // NOTE(leo): Destroy hit brick
      if(hit_brick)
        bricks[hit_brick_index] = bricks[--bricks_remaining];

      elapsed += step;
    }
  }

  F32 scale = 4.0f;

  // NOTE(leo): Draw bricks
  Color brick_colors[4] = { (Color){ 0.77f, 0.78f, 0.09f, 1.0f }, (Color){ 0.0f, 0.5f, 0.13f, 1.0f }, (Color){ 0.76f, 0.51f, 0.0f, 1.0f }, (Color){ 0.63f, 0.04f, 0.0f, 1.0f } };
  for(int brick_index = 0; brick_index < bricks_remaining; brick_index++) {
    Brick *brick = &bricks[brick_index];
    Color *color = &brick_colors[brick->type];
    draw_rectangle(v2_smul(scale, brick->rect.pos), v2_smul(scale, v2_add(brick->rect.pos, brick->rect.dim)), color->r, color->g, color->b, image);
  }

  // NOTE(leo): Draw paddle
  draw_rectangle(v2_smul(scale, paddle.pos), v2_smul(scale, v2_add(paddle.pos, paddle.dim)), PADDLE_COLOR_R, PADDLE_COLOR_G, PADDLE_COLOR_B, image);

  // NOTE(leo): Draw ball
  draw_rectangle(v2_smul(scale, ball.pos), v2_smul(scale, v2_add(ball.pos, ball.dim)), BALL_COLOR_R, BALL_COLOR_G, BALL_COLOR_B, image);

  // NOTE(leo): Draw score
  {
    int ones = score % 10;
    int tens = (score / 10) % 10;
    int hundreds = (score / 100) % 10;
    int numbers[3] = { hundreds, tens, ones };
    for(int i = 0; i < 3; i++) {
      // TODO(leo): Proper positioning
      // TODO(leo): Move out constants
      V2 pos = { ARENA_WIDTH/2.0f + i*6.0f, ARENA_HEIGHT+4.0f };
      switch(numbers[i]) {
        case 0: {
          /*
              #####
              #---#
              #---#
              #---#
              #---#
              #---#
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 1: {
          /*
              ---#-
              --##-
              ---#-
              ---#-
              ---#-
              ---#-
              --###
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){2.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){2.0f, 5.0f})), v2_smul(scale, v2_add(pos, (V2){ 3.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){3.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 4.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 2: {
          /*
              #####
              ----#
              ----#
              #####
              #----
              #----
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 3: {
          /*
              #####
              ----#
              ----#
              -####
              ----#
              ----#
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){1.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 4: {
          /*
              #---#
              #---#
              #---#
              #####
              ----#
              ----#
              ----#
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 5: {
          /*
              #####
              #----
              #----
              #####
              ----#
              ----#
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 6: {
          /*
              #####
              #----
              #----
              #####
              #---#
              #---#
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 7: {
          /*
              #####
              ----#
              ----#
              ----#
              ----#
              ----#
              ----#
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 8: {
          /*
              #####
              #---#
              #---#
              #####
              #---#
              #---#
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
        case 9: {
          /*
              #####
              #---#
              #---#
              #####
              ----#
              ----#
              #####
          */
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 0.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 1.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 3.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 4.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 6.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 7.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 1.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 3.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){4.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 5.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
          draw_rectangle(v2_smul(scale, v2_add(pos, (V2){0.0f, 4.0f})), v2_smul(scale, v2_add(pos, (V2){ 1.0f, 6.0f })), 1.0f, 1.0f, 1.0f, image);
        } break;
      }
    }
  }
}
