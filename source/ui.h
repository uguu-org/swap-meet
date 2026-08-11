#ifndef UI_H_
#define UI_H_

#include"pd_api.h"

// Load images and fonts.
void InitUI(PlaydateAPI *pd);

// Draw title screen.
void DrawTitleScreen(PlaydateAPI *pd);

// Draw stats for game over screen.
void DrawGameStats(PlaydateAPI *pd, int time_ms, int trades);

// Draw input statuses for input test mode.
void DrawInputStatus(PlaydateAPI *pd);

// Update menu image for when we are in between states.
void SetTransitionMenuImage(PlaydateAPI *pd);

// Update menu image for title screen.
void SetTitleMenuImage(PlaydateAPI *pd);

// Update menu image for game in progress state.
void SetGameRunningMenuImage(PlaydateAPI *pd);

// Update menu image for game over state.
void SetGameOverMenuImage(PlaydateAPI *pd);

#endif  // UI_H_
