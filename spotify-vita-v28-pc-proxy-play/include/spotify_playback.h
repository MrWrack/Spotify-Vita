#ifndef SPOTIFY_PLAYBACK_H
#define SPOTIFY_PLAYBACK_H

int spotify_playback_play(void);
int spotify_playback_play_uri(const char *uri);
int spotify_playback_pause(void);
int spotify_playback_next(void);
int spotify_playback_previous(void);
int spotify_playback_seek(int position_ms);
int spotify_playback_volume(int percent);

#endif
