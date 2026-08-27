#include<assert.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>

#include"pd_api.h"

#include"bgm.h"
#include"common.h"
#include"data.h"
#include"ui.h"
#include"trade.h"
#include"world.h"

// Game difficulty settings.  See MAX_TRADER_COUNT in trade.h.
#define GAME_HARD_SIZE        MAX_TRADER_COUNT
#define GAME_EASY_SIZE        (GAME_HARD_SIZE / 2)

// Milliseconds per tick.  Same as ms_per_tick in gnossienne.m4.
#define MS_PER_TICK           70

// Syntactic sugar.
#define ANY_DIRECTION         (kButtonUp|kButtonDown|kButtonLeft|kButtonRight)
#define ANY_BUTTON            (ANY_DIRECTION | kButtonA | kButtonB)

// Wait this many milliseconds before starting demo mode.
#define DEMO_START_DELAY      10000

// Wait this many milliseconds before returning from completed demo to
// title screen.
#define DEMO_END_DELAY        5000

// Millisecond timestamp at previous call to Update().
static uint32_t g_last_update_time_ms;

// Millisecond timestamp of when D-Pad was last pressed on title screen.
static uint32_t g_title_dpad_press_time_ms;

// Initialize progress.
typedef enum
{
   kInitStart,
   kInitImages,
   kInitBgm,
   kInitDone,
} InitState;
static int g_init_state = kInitStart;

// Game state.
typedef enum
{
   kGameTitleScreen,
   kGameTransitionGameToTitle,
   kGameTransitionTitleToGame,
   kGameTransitionTitleToDemo,
   kGameInProgress,
   kGameOver,
   kGameDemoInProgress,
   kGameDemoDone,
   kGameInputTestRunning
} GameState;
static GameState g_game_state = kGameTitleScreen;

// Game time.
static int g_game_time_ms;
static int g_game_frames;

// Background music state.
static int g_bgm_started;
static int g_song_time_ms;

// Number of traders, selected at title screen.
static int g_game_size;

// Game completion stats.
static int g_game_complete_duration_ms;

// Hint display options.
static const char *kHintOptions[3] = { "hide", "stable", "flash" };
static PDMenuItem *g_hint_option = NULL;

// If nonzero, draw triangles showing where to go next.
static int g_show_hints;

// If nonzero, show frame rate in bottom left corner.
static int g_debug_show_fps;

// Load game data.
static int InitGame(PlaydateAPI *pd)
{
   #ifndef NDEBUG
      const int init_start = pd->system->getCurrentTimeMilliseconds();
   #endif
   switch( g_init_state )
   {
      case kInitStart:
         pd->system->setCrankSoundsDisabled(1);
         break;

      case kInitImages:
         InitImages(pd);
         InitUI(pd);
         break;

      case kInitBgm:
         InitBgm(pd, 0);
         break;

      default:
         break;
   }

   #ifndef NDEBUG
      pd->system->logToConsole(
         "Init step %d: %d ms",
         g_init_state,
         pd->system->getCurrentTimeMilliseconds() - init_start);
   #endif
   g_init_state++;
   return 0;
}

// Reset game to title screen.
static void MenuActionReset(void *userdata)
{
   PlaydateAPI *pd = userdata;

   g_game_state = kGameTransitionGameToTitle;

   // Reset clocks.  Note that g_song_time_ms is left untouched.
   g_game_time_ms = 0;
   g_game_frames = 0;
   g_title_dpad_press_time_ms = g_last_update_time_ms;

   // Reset frame rate display.
   g_debug_show_fps = 0;

   // Disable accelerometer when transitioning from input test to title screen.
   pd->system->setPeripheralsEnabled(kNone);
}

// Update hint setting.
static void MenuActionHint(void *userdata)
{
   PlaydateAPI *pd = userdata;
   g_show_hints = pd->system->getMenuItemValue(g_hint_option);
}

