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
void DrawWorld(PlaydateAPI *pd, int game_size, int frames);

// Draw arrow pointing at next recommended trader.
void DrawHint(PlaydateAPI *pd);

// Apply movement.
void MakeMove(int dx, int dy);

// Attempt to make a trade at current location.  Returns 1 if trade happened.
#ifndef NDEBUG
int MakeTrade(PlaydateAPI *pd, int game_size, int direction);
#else
int MakeTrade(int game_size, int direction);
#endif

// Check if player holds the goal item.  Returns 1 if so.
int ReachedGoal(void);

#endif  // WORLD_H_
