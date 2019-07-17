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

enum {
  EDGE_LEFT = 1<<0,
  EDGE_BOTTOM = 1<<1,
  EDGE_RIGHT = 1<<2,
  EDGE_TOP = 1<<3
};

typedef struct Impact {
  F32 time;
  U8 edges;
} Impact;

internal
Impact compute_impact(Rect a, V2 delta_a, Rect b, V2 delta_b)
{
  V2 point = v2_add(a.pos, v2_smul(0.5f, a.dim));
  V2 point_delta = v2_sub(delta_a, delta_b);

  Rect rect = (Rect){
    .pos = v2_sub(b.pos, v2_smul(0.5f, a.dim)),
    .dim = v2_add(b.dim, a.dim),
  };

  // NOTE(leo): Compute time of impact of point with each rect edge
  F32 ts[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  if(point_delta.x != 0.0f) {
    ts[0] = (rect.pos.x-point.x)/point_delta.x;
    ts[2] = (rect.pos.x+rect.dim.x - point.x)/point_delta.x;
  }
  if(point_delta.y != 0.0f) {
    ts[1] = (rect.pos.y-point.y)/point_delta.y;
    ts[3] = (rect.pos.y+rect.dim.y - point.y)/point_delta.y;
  }

  F32 toi = 1.0f;
  int edges = 0;
  for(int i = 0; i < 4; i++) {
    F32 t = ts[i];
    // NOTE(leo): Inflate rect for hit testing to combat rounding errors
    F32 inflation = 0.001f;
    Rect inflated = {
      .pos = {rect.pos.x-inflation, rect.pos.y-inflation},
      .dim = {rect.dim.x+2.0f*inflation, rect.dim.y+2.0f*inflation}
    };
    V2 hit = v2_add(point, v2_smul(t, point_delta));
    if(point_inside_rect(hit, inflated)) {
      if(t > 0.0f && t < 1.0f && t <= toi) {
        if(t < toi) {
          edges = 0;
          toi = t;
        }
        edges |= (1<<i);
      }
    }
  }

  return (Impact){ .time = toi, .edges = edges };
}

void reflect_ball(U8 edges, Rect *ball, V2 *ball_direction)
{
  if((edges & EDGE_LEFT) || (edges & EDGE_RIGHT)) {
    if(edges & EDGE_LEFT)
      ball->pos.x += -0.001f;
    else
      ball->pos.x += 0.001f;
    ball_direction->x *= -1.0f;
  }
  if((edges & EDGE_BOTTOM) || (edges & EDGE_TOP)) {
    if(edges & EDGE_BOTTOM)
      ball->pos.y += -0.001f;
    else
      ball->pos.y += 0.001f;
    ball_direction->y *= -1.0f;
  }
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

  F32 scale = 4.0f;

  F32 ball_speeds[4] = { 50.0f, 75.0f, 100.0f, 125.0f };

  typedef struct Brick {
    Rect rect;
    U32 type;
  } Brick;

  local_persist Rect ball;
  local_persist V2 ball_direction;
  local_persist F32 ball_speed;
  local_persist F32 target_ball_speed;

  local_persist Rect paddle;
  local_persist F32 paddle_speed;
  local_persist F32 target_paddle_pos;

  local_persist Brick bricks[BRICK_COUNT_X*BRICK_COUNT_Y];
  local_persist int bricks_remaining;

  local_persist int score;
  local_persist int hit_count;
  local_persist int balls_remaining;
  local_persist bool waiting_for_serve;

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
    target_ball_speed = ball_speeds[0];

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
    balls_remaining = 3;
    waiting_for_serve = true;
  }

  if(waiting_for_serve && input->button_serve.is_down) {
    if(balls_remaining) {
      waiting_for_serve = false;
      ball.pos = (V2){ ARENA_WIDTH/2.0f - BALL_WIDTH/2.0f, PADDLE_Y + 10.0f };
      ball_direction.x = 1.5f* ((F32)rand()/RAND_MAX) - 1.5f/2.0f;
      ball_direction.y = sqrtf(1.0f - ball_direction.x*ball_direction.x);
      ball_speed = 0;
      target_ball_speed = ball_speeds[0];
      paddle.pos = (V2){ ARENA_WIDTH/2.0f - PADDLE_WIDTH/2.0f, PADDLE_Y };
      hit_count = 0;
    }
    else {
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
      target_ball_speed = ball_speeds[0];

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
      balls_remaining = 3;
      waiting_for_serve = true;
    }
  }


  // NOTE(leo): Physics
  if(!waiting_for_serve)
  {
    // NOTE(leo): Compute new paddle speed
    {
      F32 paddle_speed_factor = 20.0f;
      target_paddle_pos = (F32)input->pointer.x/scale - paddle.dim.x/2.0f;
      if(target_paddle_pos < 0.0f)
        target_paddle_pos = 0.0f;
      if(target_paddle_pos > ARENA_WIDTH)
        target_paddle_pos = ARENA_WIDTH;
      F32 add_pos = target_paddle_pos - paddle.pos.x;
      F32 dx = paddle_speed_factor*add_pos*dt;
      if(fabsf(dx) > fabsf(add_pos))
        dx = add_pos;
      paddle_speed = dx/dt;
    }

    // NOTE(leo): Compute new ball speed
    {
      bool too_fast = target_ball_speed < ball_speed;
      F32 ball_add_speed = fabsf(target_ball_speed - ball_speed);
      F32 ball_acceleration = 100.0f;
      F32 ball_accelerate_speed = ball_acceleration*dt;
      if(ball_accelerate_speed > ball_add_speed)
        ball_accelerate_speed = ball_add_speed;
      if(too_fast)
        ball_speed -= ball_accelerate_speed;
      else
        ball_speed += ball_accelerate_speed;
    }

    F32 elapsed = 0.0f;
    int iterations = 0;
    while(elapsed < dt) {
      iterations++;

      F32 step = dt-elapsed;

      V2 ball_delta = v2_smul(step*ball_speed, ball_direction);

      // NOTE(leo): Compute time of impact with up to 3 bricks (eg: hit corner)
      F32 toi_bricks = 1.0f;
      int hit_brick_indices[3] = { -1,-1,-1 };
      U8 hit_brick_edges[3] = { 0, 0, 0 };
      int hit_brick_count = 0;
      {
        for(int brick_index = 0; brick_index < bricks_remaining; brick_index++) {
          Brick *brick = &bricks[brick_index];
          Impact impact = compute_impact(ball, ball_delta, brick->rect, (V2) { 0.0f, 0.0f });
          if(impact.time < 1.0f && impact.time <= toi_bricks) {
            if(impact.time < toi_bricks) {
              hit_brick_count = 0;
              toi_bricks = impact.time;
            }
            hit_brick_indices[hit_brick_count] = brick_index;
            hit_brick_edges[hit_brick_count] = impact.edges;
            hit_brick_count++;
            assert(hit_brick_count <= 3);
          }
        }
      }

      // NOTE(leo): Compute toi for walls
      F32 toi_walls = 1.0f;
      U8 hit_wall_edges = 0;
      {
        Rect walls[4] = {
          { .pos = {0.0f, 0.0f}, .dim = {0.0f, ARENA_HEIGHT} },
          { .pos = {0.0f, 0.0f}, .dim = {ARENA_WIDTH, 0.0f} },
          { .pos = {ARENA_WIDTH, 0.0f}, .dim = {0.0f, ARENA_HEIGHT} },
          { .pos = {0.0f, ARENA_HEIGHT}, .dim = {ARENA_WIDTH, 0.0f} },
        };
        for(int i = 0; i < 4; i++) {
          Impact impact = compute_impact(ball, ball_delta, walls[i], (V2) { 0.0f, 0.0f });
          F32 t = impact.time;
          if(t < 1.0f && t <= toi_walls) {
            if(t < toi_walls) {
              toi_walls = impact.time;
              hit_wall_edges = 0;
            }
            hit_wall_edges |= impact.edges;
          }
        }
      }

      // NOTE(leo): Compute toi_paddle
      F32 toi_paddle = 1.0f;
      U8 hit_paddle_edges = 0;
      {
        V2 paddle_delta = { step*paddle_speed, 0.0f };
        if(paddle.pos.x + paddle_delta.x < 0.0f)
          paddle_delta.x = 0.0f - paddle.pos.x;
        else if(paddle.pos.x + paddle_delta.x > ARENA_WIDTH - paddle.dim.x)
          paddle_delta.x = ARENA_WIDTH - paddle.dim.x - paddle.pos.x;
        Impact impact = compute_impact(ball, ball_delta, paddle, paddle_delta);
        toi_paddle = impact.time;
        hit_paddle_edges = impact.edges;
      }

      // NOTE(leo): choose smallest toi
      F32 toi_min = 1.0f;
      bool hit_bricks = false;
      bool hit_walls = false;
      bool hit_paddle = false;
      if(toi_bricks < toi_min) {
        hit_bricks = true;
        toi_min = toi_bricks;
      }
      if(toi_walls < toi_min) {
        // NOTE(leo): Impossible to hit bricks and walls at same time
        hit_bricks = false;
        hit_walls = true;
        toi_min = toi_walls;
      }
      if(toi_paddle < 1.0f && toi_paddle <= toi_min) {
        // NOTE(leo) Possible to hit wall and paddle at same time
        if(toi_paddle < toi_min) {
          hit_bricks = false;
          hit_walls = false;
        }
        hit_paddle = true;
        toi_min = toi_paddle;
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

      if(hit_bricks) {
        U8 edges = 0;
        for(int i = 0; i < hit_brick_count; i++)
          edges |= hit_brick_edges[i];
        reflect_ball(edges, &ball, &ball_direction);
        hit_count += hit_brick_count;

        // NOTE(leo): Attribute score for hitting brick; Max ball speed if orange or red brick
        for(int i = 0; i < hit_brick_count; i++) {
          Brick *brick = &bricks[hit_brick_indices[i]];
          if(brick->type == 0) {
            score += 1;
          }
          else if(brick->type == 1) {
            score += 3;
          }
          else if(brick->type == 2) {
            score += 5;
            target_ball_speed = ball_speeds[3];
          }
          else if(brick->type == 3) {
            score += 7;
            target_ball_speed = ball_speeds[3];
          }
        }

        // NOTE(leo): Destroy hit bricks
        for(int i = 0; i < hit_brick_count; i++)
          bricks[hit_brick_indices[i]] = bricks[--bricks_remaining];
      }

      if(hit_walls) {
        reflect_ball(hit_wall_edges, &ball, &ball_direction);
        if((hit_wall_edges & EDGE_LEFT) || (hit_wall_edges & EDGE_RIGHT))
          hit_count++;
        if((hit_wall_edges & EDGE_BOTTOM) || (hit_wall_edges & EDGE_TOP))
          hit_count++;

        // NOTE(leo): Paddle shrinking
        if(hit_wall_edges & EDGE_BOTTOM && paddle.dim.x == PADDLE_WIDTH) {
          F32 new_width = PADDLE_WIDTH/2.0f;
          paddle.pos.x += PADDLE_WIDTH/2.0f - new_width/2.0f;
          paddle.dim.x = new_width;
        }

        if(hit_wall_edges & EDGE_TOP) {
          balls_remaining--;
          waiting_for_serve = true;
        }
      }

      if(hit_paddle) {
        hit_count++;
        if(hit_paddle_edges & EDGE_TOP) {
          V2 left = ball.pos;
          V2 right = v2_add(ball.pos, (V2) { ball.dim.x, 0.0f });
          if(left.x < paddle.pos.x)
            left.x = paddle.pos.x;
          if(right.x > paddle.pos.x + paddle.dim.x)
            right.x = paddle.pos.x + paddle.dim.x;
          V2 mid = v2_add(v2_smul(0.5f, left), v2_smul(0.5f, right));
          F32 hit_normalized = (mid.x - paddle.pos.x)/paddle.dim.x;
          // NOTE(leo): 0: -45 degs, 1: 45 degs, in between: lerp. Relative to paddle normal
          F32 PI = 3.14159f;
          F32 angle = hit_normalized*(PI/2.0f - PI/4.0f) + (1.0f - hit_normalized)*(PI/2.0f + PI/4.0f);
          ball_direction.x = cosf(angle);
          ball_direction.y = sinf(angle);
        }
        else if(hit_paddle_edges & EDGE_LEFT || hit_paddle_edges & EDGE_RIGHT) {
          /*
            Elastic collision:
              m1 = 1, m2 = inf
              s1 = ball_speed_x, s2 = paddle_speed
              u1 = ball_speed_x - paddle_speed, u2 = 0
              v1 = (m1-m2)/(m1+m2)*u1 + (2*m2)/(m1+m2)*u2 = -1*(ball_speed_x - paddle_speed)
              v2 = (2*m1)/(m1+m2)*u1 + (m2-m1)/(m1+m2)*u2 = 0
              v1 = -ball_speed_x + paddle_speed
              v2 = 0
              w1 = -ball_speed_x + 2*paddle_speed
              w2 = paddle_speed
          */
          V2 w1 = { -ball_speed*ball_direction.x + 2*paddle_speed, ball_speed*ball_direction.y };
          ball_speed = sqrtf(w1.x*w1.x + w1.y*w1.y);
          ball_direction = v2_smul(1.0f/ball_speed, w1);
          if(hit_paddle_edges & EDGE_LEFT)
            ball.pos.x += -0.001f;
          else
            ball.pos.x += 0.001f;
          paddle_speed = 0.0f;
        }
        else {
          reflect_ball(hit_paddle_edges, &ball, &ball_direction);
        }
      }

      // TODO(leo): Prevent paddle from pushing ball into wall

      // NOTE(leo): Ball speed
      if(hit_count == 4 && target_ball_speed < ball_speeds[1])
        target_ball_speed = ball_speeds[1];
      else if(hit_count == 12 && target_ball_speed < ball_speeds[2])
        target_ball_speed = ball_speeds[2];

      elapsed += step;
    }
    assert(iterations < 25);
  }

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
  if(!waiting_for_serve)
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