// Wait for button press to start game.
static void ShowTitleScreen(PlaydateAPI *pd)
{
   // Start game on button press.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & (kButtonA | kButtonB)) != 0 )
   {
      g_game_time_ms = 0;
      g_game_frames = 0;
      g_game_state = kGameTransitionTitleToGame;

      if( (pushed & kButtonA) != 0 )
         g_game_size = GAME_EASY_SIZE;
      else
         g_game_size = GAME_HARD_SIZE;

      // Start background music the first time we start a game.
      if( !g_bgm_started )
      {
         RewindBgm();
         g_song_time_ms = 0;
         g_bgm_started = 1;
      }
   }

   // Start demo mode if we have waited long enough.
   //
   // Note that background music is not started in this case, but
   // if it has started already, it will keep playing.
   if( g_game_time_ms >= DEMO_START_DELAY )
   {
      g_game_time_ms = 0;
      g_game_frames = 0;
      g_game_state = kGameTransitionTitleToDemo;

      // Demo alternates between easy and hard modes, depending on what was
      // played last time.  If no game or demo has been played yet, first
      // demo run will be in easy mode.
      g_game_size = (g_game_size == GAME_EASY_SIZE ? GAME_HARD_SIZE
                                                   : GAME_EASY_SIZE);
   }

   // Special debug features, toggled by holding any direction on
   // D-pad at title screen for a few seconds.
   if( (pushed & ANY_DIRECTION) != 0 )
      g_title_dpad_press_time_ms = g_last_update_time_ms;
   if( (current & ANY_DIRECTION) != 0 )
   {
      // Enable frame rate display after hold D-pad for 1 second.
      if( g_last_update_time_ms - g_title_dpad_press_time_ms > 1000 )
      {
         g_debug_show_fps = 1;

         // Enter input test after holding D-pad for 3 seconds.
         if( g_last_update_time_ms - g_title_dpad_press_time_ms > 3000 )
         {
            g_game_state = kGameInputTestRunning;

            // Enable accelerometer for input test, even though we
            // don't use it in this game.
            pd->system->setPeripheralsEnabled(kAccelerometer);
         }
      }
   }

   // Draw title screen.
   DrawTitleScreen(pd);
}

//////////////////////////////////////////////////////////////////////

// Transition from title screen to start of game or start of demo.
static void TransitionTitleToGame(PlaydateAPI *pd)
{
   if( g_game_frames < 15 )
   {
      // Screen is not cleared.  Instead, we just draw progressively
      // lighter rectangles over what's already there.
      const int opacity = g_game_frames * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }
   else
   {
      // Initialize game states on first frame of fade in.
      if( g_game_frames == 15 )
      {
         InitTraders(g_game_size);
         ResetWorld(g_game_size);
         g_game_complete_duration_ms = 0;
      }

      // Draw game graphics.
      UpdateWorld();
      DrawWorld(pd, g_game_size, g_song_time_ms / MS_PER_TICK);

      // Draw progressively more transparent white rectangles over
      // game graphics for fade-in effect.
      const int opacity = (30 - g_game_frames) * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }

   // If we are currently in demo mode, any button press will return us
   // to title screen.
   if( g_game_state == kGameTransitionTitleToDemo )
   {
      PDButtons current, pushed, released;
      pd->system->getButtonState(&current, &pushed, &released);
      if( (pushed & ANY_BUTTON) != 0 )
      {
         MenuActionReset(pd);
         return;
      }
   }

   if( g_game_frames >= 30 )
   {
      g_game_time_ms = 0;
      g_game_frames = 0;
      g_game_state = g_game_state == kGameTransitionTitleToDemo
         ? kGameDemoInProgress
         : kGameInProgress;
   }
}

// Transition from kGameOver state to kGameTitleScreen state.
static void TransitionGameToTitle(PlaydateAPI *pd)
{
   if( g_game_frames < 15 )
   {
      // Screen is not cleared.  Instead, we just draw progressively
      // lighter rectangles over what's already there.
      const int opacity = g_game_frames * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }
   else
   {
      // Draw title screen.
      DrawTitleScreen(pd);

      // Draw progressively more transparent white rectangles over
      // title screen for fade-in effect.
      const int opacity = (30 - g_game_frames) * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }

   if( g_game_frames >= 30 )
      g_game_state = kGameTitleScreen;
}

//////////////////////////////////////////////////////////////////////

