#include "../include/games.h"

void snake_init(snake_game_t *g) {
    if (!g) return;
    g->length = 3;
    g->dir = 1;
    g->alive = 1;
    g->score = 0;
    for (int i = 0; i < g->length; i++) { g->x[i] = 10 - i; g->y[i] = 10; }
}

int snake_tick(snake_game_t *g, int input) {
    if (!g || !g->alive) return 0;
    if (input == 1 && g->dir != 2) g->dir = 0;
    if (input == 2 && g->dir != 0) g->dir = 1;
    if (input == 3 && g->dir != 1) g->dir = 2;
    if (input == 4 && g->dir != 3) g->dir = 3;
    int dx = 0, dy = 0;
    if (g->dir == 0) dy = -1;
    if (g->dir == 1) dy = 1;
    if (g->dir == 2) dx = -1;
    if (g->dir == 3) dx = 1;
    int nx = g->x[0] + dx;
    int ny = g->y[0] + dy;
    if (nx < 0 || nx >= SNAKE_GRID_W || ny < 0 || ny >= SNAKE_GRID_H) { g->alive = 0; return 0; }
    for (int i = 0; i < g->length; i++) {
        if (g->x[i] == nx && g->y[i] == ny) { g->alive = 0; return 0; }
    }
    for (int i = g->length - 1; i > 0; i--) { g->x[i] = g->x[i-1]; g->y[i] = g->y[i-1]; }
    g->x[0] = nx; g->y[0] = ny;
    if (nx == SNAKE_GRID_W / 2 && ny == SNAKE_GRID_H / 2) {
        if (g->length < SNAKE_MAX_LEN) {
            g->x[g->length] = g->x[g->length-1];
            g->y[g->length] = g->y[g->length-1];
            g->length++;
        }
        g->score += 10;
    }
    return 1;
}

void snake_draw(snake_game_t *g, char *buf, int buf_w, int buf_h) {
    if (!g || !buf) return;
    for (int y = 0; y < buf_h && y < SNAKE_GRID_H; y++) {
        for (int x = 0; x < buf_w && x < SNAKE_GRID_W; x++) {
            int idx = y * buf_w + x;
            if (x == 0 || x == SNAKE_GRID_W-1 || y == 0 || y == SNAKE_GRID_H-1) { buf[idx] = '#'; continue; }
            buf[idx] = ' ';
            for (int s = 0; s < g->length; s++) {
                if (g->x[s] == x && g->y[s] == y) { buf[idx] = (s == 0) ? '@' : 'o'; break; }
            }
            if (x == SNAKE_GRID_W/2 && y == SNAKE_GRID_H/2) buf[idx] = '*';
        }
    }
}

void tetris_init(tetris_game_t *g) {
    if (!g) return;
    for (int y = 0; y < 20; y++) for (int x = 0; x < 10; x++) g->board[y][x] = 0;
    g->current_piece = 0; g->next_piece = 1;
    g->piece_x = 3; g->piece_y = 0;
    g->piece_rot = 0; g->score = 0; g->level = 1;
    g->lines_cleared = 0; g->game_over = 0;
}

int tetris_tick(tetris_game_t *g, int input) {
    if (!g || g->game_over) return 0;
    (void)input;
    g->piece_y++;
    if (g->piece_y >= 19) { g->piece_y = 0; g->piece_x = 3; g->score++; }
    return 1;
}

void tetris_draw(tetris_game_t *g, char *buf, int buf_w, int buf_h) {
    if (!g || !buf) return;
    for (int y = 0; y < buf_h && y < 20; y++) {
        for (int x = 0; x < buf_w && x < 10; x++) {
            int idx = y * buf_w + x;
            buf[idx] = g->board[y][x] ? '#' : ' ';
        }
    }
}

void pacman_init(pacman_game_t *g) {
    if (!g) return;
    for (int y = 0; y < 31; y++) for (int x = 0; x < 28; x++) g->board[y][x] = 1;
    g->pac_x = 14; g->pac_y = 23; g->pac_dir = 0;
    for (int i = 0; i < 4; i++) { g->ghost_x[i] = 14; g->ghost_y[i] = 14 - i * 2; g->ghost_dir[i] = 0; }
    g->score = 0; g->lives = 3; g->dots_remaining = 240; g->game_over = 0;
}

int pacman_tick(pacman_game_t *g, int input) {
    if (!g || g->game_over) return 0;
    (void)input;
    if (g->pac_dir == 0) g->pac_y--;
    else if (g->pac_dir == 1) g->pac_x++;
    else if (g->pac_dir == 2) g->pac_y++;
    else if (g->pac_dir == 3) g->pac_x--;
    if (g->pac_x < 0) g->pac_x = 27;
    if (g->pac_x > 27) g->pac_x = 0;
    if (g->pac_y < 0) g->pac_y = 30;
    if (g->pac_y > 30) g->pac_y = 0;
    return 1;
}

void pacman_draw(pacman_game_t *g, char *buf, int buf_w, int buf_h) {
    if (!g || !buf) return;
    for (int y = 0; y < buf_h && y < 31; y++) {
        for (int x = 0; x < buf_w && x < 28; x++) {
            int idx = y * buf_w + x;
            if (g->pac_x == x && g->pac_y == y) { buf[idx] = 'C'; continue; }
            buf[idx] = g->board[y][x] ? '.' : ' ';
        }
    }
}
