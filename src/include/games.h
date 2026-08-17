#ifndef GAMES_H
#define GAMES_H

#include "types.h"

#define SNAKE_MAX_LEN 256
#define SNAKE_GRID_W  40
#define SNAKE_GRID_H  20

typedef struct {
    int x[SNAKE_MAX_LEN];
    int y[SNAKE_MAX_LEN];
    int length;
    int dir;
    int alive;
    int score;
} snake_game_t;

typedef struct {
    int board[20][10];
    int current_piece;
    int next_piece;
    int piece_x;
    int piece_y;
    int piece_rot;
    int score;
    int level;
    int lines_cleared;
    int game_over;
} tetris_game_t;

typedef struct {
    int board[31][28];
    int pac_x;
    int pac_y;
    int pac_dir;
    int ghost_x[4];
    int ghost_y[4];
    int ghost_dir[4];
    int score;
    int lives;
    int dots_remaining;
    int game_over;
} pacman_game_t;

void snake_init(snake_game_t *g);
int  snake_tick(snake_game_t *g, int input);
void snake_draw(snake_game_t *g, char *buf, int buf_w, int buf_h);

void tetris_init(tetris_game_t *g);
int  tetris_tick(tetris_game_t *g, int input);
void tetris_draw(tetris_game_t *g, char *buf, int buf_w, int buf_h);

void pacman_init(pacman_game_t *g);
int  pacman_tick(pacman_game_t *g, int input);
void pacman_draw(pacman_game_t *g, char *buf, int buf_w, int buf_h);

#endif
