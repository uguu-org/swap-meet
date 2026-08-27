#ifndef WORLD_H_
#define WORLD_H_

#include"pd_api.h"

// Load images.
void InitImages(PlaydateAPI *pd);

// Initialize game states.
void ResetWorld(int game_size);

// Update item positions.
void UpdateWorld(void);

// Draw game graphics.
void DrawWorld(PlaydateAPI *pd, int game_size, int ticks);

// Draw arrow pointing at next recommended trader.
void DrawHint(PlaydateAPI *pd);

// Draw and animate fireworks.
void DrawAndUpdateFireworks(PlaydateAPI *pd, int frames, int beats);

// Apply movement.  dx and dy should both be in the range of [-1,1].
// This function will take care of setting the actual speed.
void MakeMove(int dx, int dy);

// Attempt to make a trade at current location.  Returns 1 if trade happened.
#ifndef NDEBUG
int MakeTrade(PlaydateAPI *pd, int game_size, int direction);
#else
int MakeTrade(int game_size, int direction);
#endif

// Check total number of trades performed.
int GetTotalTrades(void);

// Check if player holds the goal item.  Returns 1 if so.
int ReachedGoal(void);

// Move automatically, used for demo mode.  Returns 1 if demo mode is done.
#ifndef NDEBUG
int AutoMove(PlaydateAPI *pd, int game_size, int game_time_ms, int use_hints);
#else
int AutoMove(int game_size, int game_time_ms, int use_hints);
#endif

#endif  // WORLD_H_