// Handle button input for RunGame and GameOver states.
//
// Returns nonzero value if A or B button was pressed.
static int HandleInput(PlaydateAPI *pd)
{
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   const int dx =
      (current & kButtonLeft) != 0 ? -1
                                   : (current & kButtonRight) != 0 ? 1 : 0;
   const int dy =
      (current & kButtonUp) != 0 ? -1
                                 : (current & kButtonDown) != 0 ? 1 : 0;
   MakeMove(dx, dy);
   if( (pushed & kButtonA) != 0 )
      return 1;
   if( (pushed & kButtonB) != 0 )
      return -1;
   return 0;
}

// Run in-progress game.
static void RunGame(PlaydateAPI *pd)
{
   // Handle input.
   const int button_pressed = HandleInput(pd);
   if( button_pressed )
   {
      #ifndef NDEBUG
      if( MakeTrade(pd, g_game_size, button_pressed) )
      #else
      if( MakeTrade(g_game_size, button_pressed) )
      #endif
      {
         if( ReachedGoal() )
         {
            g_game_complete_duration_ms = g_game_time_ms;
            g_game_state = kGameOver;
         }
      }
   }

   // Draw game state.
   UpdateWorld();
   DrawWorld(pd, g_game_size, g_song_time_ms / MS_PER_TICK);
   if( g_show_hints )
   {
      if( g_show_hints == 1 || ((g_game_frames & 15) < 13) )
         DrawHint(pd);
   }
}

// Run non-interactive demo.
static void RunDemo(PlaydateAPI *pd)
{
   // Handle input.
   PDButtons current, pushed, released;
   pd->system->getButtonState(&current, &pushed, &released);
   if( (pushed & ANY_BUTTON) != 0 )
   {
      MenuActionReset(pd);
      return;
   }

   // Generate movement.
   #ifndef NDEBUG
      const int done = AutoMove(pd, g_game_size, g_game_time_ms, g_show_hints);
   #else
      const int done = AutoMove(g_game_size, g_game_time_ms, g_show_hints);
   #endif
   if( done )
   {
      if( ReachedGoal() )
      {
         g_game_complete_duration_ms = g_game_time_ms;
         g_game_state = kGameDemoDone;
      }
      else
      {
         // We couldn't move anymore, but we did not reach the goal.
         // This means the hint system got stuck.  The hint system
         // should be sufficiently robust so this should never happen,
         // but you never know.  Rather than idling in the demo loop
         // and wait for player input, we will go back to the title
         // screen right away.
         MenuActionReset(pd);
      }
   }

   // Draw game state.
   UpdateWorld();
   DrawWorld(pd, g_game_size, g_song_time_ms / MS_PER_TICK);
   if( g_show_hints )
   {
      if( g_show_hints == 1 || ((g_game_frames & 15) < 13) )
         DrawHint(pd);
   }
}

//////////////////////////////////////////////////////////////////////

// Run game over screen.
static void GameOver(PlaydateAPI *pd)
{
   // Draw game state and fireworks, then draw end game stats on top.
   UpdateWorld();
   DrawWorld(pd, g_game_size, g_song_time_ms / MS_PER_TICK);
   DrawAndUpdateFireworks(pd, g_game_frames, g_song_time_ms / MS_PER_TICK / 8);
   DrawGameStats(pd, g_game_complete_duration_ms, GetTotalTrades());

   // Handle input after drawing fireworks.  This is because an input
   // might cause the game timers to reset, which would cause the
   // fireworks to be blanked out.  We want the fireworks to still be
   // visible while we draw the fade out layer on top, so we handle
   // input after drawing fireworks.
   if( g_game_state == kGameOver )
   {
      // Press button to return to title screen.
      if( HandleInput(pd) )
         MenuActionReset(pd);
   }
   else
   {
      // Press button to return to title screen.
      // Also return to title screen if we have waited a few seconds.
      PDButtons current, pushed, released;
      pd->system->getButtonState(&current, &pushed, &released);
      if( (pushed & ANY_BUTTON) != 0 ||
          g_game_time_ms - g_game_complete_duration_ms >= DEMO_END_DELAY )
      {
         MenuActionReset(pd);
      }
   }
}

//////////////////////////////////////////////////////////////////////

// Run sensor test.
static void RunInputTest(PlaydateAPI *pd)
{
   pd->graphics->clear(kColorWhite);
   DrawInputStatus(pd);
}

//////////////////////////////////////////////////////////////////////

