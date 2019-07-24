#include "breakout.h"

#include "util.h"
#include "symbol_grids.h"

#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

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

void choose_random_ball_direction(V2 *ball_direction)
{
  ball_direction->x = 1.5f* ((F32)rand()/RAND_MAX) - 1.5f/2.0f;
  ball_direction->y = sqrtf(1.0f - ball_direction->x*ball_direction->x);
}

void spawn_bricks(GameState *game_state)
{
  F32 ypos = FIRST_BRICK_HEIGHT;
  for(int y = 0; y < BRICK_COUNT_Y; y++) {
    F32 xpos = BRICK_DELTA_X;
    for(int x = 0; x < BRICK_COUNT_X; x++) {
      game_state->bricks[y*BRICK_COUNT_X + x] = (Brick){
        .rect = {
          .pos = (V2){xpos, ypos},
          .dim = (V2){BRICK_WIDTH, BRICK_HEIGHT} },
        .type = y/2
      };

      xpos += BRICK_WIDTH + BRICK_DELTA_X;
    }
    ypos += BRICK_HEIGHT + BRICK_DELTA_Y;
  }
  game_state->bricks_remaining = BRICK_COUNT_X*BRICK_COUNT_Y;
}

void reset_ball(GameState *game_state)
{
  game_state->ball.pos = INITIAL_BALL_POS;
  choose_random_ball_direction(&game_state->ball_direction);
  game_state->ball_speed = 0.0f;
  game_state->target_ball_speed = BALL_SPEED_1;
}

void game_start(GameState *game_state)
{
  game_state->state = GAME_STATE_WAIT_SERVE;

  reset_ball(game_state);

  game_state->paddle.pos = INITIAL_PADDLE_POS;

  spawn_bricks(game_state);

  game_state->hit_count = 0;
  game_state->score = 0;
  game_state->balls_remaining = 3;
}

void game_serve(GameState *game_state)
{
  game_state->state = GAME_STATE_PLAYING;

  reset_ball(game_state);

  game_state->hit_count = 0;
}

void change_paddle_width(GameState *game_state, F32 new_width)
{
  game_state->paddle.pos.x += game_state->paddle.dim.x/2.0f;
  game_state->paddle.pos.x -= new_width/2.0f;
  game_state->paddle.dim.x = new_width;
}

