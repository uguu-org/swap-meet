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
//
// Setting GAME_HARD_SIZE to MAX_TRADER_COUNT turns out to be too hard,
// so the numbers here are a bit smaller.
#define GAME_EASY_SIZE        10
#define GAME_HARD_SIZE        20

// Movement speed in pixels per frame.  Note that both velocities are even,
// so we will always scroll by even number of pixels.
#define ORTHOGONAL_SPEED      12
#define DIAGONAL_SPEED        10

// Syntactic sugar.
#define ANY_DIRECTION         (kButtonUp|kButtonDown|kButtonLeft|kButtonRight)

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
   kGameInProgress,
   kGameOver,
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
static int g_game_total_trades;

// Hint display options.
static const char *kHintOptions[3] = { "hide", "stable", "flash" };
static PDMenuItem *g_hint_option = NULL;

// If nonzero, draw triangles showing where to go next.
static int g_show_hints;

// If nonzero, show frame rate in bottom left corner.
static int g_debug_show_fps;

// Selected instrument.  Currently always zero.
//
// This is meant as a feature for changing currently loaded instrument,
// but that no longer works as of 3.1.0.
// https://devforum.play.date/t/loadintosample-no-longer-replaces-sound-sample-as-of-3-1-0/25975
static int g_sample_index;

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
         InitBgm(pd, g_sample_index);
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

// Transition from title screen to start of game.
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
         g_game_total_trades = 0;
      }

      // Draw game graphics.
      UpdateWorld();
      DrawWorld(pd, g_game_size, g_game_frames);

      // Draw progressively more transparent white rectangles over
      // game graphics for fade-in effect.
      const int opacity = (30 - g_game_frames) * 64 / 15;
      pd->graphics->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                             (LCDColor)kTranslucentWhite[opacity]);
   }

   if( g_game_frames >= 30 )
   {
      g_game_time_ms = 0;
      g_game_frames = 0;
      g_game_state = kGameInProgress;
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
   int dx = 0, dy = 0;
   if( (current & kButtonUp) != 0 )
   {
      if( (current & kButtonLeft) != 0 )
      {
         dx = -DIAGONAL_SPEED;
         dy = -DIAGONAL_SPEED;
      }
      else if( (current & kButtonRight) != 0 )
      {
         dx = DIAGONAL_SPEED;
         dy = -DIAGONAL_SPEED;
      }
      else
      {
         dy = -ORTHOGONAL_SPEED;
      }
   }
   else if( (current & kButtonDown) != 0 )
   {
      if( (current & kButtonLeft) != 0 )
      {
         dx = -DIAGONAL_SPEED;
         dy = DIAGONAL_SPEED;
      }
      else if( (current & kButtonRight) != 0 )
      {
         dx = DIAGONAL_SPEED;
         dy = DIAGONAL_SPEED;
      }
      else
      {
         dy = ORTHOGONAL_SPEED;
      }
   }
   else if( (current & kButtonLeft) != 0 )
   {
      dx = -ORTHOGONAL_SPEED;
   }
   else if( (current & kButtonRight) != 0 )
   {
      dx = ORTHOGONAL_SPEED;
   }
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
         g_game_total_trades++;
         if( ReachedGoal() )
         {
            g_game_complete_duration_ms = g_game_time_ms;
            g_game_state = kGameOver;
         }
      }
      else
      {
         // TODO: we should give player some feedback that the trade
         // did not happen, either because they are not close enough
         // to the trader or because they don't have all the requested
         // items, but for now we do nothing.
      }
   }

   // Draw game state.
   UpdateWorld();
   DrawWorld(pd, g_game_size, g_game_frames);
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
   // Press button to return to title screen.
   if( HandleInput(pd) )
      MenuActionReset(pd);

   // Draw game stats on top of game state.
   UpdateWorld();
   DrawWorld(pd, g_game_size, g_game_frames);
   DrawGameStats(pd, g_game_complete_duration_ms, g_game_total_trades);
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
      case kGameTransitionGameToTitle:    TransitionGameToTitle(pd);    break;
      case kGameInProgress:               RunGame(pd);                  break;
      case kGameOver:                     GameOver(pd);                 break;
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
         break;

      case kEventPause:
         switch( g_game_state )
         {
            case kGameTitleScreen: SetTitleMenuImage(pd);       break;
            case kGameInProgress:  SetGameRunningMenuImage(pd); break;
            case kGameOver:        SetGameOverMenuImage(pd);    break;
            default:               SetTransitionMenuImage(pd);  break;
         }
         break;

      default:
         break;
   }
   return 0;
}