// Draw a single frame.
static int Update(void *userdata)
{
   PlaydateAPI *pd = userdata;

   // Complete initialization over the first few frames.
   if( UNLIKELY(g_init_state < kInitDone) )
      return InitGame(pd);

   // Update clocks.
   const uint32_t current_time_ms = pd->system->getCurrentTimeMilliseconds();
   const uint32_t delta_time_ms = current_time_ms - g_last_update_time_ms;
   if( UNLIKELY(delta_time_ms < 0 || delta_time_ms > 1000) )
   {
      g_game_time_ms++;
      g_song_time_ms++;
   }
   else
   {
      g_game_time_ms += delta_time_ms;
      g_song_time_ms += delta_time_ms;
   }
   g_last_update_time_ms = current_time_ms;
   g_game_frames++;

   // Background music starts when the first game starts, and plays in an
   // endless loop.
   if( g_bgm_started )
   {
      PlayBgm(pd, 0, g_song_time_ms);
      if( BgmCompleted(0) )
      {
         RewindBgm();
         g_song_time_ms = 0;
      }
   }

   // Run game modes.
   switch( g_game_state )
   {
      case kGameTitleScreen:              ShowTitleScreen(pd);          break;
      case kGameTransitionTitleToGame:    TransitionTitleToGame(pd);    break;
      case kGameTransitionTitleToDemo:    TransitionTitleToGame(pd);    break;
      case kGameInProgress:               RunGame(pd);                  break;
      case kGameDemoInProgress:           RunDemo(pd);                  break;
      case kGameOver:                     GameOver(pd);                 break;
      case kGameDemoDone:                 GameOver(pd);                 break;
      case kGameTransitionGameToTitle:    TransitionGameToTitle(pd);    break;
      case kGameInputTestRunning:         RunInputTest(pd);             break;
   }

   #ifdef NDEBUG
      // For release builds, enable frame rate display after
      // user has held on to D-Pad on title screen for 1 second.
      if( g_debug_show_fps )
         pd->system->drawFPS(0, LCD_ROWS - 12);
   #else
      // Frame rate display is always enabled in debug builds.
      pd->system->drawFPS(0, LCD_ROWS - 12);
   #endif

   pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);
   return 1;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *pd, PDSystemEvent event, uint32_t unused_arg)
{
   // Check for consistency of constants between our header files and
   // Playdate SDK.
   assert(SCREEN_WIDTH == LCD_COLUMNS);
   assert(SCREEN_HEIGHT == LCD_ROWS);
   assert(SCREEN_STRIDE == LCD_ROWSIZE);

   // Check other miscellaneous constants.
   assert(GAME_EASY_SIZE > 3);
   assert(GAME_EASY_SIZE < GAME_HARD_SIZE);
   assert(GAME_HARD_SIZE <= MAX_TRADER_COUNT);

   switch( event )
   {
      case kEventInit:
         #ifndef NDEBUG
            {
               const int seed = pd->system->getSecondsSinceEpoch(NULL);
               pd->system->logToConsole("random seed = %d", seed);
               srand(seed);
            }
         #else
            srand(pd->system->getSecondsSinceEpoch(NULL));
         #endif

         pd->system->setUpdateCallback(Update, pd);
         pd->display->setRefreshRate(30);

         pd->system->addMenuItem("reset", MenuActionReset, pd);
         g_hint_option = pd->system->addOptionsMenuItem(
            "hint",
            kHintOptions,
            3,
            MenuActionHint,
            pd);
         MenuActionReset(pd);

         // Skip the transition state for initial start up.  This is
         // so that there is a seamless transition from card view to
         // game title screen.
         g_game_state = kGameTitleScreen;
         break;

      case kEventPause:
         switch( g_game_state )
         {
            case kGameTitleScreen:     SetTitleMenuImage(pd);        break;
            case kGameInProgress:      SetGameRunningMenuImage(pd);  break;
            case kGameDemoInProgress:  SetGameOverMenuImage(pd);     break;
            case kGameDemoDone:        SetGameOverMenuImage(pd);     break;
            case kGameOver:            SetGameOverMenuImage(pd);     break;
            default:                   SetTransitionMenuImage(pd);   break;
         }
         break;

      default:
         break;
   }
   return 0;
}
