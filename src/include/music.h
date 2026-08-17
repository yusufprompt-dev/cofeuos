#ifndef MUSIC_H
#define MUSIC_H

#include "types.h"

#define MUSIC_TITLE_MAX   64
#define MUSIC_ARTIST_MAX  64
#define MUSIC_PATH_MAX    128
#define MUSIC_PLAYLIST_MAX 32

typedef struct {
    char title[MUSIC_TITLE_MAX];
    char artist[MUSIC_ARTIST_MAX];
    char path[MUSIC_PATH_MAX];
    u32  duration_sec;
    u32  sample_rate;
    u16  channels;
    u16  bit_rate;
} music_track_t;

typedef struct {
    music_track_t tracks[MUSIC_PLAYLIST_MAX];
    int count;
    int current;
    int playing;
    int paused;
    u32 position_sec;
    u8  volume;
    u8  shuffle;
    u8  repeat;
} music_player_t;

void music_init(music_player_t *player);
int  music_play(music_player_t *player, int track_index);
int  music_pause(music_player_t *player);
int  music_stop(music_player_t *player);
int  music_next(music_player_t *player);
int  music_prev(music_player_t *player);
int  music_set_volume(music_player_t *player, u8 volume);
int  music_set_shuffle(music_player_t *player, int enabled);
int  music_set_repeat(music_player_t *player, int enabled);
int  music_tick(music_player_t *player);
int  music_add_track(music_player_t *player, const char *path, const char *title, const char *artist);
int  music_remove_track(music_player_t *player, int index);
int  music_get_status(music_player_t *player, int *track, int *position, int *total);

#endif
