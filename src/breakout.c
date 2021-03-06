#include "breakout.h"

#include "util.h"
#include "symbol_grids.h"

#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

internal
void draw_rectangle(Rect rect, Color color, RenderCmdBuffer *cmd_buffer)
{
  assert(cmd_buffer->count < cmd_buffer->capacity);

  cmd_buffer->commands[cmd_buffer->count++] = (RectangleCmd){
    .rect = rect,
    .color = color,
  };
}

internal
void draw_rectangle_offset(Rect rect, V2 offset, Color color, RenderCmdBuffer *cmd_buffer)
{
  draw_rectangle((Rect) { .pos = v2_add(rect.pos, offset), .dim = rect.dim }, color, cmd_buffer);
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

void reset_bricks(GameState *game_state)
{
  for(int brick_index = 0; brick_index < BRICK_COUNT; brick_index++)
    game_state->is_brick_broken[brick_index] = false;
  game_state->bricks_remaining = BRICK_COUNT;
}

void reset_ball(GameState *game_state)
{
  game_state->ball.pos = INITIAL_BALL_POS;
  choose_random_ball_direction(&game_state->ball_direction);
  game_state->ball_speed = 0.0f;
  game_state->target_ball_speed = BALL_SPEED_1;
}

void reset_paddle(GameState *game_state)
{
  game_state->paddle.pos.x = INITIAL_PADDLE_POS(PADDLE_WIDTH(game_state->difficulty_factor));
  game_state->paddle.dim.x = PADDLE_WIDTH(game_state->difficulty_factor);
}

void game_serve(GameState *game_state)
{
  game_state->state = GAME_STATE_PLAYING;

  reset_ball(game_state);

  game_state->paddle.pos.x = INITIAL_PADDLE_POS(PADDLE_WIDTH(game_state->difficulty_factor));
  game_state->paddle.dim.x = PADDLE_WIDTH(game_state->difficulty_factor);
  game_state->is_paddle_shrunk = false;

  game_state->hit_count = 0;
  game_state->balls_remaining--;
}

void change_paddle_width(GameState *game_state, F32 new_width)
{
  game_state->paddle.pos.x += game_state->paddle.dim.x/2.0f;
  game_state->paddle.pos.x -= new_width/2.0f;
  game_state->paddle.dim.x = new_width;
}

Rect compute_brick_rect(int brick_index)
{
  int x = brick_index%BRICK_COUNT_X;
  int y = brick_index/BRICK_COUNT_X;
  F32 xpos = BRICK_DELTA_X + (BRICK_WIDTH + BRICK_DELTA_X) * x;
  F32 ypos = FIRST_BRICK_HEIGHT + (BRICK_HEIGHT + BRICK_DELTA_Y) * y;
  Rect result = {
    .pos = (V2){xpos, ypos},
    .dim = (V2){BRICK_WIDTH, BRICK_HEIGHT}
  };
  return result;
}

U32 compute_brick_type(int brick_index)
{
  int y = brick_index/BRICK_COUNT_X;
  U32 result = y/2;
  return result;
}

void switch_to_reset_game(GameState *game_state, bool then_switch_to_main_menu, bool erase_score)
{
  for(int brick_index = 0; brick_index < BRICK_COUNT; brick_index++) {
    if(game_state->is_brick_broken[brick_index])
      game_state->brick_alpha[brick_index] = ((F32)rand() / RAND_MAX) * 0.5f;
    else
      game_state->brick_alpha[brick_index] = 1.0f;
  }
  reset_bricks(game_state);

  game_state->is_erasing_score = erase_score;
  game_state->is_switching_to_main_menu = then_switch_to_main_menu;
  game_state->state = GAME_STATE_RESET_GAME;
}

void game_update(GameState *game_state, F32 dt, Input *input, RenderCmdBuffer *cmd_buffer)
{
  // NOTE(leo): initialization
  if(game_state->state == GAME_STATE_UNINITIALIZED)
  {
    srand(time(0));

    game_state->state = GAME_STATE_MAIN_MENU;

    game_state->difficulty_factor = 1.0f;

    // NOTE(leo): Paddle
    game_state->paddle = (Rect){
      .pos = { INITIAL_PADDLE_POS(PADDLE_WIDTH(game_state->difficulty_factor)), PADDLE_Y },
      .dim = { PADDLE_WIDTH(game_state->difficulty_factor), PADDLE_HEIGTH }
    };
    game_state->is_paddle_shrunk = false;

    // NOTE(leo): Ball
    game_state->ball = (Rect){
      .pos = INITIAL_BALL_POS,
      .dim = { BALL_WIDTH, BALL_HEIGHT }
    };

    // NOTE(leo): Bricks
    reset_bricks(game_state);

    game_state->balls_remaining = 3;
  }

  // NOTE(leo): Animate paddle back
  if(game_state->state == GAME_STATE_RESET_PADDLE) {
    // NOTE(leo): Width
    F32 target_paddle_width = PADDLE_WIDTH(game_state->difficulty_factor);
    {
      F32 add_width = target_paddle_width - game_state->paddle.dim.x;
      F32 dw = 20.0f*add_width*dt;
      if(fabsf(dw) > fabsf(add_width))
        dw = add_width;
      change_paddle_width(game_state, game_state->paddle.dim.x + dw);
    }
    // NOTE(leo): Position
    F32 target_paddle_pos = INITIAL_PADDLE_POS(game_state->paddle.dim.x);
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
      reset_ball(game_state);
      reset_paddle(game_state);
      game_state->state = GAME_STATE_WAIT_SERVE;
    }
  }
  // NOTE(leo): Fade blocks, morph paddle back
  else if(game_state->state == GAME_STATE_RESET_GAME) {
    F32 alpha_speed = 6.0f;
    F32 offset = 0.1f;
    int finished_brick_count = 0;
    for(int brick_index = 0; brick_index < BRICK_COUNT; brick_index++) {
      F32 alpha = game_state->brick_alpha[brick_index];
      alpha += (1.0f - alpha + offset) * alpha_speed * dt;
      if(alpha >= 1.0f - 0.001f) {
        finished_brick_count++;
        alpha = 1.0f;
      }
      game_state->brick_alpha[brick_index] = alpha;
    }

    // NOTE(leo): Width
    bool is_paddle_width_finished = false;
    {
      F32 target_paddle_width = PADDLE_WIDTH(game_state->difficulty_factor);
      F32 add_width = target_paddle_width - game_state->paddle.dim.x;
      F32 dw = 20.0f*add_width*dt;
      if(fabsf(dw) > fabsf(add_width))
        dw = add_width;
      change_paddle_width(game_state, game_state->paddle.dim.x + dw);
      if(fabsf(target_paddle_width - game_state->paddle.dim.x) < 0.001f)
        is_paddle_width_finished = true;
    }
    // NOTE(leo): Position
    bool is_paddle_position_finished = false;
    {
      F32 target_paddle_pos = INITIAL_PADDLE_POS(game_state->paddle.dim.x);
      F32 paddle_speed_factor = 20.0f;
      F32 add_pos = target_paddle_pos - game_state->paddle.pos.x;
      F32 dx = paddle_speed_factor*add_pos*dt;
      if(fabsf(dx) > fabsf(add_pos))
        dx = add_pos;
      game_state->paddle.pos.x += dx;
      if(fabsf(target_paddle_pos - game_state->paddle.pos.x) < 0.001f)
        is_paddle_position_finished = true;
    }

    if(finished_brick_count == BRICK_COUNT && is_paddle_width_finished && is_paddle_position_finished) {
      if(game_state->is_switching_to_main_menu)
        game_state->state = GAME_STATE_MAIN_MENU;
      else
        game_state->state = GAME_STATE_WAIT_SERVE;
      reset_ball(game_state);
      reset_paddle(game_state);

      if(game_state->is_erasing_score) {
        game_state->score = 0;
        game_state->balls_remaining = 3;
        game_state->has_cleared_bricks = false;
      }
    }
  }


  // NOTE(leo): Physics
  if(game_state->state == GAME_STATE_PLAYING || game_state->state == GAME_STATE_GAME_OVER)
  {
    // NOTE(leo): Compute new paddle speed
    F32 paddle_speed = 0.0f;
    if(game_state->state == GAME_STATE_PLAYING)
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
        for(int brick_index = 0; brick_index < BRICK_COUNT; brick_index++) {
          if(game_state->is_brick_broken[brick_index])
            continue;

          Impact impact = compute_impact(game_state->ball, ball_delta, compute_brick_rect(brick_index), (V2) { 0.0f, 0.0f });
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
      }

      // NOTE(leo): Reflect off walls
      if(hit_walls) {
        reflect_ball(hit_wall_edges, &game_state->ball, &game_state->ball_direction);
      }

      // NOTE(leo): "Reflect" off paddle
      if(hit_paddle) {
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


      // NOTE(leo): Hit bricks gameplay logic
      if(hit_bricks && game_state->state == GAME_STATE_PLAYING) {
        game_state->hit_count += hit_brick_count;
        game_state->bricks_remaining -= hit_brick_count;

        // NOTE(leo): Attribute score for hitting brick; Max ball speed if orange or red brick
        for(int i = 0; i < hit_brick_count; i++) {
          game_state->is_brick_broken[hit_brick_indices[i]] = true;
          U32 brick_type = compute_brick_type(hit_brick_indices[i]);
          if(brick_type == 0) {
            game_state->score += roundf(1 * game_state->difficulty_factor);
          }
          else if(brick_type == 1) {
            game_state->score += roundf(3 * game_state->difficulty_factor);
          }
          else if(brick_type == 2) {
            game_state->score += roundf(5 * game_state->difficulty_factor);
            if(game_state->target_ball_speed < BALL_SPEED_4)
              game_state->target_ball_speed = BALL_SPEED_4;
          }
          else if(brick_type == 3) {
            game_state->score += roundf(7 * game_state->difficulty_factor);
            if(game_state->target_ball_speed < BALL_SPEED_4)
              game_state->target_ball_speed = BALL_SPEED_4;
          }
        }

        // NOTE(leo): Second set of bricks
        if(game_state->bricks_remaining == 0) {
          if(game_state->has_cleared_bricks) {
            game_state->state = GAME_STATE_GAME_OVER;
          }
          else {
            switch_to_reset_game(game_state, false, false);
            game_state->has_cleared_bricks = true;
            // TODO(leo): Is this confusing the player? Could think: Why do
            // they take a ball from me when I serve after I have cleared the
            // first set of bricks?
            game_state->balls_remaining++;
          }
          elapsed = dt;
          break;
        }
      }

      // NOTE(leo): Hit walls gameplay logic
      if(hit_walls && game_state->state == GAME_STATE_PLAYING) {
        if((hit_wall_edges & EDGE_LEFT) || (hit_wall_edges & EDGE_RIGHT))
          game_state->hit_count++;
        if((hit_wall_edges & EDGE_BOTTOM) || (hit_wall_edges & EDGE_TOP))
          game_state->hit_count++;

        // NOTE(leo): Paddle shrinking
        if(hit_wall_edges & EDGE_BOTTOM && !game_state->is_paddle_shrunk) {
          game_state->is_paddle_shrunk = true;
          change_paddle_width(game_state, game_state->paddle.dim.x/2.0f);
        }

        // NOTE(leo): Round over
        if(hit_wall_edges & EDGE_TOP) {
          if(game_state->balls_remaining)
            game_state->state = GAME_STATE_RESET_PADDLE;
          else
            game_state->state = GAME_STATE_GAME_OVER;

          elapsed = dt;
          break;
        }
      }

      // NOTE(leo): Hit paddle gameplay logic
      if(hit_paddle && game_state->state == GAME_STATE_PLAYING) {
        game_state->hit_count++;
      }


      // TODO(leo): Prevent paddle from pushing ball into wall

      // NOTE(leo): Ball speed gameplay logic
      if(game_state->state == GAME_STATE_PLAYING) {
        if(game_state->hit_count == 4 && game_state->target_ball_speed < BALL_SPEED_2)
          game_state->target_ball_speed = BALL_SPEED_2;
        else if(game_state->hit_count == 12 && game_state->target_ball_speed < BALL_SPEED_3)
          game_state->target_ball_speed = BALL_SPEED_3;
      }

      elapsed += step;
      if(iterations > 25) {
        // TODO(leo): Proper time step (why not dt to previous frame?)
        assert(false);
        break;
      }
    }
  }

  // NOTE(leo): Draw playing area boundaries (only visible if window width is too small)
#if 0
  draw_rectangle(v2_add(playing_area.pos, (V2) { 0.0f, -scale*2.0f }), v2_add(playing_area.pos, (V2) { playing_area.dim.x, 0.0f }), COLOR_WHITE, image);
  draw_rectangle(v2_add(playing_area.pos, (V2) { 0.0f, playing_area.dim.y }), v2_add(playing_area.pos, (V2) { playing_area.dim.x, playing_area.dim.y + scale*2.0f }), COLOR_WHITE, image);
#endif

  // NOTE(leo): Draw arena
  draw_rectangle((Rect) { (V2) { 0.0f, 0.0f }, (V2) { 2.0f, PLAYING_AREA_HEIGHT } }, COLOR_WHITE, cmd_buffer);
  draw_rectangle((Rect) { (V2) { 2.0f+ARENA_WIDTH, 0.0f }, (V2) { 2.0f, PLAYING_AREA_HEIGHT } }, COLOR_WHITE, cmd_buffer);
  draw_rectangle((Rect) { (V2) { 2.0f, ARENA_HEIGHT }, (V2) { ARENA_WIDTH, 2.0f } }, COLOR_WHITE, cmd_buffer);

  V2 arena_offset = { 2.0f, 0.0f };

  // NOTE(leo): Draw bricks
  Color brick_colors[4] = { (Color){ 0.77f, 0.78f, 0.09f, 1.0f }, (Color){ 0.0f, 0.5f, 0.13f, 1.0f }, (Color){ 0.76f, 0.51f, 0.0f, 1.0f }, (Color){ 0.63f, 0.04f, 0.0f, 1.0f } };
  for(int brick_index = 0; brick_index < BRICK_COUNT; brick_index++) {
    if(game_state->is_brick_broken[brick_index])
      continue;
    Color color = brick_colors[compute_brick_type(brick_index)];
    if(game_state->state == GAME_STATE_RESET_GAME)
      color.a = game_state->brick_alpha[brick_index];
    Rect brick_rect = compute_brick_rect(brick_index);
    draw_rectangle_offset(brick_rect, arena_offset, color, cmd_buffer);
  }

  // NOTE(leo): Draw paddle
  draw_rectangle_offset(game_state->paddle, arena_offset, PADDLE_COLOR, cmd_buffer);

  // NOTE(leo): Draw ball
  if((game_state->state == GAME_STATE_WAIT_SERVE) || (game_state->state == GAME_STATE_PLAYING)
    || (game_state->state == GAME_STATE_GAME_OVER) || (game_state->state == GAME_STATE_PAUSE))
  {
    draw_rectangle_offset(game_state->ball, arena_offset, BALL_COLOR, cmd_buffer);
  }

  // NOTE(leo): Draw score
  {
    char buffer[4];
    buffer[0] = (game_state->score / 100) % 10 + '0';
    buffer[1] = (game_state->score / 10) % 10 + '0';
    buffer[2] = (game_state->score / 1) % 10 + '0';
    buffer[3] = 0;
    V2 cursor = (V2){ 2.0f + ARENA_WIDTH/2.0f + ARENA_WIDTH/4.0f, ARENA_HEIGHT + (PLAYING_AREA_HEIGHT-ARENA_HEIGHT)/2.0f };
    draw_text_centered(buffer, cursor, 1, COLOR_WHITE, cmd_buffer);
  }

  // NOTE(leo): Draw ball count
  {
    char buffer[4];
    buffer[0] = (game_state->balls_remaining / 100) % 10 + '0';
    buffer[1] = (game_state->balls_remaining / 10) % 10 + '0';
    buffer[2] = (game_state->balls_remaining / 1) % 10 + '0';
    buffer[3] = 0;
    V2 cursor = (V2){ 2.0f + ARENA_WIDTH/2.0f - ARENA_WIDTH/4.0f, ARENA_HEIGHT + (PLAYING_AREA_HEIGHT-ARENA_HEIGHT)/2.0f };
    draw_text_centered(buffer, cursor, 1, COLOR_WHITE, cmd_buffer);
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

  // TODO(leo): This "breaks" the paddle control (jumps when start playing). Do flooring for playing area bound rects?
  /*
  result.pos.x = roundf(result.pos.x);
  result.pos.y = roundf(result.pos.y);
  result.dim.x = roundf(result.dim.x);
  result.dim.y = roundf(result.dim.y);
  */

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

void draw_grid_top_down(bool *values, int width, int height, V2 bottom_left, F32 pixel_size, Color color, RenderCmdBuffer *cmd_buffer)
{
  V2 cursor = v2_add(bottom_left, (V2) { 0.0f, (height-1)*pixel_size });
  for(int y = 0; y < height; y++) {
    for(int x = 0; x < width; x++) {
      if(values[y*width + x])
        draw_rectangle((Rect) { cursor, (V2) { pixel_size, pixel_size } }, color, cmd_buffer);
      cursor.x += pixel_size;
    }
    cursor.x = bottom_left.x;
    cursor.y -= pixel_size;
  }
}

void draw_symbol(char symbol, V2 bottom_left, F32 pixel_size, Color color, RenderCmdBuffer *cmd_buffer)
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
      draw_rectangle((Rect) { bottom_left, (V2) { SYMBOL_WIDTH *pixel_size, SYMBOL_HEIGHT *pixel_size } }, (Color) { 1.0f, 0.0f, 1.0f, 1.0f }, cmd_buffer);
      return;
    } break;
  };

  draw_grid_top_down(grid, SYMBOL_WIDTH, SYMBOL_HEIGHT, bottom_left, pixel_size, color, cmd_buffer);
}

void draw_text(char *text, V2 bottom_left, F32 pixel_size, Color color, RenderCmdBuffer *cmd_buffer)
{
  int symbol_count = strlen(text);

  V2 cursor = bottom_left;

  for(int i = 0; i < symbol_count; i++) {
    draw_symbol(text[i], cursor, pixel_size, color, cmd_buffer);
    cursor.x += (SYMBOL_WIDTH+SYMBOL_SPACING)*pixel_size;
  }
}

void draw_text_centered(char *text, V2 center, F32 pixel_size, Color color, RenderCmdBuffer *cmd_buffer)
{
  int symbol_count = strlen(text);

  Rect text_rect = {
    .pos = center,
    .dim = { SYMBOL_WIDTH*pixel_size*symbol_count + SYMBOL_SPACING*pixel_size*(symbol_count-1), SYMBOL_HEIGHT*pixel_size },
  };
  text_rect.pos = v2_sub(text_rect.pos, v2_smul(0.5f, text_rect.dim));

  draw_text(text, text_rect.pos, pixel_size, color, cmd_buffer);
}
