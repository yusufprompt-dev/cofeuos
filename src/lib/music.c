#include "../include/music.h"
#include "../include/string.h"

static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0'; }

void music_init(music_player_t *player) {
    if (!player) return;
    player->count = 0;
    player->current = 0;
    player->playing = 0;
    player->paused = 0;
    player->position_sec = 0;
    player->volume = 80;
    player->shuffle = 0;
    player->repeat = 0;
}

int music_add_track(music_player_t *player, const char *path, const char *title, const char *artist) {
    if (!player || !path || player->count >= MUSIC_PLAYLIST_MAX) return -1;
    music_track_t *t = &player->tracks[player->count];
    str_copy(t->path, path, MUSIC_PATH_MAX);
    if (title) str_copy(t->title, title, MUSIC_TITLE_MAX);
    else t->title[0] = '\0';
    if (artist) str_copy(t->artist, artist, MUSIC_ARTIST_MAX);
    else t->artist[0] = '\0';
    t->duration_sec = 180 + player->count * 30;
    t->sample_rate = 44100;
    t->channels = 2;
    t->bit_rate = 128;
    player->count++;
    return player->count - 1;
}

int music_play(music_player_t *player, int track_index) {
    if (!player || track_index < 0 || track_index >= player->count) return -1;
    player->current = track_index;
    player->playing = 1;
    player->paused = 0;
    player->position_sec = 0;
    return 0;
}

int music_pause(music_player_t *player) {
    if (!player) return -1;
    if (player->playing) { player->paused = !player->paused; return 0; }
    return -2;
}

int music_stop(music_player_t *player) {
    if (!player) return -1;
    player->playing = 0;
    player->paused = 0;
    player->position_sec = 0;
    return 0;
}

int music_next(music_player_t *player) {
    if (!player || player->count == 0) return -1;
    player->current++;
    if (player->current >= player->count) player->current = 0;
    player->position_sec = 0;
    return player->current;
}

int music_prev(music_player_t *player) {
    if (!player || player->count == 0) return -1;
    player->current--;
    if (player->current < 0) player->current = player->count - 1;
    player->position_sec = 0;
    return player->current;
}

int music_set_volume(music_player_t *player, u8 volume) {
    if (!player) return -1;
    player->volume = volume;
    return 0;
}

int music_set_shuffle(music_player_t *player, int enabled) {
    if (!player) return -1;
    player->shuffle = enabled ? 1 : 0;
    return 0;
}

int music_set_repeat(music_player_t *player, int enabled) {
    if (!player) return -1;
    player->repeat = enabled ? 1 : 0;
    return 0;
}

int music_tick(music_player_t *player) {
    if (!player || !player->playing || player->paused) return 0;
    player->position_sec++;
    if (player->current < player->count) {
        u32 dur = player->tracks[player->current].duration_sec;
        if (player->position_sec >= dur) {
            if (player->repeat) { player->position_sec = 0; }
            else { music_next(player); }
        }
    }
    return 1;
}

int music_remove_track(music_player_t *player, int index) {
    if (!player || index < 0 || index >= player->count) return -1;
    player->tracks[index] = player->tracks[player->count - 1];
    player->count--;
    if (player->current >= player->count) player->current = 0;
    return 0;
}

int music_get_status(music_player_t *player, int *track, int *position, int *total) {
    if (!player) return -1;
    if (track) *track = player->current;
    if (position) *position = (int)player->position_sec;
    if (total && player->current < player->count)
        *total = (int)player->tracks[player->current].duration_sec;
    else if (total) *total = 0;
    return player->playing ? (player->paused ? 2 : 1) : 0;
}
