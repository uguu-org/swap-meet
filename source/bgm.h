#ifndef BGM_H_
#define BGM_H_

#include<stdint.h>

#include"pd_api.h"

// Load or change instruments.
void InitBgm(PlaydateAPI *pd, int sample_index);

// Rewind song state to the beginning.
void RewindBgm(void);

// Play all notes up to song_time_ms, inclusive.
//
// For music to keep playing, the update function must call this
// function once per frame.  Thus there is no "stop" function, since
// music stops playing when this function stops being called.
void PlayBgm(PlaydateAPI *pd, int song_index, int32_t song_time_ms);

// Check if current song has completed, returns 1 if so.
int BgmCompleted(int song_index);

#endif  // BGM_H_
