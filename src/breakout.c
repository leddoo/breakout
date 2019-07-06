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
V2 v2_smul(F32 s, V2 v)
{
  return (V2) { s *v.x, s *v.y };
}

internal
void draw_rectangle(F32 min_x, F32 min_y, F32 max_x, F32 max_y, U32 color, Image *image)
{
  int x0 = (int)roundf(min_x);
  int y0 = (int)roundf(min_y);
  int x1 = (int)roundf(max_x);
  int y1 = (int)roundf(max_y);

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

  for(int y = y0; y < y1; y++) {
    for(int x = x0; x < x1; x++)
      image->memory[y*image->pitch + x] = color;
  }
}

void game_update(F32 dt, Input *input, Image *image)
{
  F32 arena_width = 1280;
  F32 arena_height = 720;

  typedef struct Block {
    V2 pos;
    U32 color;
    bool dead;
  } Block;

  #define BLOCK_COUNT_X 12
  #define BLOCK_COUNT_Y 8

  F32 block_width = arena_width/BLOCK_COUNT_X;
  F32 block_height = 25;

  F32 paddle_y = 50.0f;
  F32 paddle_width = 100;
  F32 paddle_height = 10;
  F32 paddle_max_speed = 2000.0f;

  V2 initial_ball_pos = { arena_width/2.0f, paddle_y + 50.0f };
  F32 ball_radius = 5;

  local_persist F32 paddle_x;
  local_persist F32 paddle_dx;

  local_persist V2 ball_pos;
  local_persist F32 ball_speed;
  local_persist V2 ball_direction;

  local_persist Block blocks[BLOCK_COUNT_X*BLOCK_COUNT_Y];

  local_persist initialized = false;
  if(!initialized) {
    initialized = true;

    paddle_x = arena_width/2.0f;

    ball_pos = initial_ball_pos;
    ball_direction.x = 1.5f* ((F32)rand()/RAND_MAX) - 1.5f/2.0f;
    ball_direction.y = sqrtf(1.0f - ball_direction.x*ball_direction.x);
    ball_speed = 500.0f;

    for(int y = 0; y < BLOCK_COUNT_Y; y++) {
      U32 row_colors[BLOCK_COUNT_Y] = {
        //AARRGGBB
        0xFFFFFF00, // yellow
        0xFF00FF00, // green
        0xFFFFA500, // orange
        0xFFFF0000, // red
      };
      U32 row_color = row_colors[y/2];
      F32 row_y = arena_height*7.0f/8.0f - (BLOCK_COUNT_Y-y)*block_height;
      for(int x = 0; x < BLOCK_COUNT_X; x++)
        blocks[y*BLOCK_COUNT_X + x] = (Block){ (V2){block_width*(x+0.5f), row_y}, row_color, false };
    }
  }

  // Paddle Friction
  do {
    F32 stop_speed = 50.0f;
    F32 friction = 6.0f;

    F32 speed = fabsf(paddle_dx);
    if(speed < 1.0f) {
      paddle_dx = 0.0f;
      break;
    }

    if(speed < stop_speed)
      speed = stop_speed;
    F32 drop = speed*friction*dt;
    F32 new_speed = speed - drop;
    if(new_speed < 0.0f)
      new_speed = 0.0f;
    paddle_dx *= new_speed/speed;
  } while(false);

  // Paddle Acceleration
  do {
    F32 acceleration = 3.5f;
    F32 wish_dir = 0.0f;
    if(input->button_left.is_down)
      wish_dir -= 1.0f;
    if(input->button_right.is_down)
      wish_dir += 1.0f;

    F32 current_speed = paddle_dx * wish_dir;
    F32 add_speed = paddle_max_speed - current_speed;
    if(add_speed <= 0.0f)
      break;
    F32 accelerate_speed = acceleration*paddle_max_speed*dt;
    if(accelerate_speed > add_speed)
      accelerate_speed = add_speed;

    paddle_dx += accelerate_speed*wish_dir;
  } while(false);

  // Integrate paddle position
  paddle_x = paddle_x + paddle_dx*dt;

  // Keep paddle in bounds
  if(paddle_x <= 0.0f + paddle_width/2.0f)
    paddle_x = 0.0f + paddle_width/2.0f;
  if(paddle_x >= arena_width - paddle_width/2.0f)
    paddle_x = arena_width - paddle_width/2.0f;

  // Integrate ball position
  ball_pos = v2_add(ball_pos, v2_smul(dt*ball_speed, ball_direction));

  // Ball - wall interaction
  //  left
  if(ball_pos.x < 0.0f + ball_radius) {
    ball_pos.x = 0.0f + ball_radius;
    if(ball_direction.x < 0.0f)
      ball_direction.x = -ball_direction.x;
  }
  //  top
  if(ball_pos.y > arena_height - ball_radius) {
    ball_pos.y = arena_height - ball_radius;
    if(ball_direction.y > 0.0f)
      ball_direction.y = -ball_direction.y;
  }
  //  right
  if(ball_pos.x > arena_width - ball_radius) {
    ball_pos.x = arena_width - ball_radius;
    if(ball_direction.x > 0.0f)
      ball_direction.x = -ball_direction.x;
  }
  //  bottom
  if(ball_pos.y < 0.0f + ball_radius) {
    ball_pos = initial_ball_pos;
    ball_direction.x = 1.5f* ((F32)rand()/RAND_MAX) - 1.5f/2.0f;
    ball_direction.y = sqrtf(1.0f - ball_direction.x*ball_direction.x);
  }

  // Ball - paddle interaction
  do {
    if(ball_pos.x < paddle_x - paddle_width/2.0f - ball_radius)
      break;
    if(ball_pos.x > paddle_x + paddle_width/2.0f + ball_radius)
      break;
    if(ball_pos.y > paddle_y + paddle_height/2.0f + ball_radius)
      break;
    // left edge
    if((ball_pos.x < paddle_x - paddle_width/2.0f) && (ball_pos.y < paddle_y + paddle_height/2.0f)) {
      ball_direction.x = -ball_direction.x;
      ball_pos.x = paddle_x - paddle_width/2.0f - ball_radius;
    }
    // right edge
    else if((ball_pos.x > paddle_x + paddle_width/2.0f) && (ball_pos.y < paddle_y + paddle_height/2.0f)) {
      ball_direction.x = -ball_direction.x;
      ball_pos.x = paddle_x + paddle_width/2.0f + ball_radius;
    }
    // top (bottom should be impossible)
    else {
      ball_direction.y = -ball_direction.y;
      ball_pos.y = paddle_y + paddle_height/2.0f + ball_radius;
    }
    // Apply friction
    ball_direction.x += paddle_dx/paddle_max_speed;
    F32 length = sqrt(ball_direction.x*ball_direction.x + ball_direction.y*ball_direction.y);
    ball_direction.x /= length;
    ball_direction.y /= length;
  } while(false);

  // Ball - block interaction
  for(int y = 0; y < BLOCK_COUNT_Y; y++) {
    for(int x = 0; x < BLOCK_COUNT_X; x++) {
      Block *block = &blocks[y*BLOCK_COUNT_X + x];
      if(block->dead)
        continue;
      if(ball_pos.x < block->pos.x - block_width/2.0f - ball_radius)
        continue;
      if(ball_pos.y < block->pos.y - block_height/2.0f - ball_radius)
        continue;
      if(ball_pos.x > block->pos.x + block_width/2.0f + ball_radius)
        continue;
      if(ball_pos.y > block->pos.y + block_height/2.0f + ball_radius)
        continue;
      block->dead = true;

      // left edge
      if(ball_pos.x < block->pos.x - block_width/2.0f) {
        ball_direction.x = -ball_direction.x;
        ball_pos.x = block->pos.x - block_width/2.0f - ball_radius;
      }
      // bottom edge
      if(ball_pos.y < block->pos.y - block_height/2.0f) {
        ball_direction.y = -ball_direction.y;
        ball_pos.y = block->pos.y - block_height/2.0f - ball_radius;
      }
      // right edge
      if(ball_pos.x > block->pos.x + block_width/2.0f) {
        ball_direction.x = -ball_direction.x;
        ball_pos.x = block->pos.x + block_width/2.0f + ball_radius;
      }
      // top edge
      if(ball_pos.y > block->pos.y + block_height/2.0f) {
        ball_direction.y = -ball_direction.y;
        ball_pos.y = block->pos.y + block_height/2.0f + ball_radius;
      }
    }
  }

  // NOTE(leo): Draw arena
  {
    int x0 = 0;
    int y0 = 0;
    int x1 = arena_width;
    int y1 = arena_height;
    if(x0 < 0)
      x0 = 0;
    if(y0 < 0)
      y0 = 0;
    if(x1 >= image->width)
      x1 = image->width - 1;
    if(y1 >= image->height)
      y1 = image->height - 1;
    for(int x = x0; x <= x1; x++) {
        image->memory[y0*image->pitch + x] = 0xFFFFFFFF;
        image->memory[y1*image->pitch + x] = 0xFFFFFFFF;
    }
    for(int y = y0; y <= y1; y++) {
        image->memory[y*image->pitch + x0] = 0xFFFFFFFF;
        image->memory[y*image->pitch + x1] = 0xFFFFFFFF;
    }
  }

  // NOTE(leo): Draw paddle
  {
    F32 x0 = paddle_x - paddle_width/2.0f;
    F32 y0 = paddle_y - paddle_height/2.0f;
    F32 x1 = paddle_x + paddle_width/2.0f;
    F32 y1 = paddle_y + paddle_height/2.0f;
    draw_rectangle(x0, y0, x1, y1, 0xFFFFFFFF, image);
  }


  // NOTE(leo): Draw blocks
  for(int y = 0; y < BLOCK_COUNT_Y; y++) {
    for(int x = 0; x < BLOCK_COUNT_X; x++) {
      Block *block = &blocks[y*BLOCK_COUNT_X + x];
      if(!block->dead) {
        F32 x0 = block->pos.x - block_width/2.0f;
        F32 y0 = block->pos.y - block_height/2.0f;
        F32 x1 = block->pos.x + block_width/2.0f;
        F32 y1 = block->pos.y + block_height/2.0f;
        draw_rectangle(x0, y0, x1, y1, block->color, image);
      }
    }
  }

  // NOTE(leo): Draw ball
  //draw_circle(ball_pos.x, ball_pos.y, ball_radius, 0xFFFFFFF, image);
  {
    int x0 = ball_pos.x - ball_radius;
    int y0 = ball_pos.y - ball_radius;
    int x1 = ball_pos.x + ball_radius;
    int y1 = ball_pos.y + ball_radius;
    draw_rectangle(x0, y0, x1, y1, 0xFFFFFFFF, image);
  }
}