void game_update(GameState *game_state, F32 dt, Input *input, Image *image, Rect playing_area)
{
  // NOTE(leo): initialization
  if(game_state->state == GAME_STATE_UNINITIALIZED)
  {
    game_state->state = GAME_STATE_MAIN_MENU;

    srand(time(0));

    // NOTE(leo): Paddle
    game_state->paddle = (Rect){
      .pos = INITIAL_PADDLE_POS,
      .dim = { PADDLE_WIDTH, PADDLE_HEIGTH }
    };

    // NOTE(leo): Ball
    game_state->ball = (Rect){
      .pos = INITIAL_BALL_POS,
      .dim = { BALL_WIDTH, BALL_HEIGHT }
    };

    // NOTE(leo): Bricks
    spawn_bricks(game_state);

    game_state->balls_remaining = 3;
  }

  // NOTE(leo): Ball lost, animate paddle back
  if(game_state->state == GAME_STATE_BALL_LOST) {
    F32 target_paddle_pos = INITIAL_PADDLE_POS.x;
    F32 target_paddle_width = PADDLE_WIDTH;
    // NOTE(leo): Width
    {
      F32 add_width = target_paddle_width - game_state->paddle.dim.x;
      F32 dw = 20.0f*add_width*dt;
      if(fabsf(dw) > fabsf(add_width))
        dw = add_width;
      change_paddle_width(game_state, game_state->paddle.dim.x + dw);
    }
    // NOTE(leo): Position
    {
      F32 paddle_speed_factor = 20.0f;
      F32 add_pos = target_paddle_pos - game_state->paddle.pos.x;
      F32 dx = paddle_speed_factor*add_pos*dt;
      if(fabsf(dx) > fabsf(add_pos))
        dx = add_pos;
      game_state->paddle.pos.x += dx;
    }
    if(fabsf(target_paddle_pos - game_state->paddle.pos.x) < 0.001f
      && fabsf(target_paddle_width - game_state->paddle.dim.x) < 0.001f) {
      game_state->state = GAME_STATE_WAIT_SERVE;
      reset_ball(game_state);
      game_state->paddle.pos.x = target_paddle_pos;
    }
  }


  // NOTE(leo): Physics
  if(game_state->state == GAME_STATE_PLAYING)
  {
    // NOTE(leo): Compute new paddle speed
    F32 paddle_speed;
    {
      F32 paddle_speed_factor = 20.0f;
      F32 target_paddle_pos = (F32)input->paddle_control*(ARENA_WIDTH-game_state->paddle.dim.x);
      if(target_paddle_pos < 0.0f)
        target_paddle_pos = 0.0f;
      if(target_paddle_pos > ARENA_WIDTH)
        target_paddle_pos = ARENA_WIDTH;
      F32 add_pos = target_paddle_pos - game_state->paddle.pos.x;
      F32 dx = paddle_speed_factor*add_pos*dt;
      if(fabsf(dx) > fabsf(add_pos))
        dx = add_pos;
      paddle_speed = dx/dt;
    }

    // NOTE(leo): Compute new ball speed
    {
      bool too_fast = game_state->target_ball_speed < game_state->ball_speed;
      F32 ball_add_speed = fabsf(game_state->target_ball_speed - game_state->ball_speed);
      F32 ball_acceleration = 100.0f;
      F32 ball_accelerate_speed = ball_acceleration*dt;
      if(ball_accelerate_speed > ball_add_speed)
        ball_accelerate_speed = ball_add_speed;
      if(too_fast)
        game_state->ball_speed -= ball_accelerate_speed;
      else
        game_state->ball_speed += ball_accelerate_speed;
    }

    F32 elapsed = 0.0f;
    int iterations = 0;
    while(elapsed < dt) {
      iterations++;

      F32 step = dt-elapsed;

      V2 ball_delta = v2_smul(step*game_state->ball_speed, game_state->ball_direction);

      // NOTE(leo): Compute time of impact with up to 3 bricks (eg: hit corner)
      F32 toi_bricks = 1.0f;
      int hit_brick_indices[3] = { -1,-1,-1 };
      U8 hit_brick_edges[3] = { 0, 0, 0 };
      int hit_brick_count = 0;
      {
        for(int brick_index = 0; brick_index < game_state->bricks_remaining; brick_index++) {
          Brick *brick = &game_state->bricks[brick_index];
          Impact impact = compute_impact(game_state->ball, ball_delta, brick->rect, (V2) { 0.0f, 0.0f });
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
          Impact impact = compute_impact(game_state->ball, ball_delta, walls[i], (V2) { 0.0f, 0.0f });
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
        if(game_state->paddle.pos.x + paddle_delta.x < 0.0f)
          paddle_delta.x = 0.0f - game_state->paddle.pos.x;
        else if(game_state->paddle.pos.x + paddle_delta.x > ARENA_WIDTH - game_state->paddle.dim.x)
          paddle_delta.x = ARENA_WIDTH - game_state->paddle.dim.x - game_state->paddle.pos.x;
        Impact impact = compute_impact(game_state->ball, ball_delta, game_state->paddle, paddle_delta);
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
      game_state->ball.pos = v2_add(game_state->ball.pos, v2_smul(step*game_state->ball_speed, game_state->ball_direction));

      V2 paddle_delta = { step*paddle_speed, 0.0f };
      if(game_state->paddle.pos.x + paddle_delta.x < 0.0f)
        paddle_delta.x = 0.0f - game_state->paddle.pos.x;
      else if(game_state->paddle.pos.x + paddle_delta.x > ARENA_WIDTH - game_state->paddle.dim.x)
        paddle_delta.x = ARENA_WIDTH - game_state->paddle.dim.x - game_state->paddle.pos.x;
      game_state->paddle.pos = v2_add(game_state->paddle.pos, paddle_delta);


      // NOTE(leo): Reflect off bricks
      if(hit_bricks) {
        U8 edges = 0;
        for(int i = 0; i < hit_brick_count; i++)
          edges |= hit_brick_edges[i];
        reflect_ball(edges, &game_state->ball, &game_state->ball_direction);
        game_state->hit_count += hit_brick_count;

        // NOTE(leo): Attribute score for hitting brick; Max ball speed if orange or red brick
        for(int i = 0; i < hit_brick_count; i++) {
          Brick *brick = &game_state->bricks[hit_brick_indices[i]];
          if(brick->type == 0) {
            game_state->score += 1;
          }
          else if(brick->type == 1) {
            game_state->score += 3;
          }
          else if(brick->type == 2) {
            game_state->score += 5;
            if(game_state->target_ball_speed < BALL_SPEED_4)
              game_state->target_ball_speed = BALL_SPEED_4;
          }
          else if(brick->type == 3) {
            game_state->score += 7;
            if(game_state->target_ball_speed < BALL_SPEED_4)
              game_state->target_ball_speed = BALL_SPEED_4;
          }
        }

        // NOTE(leo): Destroy hit bricks
        for(int i = 0; i < hit_brick_count; i++)
          game_state->bricks[hit_brick_indices[i]] = game_state->bricks[--game_state->bricks_remaining];
      }

      // NOTE(leo): Reflect off walls
      if(hit_walls) {
        reflect_ball(hit_wall_edges, &game_state->ball, &game_state->ball_direction);
        if((hit_wall_edges & EDGE_LEFT) || (hit_wall_edges & EDGE_RIGHT))
          game_state->hit_count++;
        if((hit_wall_edges & EDGE_BOTTOM) || (hit_wall_edges & EDGE_TOP))
          game_state->hit_count++;

        // NOTE(leo): Paddle shrinking
        if(hit_wall_edges & EDGE_BOTTOM && game_state->paddle.dim.x == PADDLE_WIDTH) {
          change_paddle_width(game_state, PADDLE_WIDTH/2.0f);
        }

        // NOTE(leo): Round over
        if(hit_wall_edges & EDGE_TOP) {
          game_state->balls_remaining--;
          if(game_state->balls_remaining)
            game_state->state = GAME_STATE_BALL_LOST;
          else
            game_state->state = GAME_STATE_GAME_OVER;

          elapsed = dt;
          break;
        }
      }

      // NOTE(leo): Interact with paddle
      if(hit_paddle) {
        game_state->hit_count++;
        if(hit_paddle_edges & EDGE_TOP) {
          V2 left = game_state->ball.pos;
          V2 right = v2_add(game_state->ball.pos, (V2) { game_state->ball.dim.x, 0.0f });
          if(left.x < game_state->paddle.pos.x)
            left.x = game_state->paddle.pos.x;
          if(right.x > game_state->paddle.pos.x + game_state->paddle.dim.x)
            right.x = game_state->paddle.pos.x + game_state->paddle.dim.x;
          V2 mid = v2_add(v2_smul(0.5f, left), v2_smul(0.5f, right));
          F32 hit_normalized = (mid.x - game_state->paddle.pos.x)/game_state->paddle.dim.x;
          // NOTE(leo): 0: -45 degs, 1: 45 degs, in between: lerp. Relative to paddle normal
          F32 PI = 3.14159f;
          F32 angle = hit_normalized*(PI/2.0f - PI/4.0f) + (1.0f - hit_normalized)*(PI/2.0f + PI/4.0f);
          game_state->ball_direction.x = cosf(angle);
          game_state->ball_direction.y = sinf(angle);
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
          V2 w1 = { -game_state->ball_speed*game_state->ball_direction.x + 2*paddle_speed, game_state->ball_speed*game_state->ball_direction.y };
          game_state->ball_speed = sqrtf(w1.x*w1.x + w1.y*w1.y);
          game_state->ball_direction = v2_smul(1.0f/game_state->ball_speed, w1);
          if(hit_paddle_edges & EDGE_LEFT)
            game_state->ball.pos.x += -0.001f;
          else
            game_state->ball.pos.x += 0.001f;
        }
        else {
          reflect_ball(hit_paddle_edges, &game_state->ball, &game_state->ball_direction);
        }
      }

      // TODO(leo): Prevent paddle from pushing ball into wall

      // NOTE(leo): Ball speed
      if(game_state->hit_count == 4 && game_state->target_ball_speed < BALL_SPEED_2)
        game_state->target_ball_speed = BALL_SPEED_2;
      else if(game_state->hit_count == 12 && game_state->target_ball_speed < BALL_SPEED_3)
        game_state->target_ball_speed = BALL_SPEED_3;

      elapsed += step;
      assert(iterations < 25);
    }
  }

  Image playing_area_image = *image;
  playing_area_image.memory = &image->memory[(int)playing_area.pos.y*image->pitch + (int)playing_area.pos.x];
  F32 scale = playing_area.dim.x/PLAYING_AREA_WIDTH;
  V2 arena_offset = v2_smul(scale, (V2) { 2.0f, 0.0f });

  // NOTE(leo): Draw arena
  draw_rectangle(v2_smul(scale, (V2){0.0f, 0.0f}), v2_smul(scale, (V2){2.0f, PLAYING_AREA_HEIGHT}), 1.0f, 1.0f, 1.0f, &playing_area_image);
  draw_rectangle(v2_smul(scale, (V2){2.0f+ARENA_WIDTH, 0.0f}), v2_smul(scale, (V2){2.0f+ARENA_WIDTH+2.0f, PLAYING_AREA_HEIGHT}), 1.0f, 1.0f, 1.0f, &playing_area_image);

  // NOTE(leo): Draw bricks
  Color brick_colors[4] = { (Color){ 0.77f, 0.78f, 0.09f, 1.0f }, (Color){ 0.0f, 0.5f, 0.13f, 1.0f }, (Color){ 0.76f, 0.51f, 0.0f, 1.0f }, (Color){ 0.63f, 0.04f, 0.0f, 1.0f } };
  for(int brick_index = 0; brick_index < game_state->bricks_remaining; brick_index++) {
    Brick *brick = &game_state->bricks[brick_index];
    Color *color = &brick_colors[brick->type];
    draw_rectangle(v2_add(arena_offset, v2_smul(scale, brick->rect.pos)), v2_add(arena_offset, v2_smul(scale, v2_add(brick->rect.pos, brick->rect.dim))), color->r, color->g, color->b, &playing_area_image);
  }

  // NOTE(leo): Draw paddle
  draw_rectangle(v2_add(arena_offset, v2_smul(scale, game_state->paddle.pos)), v2_add(arena_offset, v2_smul(scale, v2_add(game_state->paddle.pos, game_state->paddle.dim))), PADDLE_COLOR_R, PADDLE_COLOR_G, PADDLE_COLOR_B, &playing_area_image);

  // NOTE(leo): Draw ball
  if(game_state->state != GAME_STATE_BALL_LOST)
    draw_rectangle(v2_add(arena_offset, v2_smul(scale, game_state->ball.pos)), v2_add(arena_offset, v2_smul(scale, v2_add(game_state->ball.pos, game_state->ball.dim))), BALL_COLOR_R, BALL_COLOR_G, BALL_COLOR_B, &playing_area_image);

  // NOTE(leo): Draw score
  {
    char buffer[4];
    buffer[0] = (game_state->score / 100) % 10 + '0';
    buffer[1] = (game_state->score / 10) % 10 + '0';
    buffer[2] = (game_state->score / 1) % 10 + '0';
    buffer[3] = 0;
    V2 cursor = v2_add(playing_area.pos, playing_area.dim);
    cursor.x -= (5*SYMBOL_WIDTH + 2*SYMBOL_SPACING)*scale;
    cursor.y -= (3*SYMBOL_WIDTH + 2*SYMBOL_SPACING)*scale;
    draw_text(buffer, cursor, scale, 1.0f, 1.0f, 1.0f, image);
  }
}

Rect compute_playing_area(V2 image_size)
{
  Rect result;

  F32 area_aspect_radio = PLAYING_AREA_WIDTH/PLAYING_AREA_HEIGHT;
  F32 image_aspect_ratio = image_size.x/image_size.y;
  if(image_aspect_ratio > area_aspect_radio) {
    // NOTE(leo): Image is wider
    F32 actual_width = image_size.y*area_aspect_radio;
    result.pos = (V2){ image_size.x/2.0f - actual_width/2.0f, 0.0f };
    result.dim = (V2){ actual_width, image_size.y };
  }
  else {
    F32 actual_height = image_size.x/area_aspect_radio;
    result.pos = (V2){ 0.0f, image_size.y/2.0f - actual_height/2.0f };
    result.dim = (V2){ image_size.x, actual_height };
  }

  return result;
}

Rect compute_paddle_rect_in_image(GameState *game_state, Rect playing_area)
{
  F32 scale = playing_area.dim.x/PLAYING_AREA_WIDTH;
  V2 arena_offset = v2_add(playing_area.pos, v2_smul(scale, (V2) { 2.0f, 0.0f }));
  Rect result = {
    .pos = v2_add(arena_offset, v2_smul(scale, game_state->paddle.pos)),
    .dim = v2_smul(scale, game_state->paddle.dim)
  };
  return result;
}

Rect compute_paddle_motion_rect_in_image(GameState *game_state, Rect playing_area)
{
  F32 scale = playing_area.dim.x/PLAYING_AREA_WIDTH;
  V2 arena_offset = v2_add(playing_area.pos, v2_smul(scale, (V2) { 2.0f, 0.0f }));
  Rect result = {
    .pos = v2_add(arena_offset, v2_smul(scale, (V2) { game_state->paddle.dim.x/2.0f, game_state->paddle.pos.y + PADDLE_HEIGTH/2.0f })),
    .dim = v2_smul(scale, (V2) { ARENA_WIDTH-game_state->paddle.dim.x, 0.0f })
  };
  return result;
}

void draw_grid_top_down(bool *values, int width, int height, V2 bottom_left, F32 pixel_size, F32 r, F32 g, F32 b, Image *image)
{
  V2 cursor = v2_add(bottom_left, (V2) { 0.0f, (height-1)*pixel_size });
  for(int y = 0; y < height; y++) {
    for(int x = 0; x < width; x++) {
      if(values[y*width + x])
        draw_rectangle(cursor, v2_add(cursor, (V2) { pixel_size, pixel_size }), r, g, b, image);
      cursor.x += pixel_size;
    }
    cursor.x = bottom_left.x;
    cursor.y -= pixel_size;
  }
}

void draw_symbol(char symbol, V2 bottom_left, F32 pixel_size, F32 r, F32 g, F32 b, Image *image)
{
  bool *grid = NULL;
  switch(symbol) {
    case 'A': { grid = grid_A; } break;
    case 'B': { grid = grid_B; } break;
    case 'C': { grid = grid_C; } break;
    case 'D': { grid = grid_D; } break;
    case 'E': { grid = grid_E; } break;
    case 'F': { grid = grid_F; } break;
    case 'G': { grid = grid_G; } break;
    case 'H': { grid = grid_H; } break;
    case 'I': { grid = grid_I; } break;
    case 'J': { grid = grid_J; } break;
    case 'K': { grid = grid_K; } break;
    case 'L': { grid = grid_L; } break;
    case 'M': { grid = grid_M; } break;
    case 'N': { grid = grid_N; } break;
    case 'O': { grid = grid_O; } break;
    case 'P': { grid = grid_P; } break;
    case 'Q': { grid = grid_Q; } break;
    case 'R': { grid = grid_R; } break;
    case 'S': { grid = grid_S; } break;
    case 'T': { grid = grid_T; } break;
    case 'U': { grid = grid_U; } break;
    case 'V': { grid = grid_V; } break;
    case 'W': { grid = grid_W; } break;
    case 'X': { grid = grid_X; } break;
    case 'Y': { grid = grid_Y; } break;
    case 'Z': { grid = grid_Z; } break;
    case '0': { grid = grid_0; } break;
    case '1': { grid = grid_1; } break;
    case '2': { grid = grid_2; } break;
    case '3': { grid = grid_3; } break;
    case '4': { grid = grid_4; } break;
    case '5': { grid = grid_5; } break;
    case '6': { grid = grid_6; } break;
    case '7': { grid = grid_7; } break;
    case '8': { grid = grid_8; } break;
    case '9': { grid = grid_9; } break;
    case '<': { grid = grid_less; } break;
    case '>': { grid = grid_greater; } break;
    case ' ': { return; };
    default: {
      draw_rectangle(bottom_left, v2_add(bottom_left, (V2) { SYMBOL_WIDTH*pixel_size, SYMBOL_HEIGHT*pixel_size }), 1.0f, 0.0f, 1.0f, image);
      return;
    } break;
  };

  draw_grid_top_down(grid, SYMBOL_WIDTH, SYMBOL_HEIGHT, bottom_left, pixel_size, r, g, b, image);
}

void draw_text(char *text, V2 bottom_left, F32 pixel_size, F32 r, F32 g, F32 b, Image *image)
{
  int symbol_count = strlen(text);

  V2 cursor = bottom_left;

  for(int i = 0; i < symbol_count; i++) {
    draw_symbol(text[i], cursor, pixel_size, r, g, b, image);
    cursor.x += (SYMBOL_WIDTH+SYMBOL_SPACING)*pixel_size;
  }
}

void draw_text_centered(char *text, V2 center, F32 pixel_size, F32 r, F32 g, F32 b, Image *image)
{
  int symbol_count = strlen(text);

  Rect text_rect = {
    .pos = center,
    .dim = { SYMBOL_WIDTH*pixel_size*symbol_count + SYMBOL_SPACING*pixel_size*(symbol_count-1), SYMBOL_HEIGHT*pixel_size },
  };
  text_rect.pos = v2_sub(text_rect.pos, v2_smul(0.5f, text_rect.dim));

  draw_text(text, text_rect.pos, pixel_size, r, g, b, image);
}
