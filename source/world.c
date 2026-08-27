#include"world.h"
#include<math.h>
#include"common.h"
#include"data.h"
#include"item.h"
#include"trade.h"

// Image dimensions in pixels.
#define PLAYER_WIDTH             80
#define PLAYER_HEIGHT            48
#define ITEM_SIZE                48
#define SMALL_ITEM_SIZE          16
#define GOAL_ITEM_SIZE           (ITEM_SIZE * 2)
#define SWAP_SIGN_WIDTH          256
#define SWAP_SIGN_HEIGHT         112
#define DIRECTION_SIGN_SIZE      72
#define FLOOR_TILE_SIZE          32
#define FLOOR_TILE_MASK          (FLOOR_TILE_SIZE - 1)
#define LANDMARK_TILE_SIZE       192
#define FIREWORKS_TILE_SIZE      100

// Number of firework animation frames.
#define FIREWORKS_FRAMES         30

// Number of fireworks to spawn at each beat.
#define FIREWORKS_PER_BEAT       4

// Maximum number of live fireworks.
//
// - New fireworks are spawned at each beat, which 70*8 = 560ms.
// - We run at 30fps, and each firework lives for 30 frames, so each
//   firework lives for exactly 1 second.
// - This means each firework overlaps for 2 beats.  Thus the number
//   of fireworks we track should be FIREWORKS_PER_BEAT*2.
#define FIREWORKS_COUNT          (FIREWORKS_PER_BEAT * 2)

// Triangle edge length in pixels.
#define TRIANGLE_LEG             10
#define TRIANGLE_HEIGHT          ((int)(TRIANGLE_LEG / 1.4142135623730951))
#define HINT_HEIGHT              16

// Triangle positions in pixels.
#define OFFSCREEN_MARGIN         10
#define HINT_MARGIN              18

// Separation between items in frames.  Each item takes the previous
// position of an earlier item after this many frames.
#define ITEM_FOLLOW_DELAY        5

// Movement speed in pixels per frame.  Note that both velocities are even,
// so we will always scroll by even number of pixels.
#define ORTHOGONAL_SPEED      12
#define DIAGONAL_SPEED        10

// Number of points to track in player movement history.
#define MOVEMENT_HISTORY_SIZE    ((MAX_ITEM_COUNT + 1) * ITEM_FOLLOW_DELAY)

// Wait this many milliseconds at each trader for demo mode.
#define AUTO_MOVE_DELAY_MS    500

// In-flight firework state.
typedef struct
{
   // Spawn position in world coordinates.
   XY position;

   // Frame counter at spawn time.  Delta from this counter is used to
   // select which animation frame to draw.
   int t;
} Firework;

// All game states.
typedef struct
{
   // Player position.  Game will scroll to always keep this at the
   // center of the screen.
   XY scroll;

   // History of player positions, updated on movement.
   XY position[MOVEMENT_HISTORY_SIZE];
   int position_index;

   // Current player direction, either 0 (facing left) or 2 (facing right).
   int direction;

   // Recommended next trader to visit, or -1 if no good trader are found.
   int hint;

   // All items currently held by player.
   ItemList player_items;

   // All items currently held by traders.
   ItemList trader_items;

   // Demo mode: Trader visit states.  If a trader's request/offer
   // state is fully visible, the corresponding array entry will be
   // set to 1.
   //
   // When a normal human plays the game, the first thing they do is
   // usually to explore the map and collect information on all
   // possible trades.  We simulate that in demo mode by requiring
   // that all trade signs have been seen before following hints.
   uint8_t seen_trader[MAX_TRADER_COUNT];
   int seen_trader_count;
   int last_seen_trader_count;

   // Demo mode: Next trader to visit.  If seen_trader_count indicates
   // that we have already seen all the signs, this field is ignored
   // and "hint" field is used instead.
   int next_trader_to_visit;

   // Demo mode: Millisecond timestamp before next movement is allowed.
   // This is to add a bit of delay after completing an action.
   int next_action_start_time;

   // Landmark positions.
   //
   // These are positions of the center of the bottom edge in world
   // coordinates.  The Y positions are also used for Z-sorting.
   //
   // The size of this array matches number of variations exactly, since we
   // never draw duplicate landmarks in the game.  This is so that players
   // can find their way around by memorizing unique landmark positions.
   XY landmarks[LANDMARK_VARIATIONS];

   // Total number of swaps performed.
   int total_trades;

   // Ring buffer of firework objects.
   Firework fireworks[FIREWORKS_COUNT];
   int next_firework_index;

   // Beat index of when the last firework was spawn.
   int last_firework_beat;
} GameState;
static GameState g_state;

// A buffered sprite.
typedef struct
{
   // Sprite position in screen coordinates.
   XY screen;

   // Sprite display order.  Given two sprites with the same Z value,
   // the sprite that is added to the buffer later will be drawn on top.
   int z;

   // Pointer to bitmap handle.
   LCDBitmap *image;
} Sprite;

// Container for pending sprites.
//
// This is used to hold sprites that require Z-sorting before they are drawn.
//
// This struct will cost a few kilobytes.  But unlike other similarly
// sized objects, this one we will allocate on the stack inside
// DrawTradersAndItems.  This is so that all the sprite sorting
// activity are done inside tightly-coupled memory.
typedef struct
{
   Sprite sprite[MAX_ITEM_COUNT + 1];
   int count;
} SpriteList;

// Image handles.
static LCDBitmapTable *g_avatar = NULL;
static LCDBitmapTable *g_items = NULL;
static LCDBitmapTable *g_small_items = NULL;
static LCDBitmap *g_goal = NULL;
static LCDBitmapTable *g_swap_sign = NULL;
static LCDBitmapTable *g_direction_sign = NULL;
static LCDBitmapTable *g_floor = NULL;
static LCDBitmapTable *g_landmark = NULL;
static LCDBitmapTable *g_fireworks = NULL;

// Hash seed, randomized by ResetWorld().
static uint32_t g_hash_seed = 0;

// Hash helper, see HashXY() below.
static uint32_t Murmur32Scramble(uint32_t k)
{
   k *= 0xcc9e2d51;
   k = (k << 15) | (k >> 17);
   k *= 0x1b873593;
   return k;
}

// Hash a pair of values into a 32 bit unsigned value.
uint32_t HashXY(int x, int y)
{
   uint32_t h = g_hash_seed;

   // Two round of Murmur3.
   // https://en.wikipedia.org/wiki/MurmurHash
   h ^= Murmur32Scramble(x);
   h = (h << 13) | (h >> 19);
   h = h * 5 + 0xe6546b64;

   h ^= Murmur32Scramble(y);
   h = (h << 13) | (h >> 19);
   h = h * 5 + 0xe6546b64;

   // Finalize.
   h ^= 8;
   h ^= h >> 16;
   h *= 0x85ebca6b;
   h ^= h >> 13;
   h *= 0xc2b2ae35;
   h ^= h >> 16;
   return h;
}

// Load images.
void InitImages(PlaydateAPI *pd)
{
   const char *error;
   g_avatar = pd->graphics->loadBitmapTable("player", &error);
   assert(g_avatar != NULL);

   g_items = pd->graphics->loadBitmapTable("items", &error);
   assert(g_items != NULL);

   g_small_items = pd->graphics->loadBitmapTable("small_items", &error);
   assert(g_small_items != NULL);

   g_goal = pd->graphics->loadBitmap("goal", &error);
   assert(g_goal != NULL);

   g_swap_sign = pd->graphics->loadBitmapTable("swap", &error);
   assert(g_swap_sign != NULL);

   g_direction_sign = pd->graphics->loadBitmapTable("direction", &error);
   assert(g_direction_sign != NULL);

   g_floor = pd->graphics->loadBitmapTable("floor", &error);
   assert(g_floor != NULL);

   g_landmark = pd->graphics->loadBitmapTable("landmarks", &error);
   assert(g_landmark != NULL);

   g_fireworks = pd->graphics->loadBitmapTable("fireworks", &error);
   assert(g_fireworks != NULL);

   #ifndef NDEBUG
      int count;
      pd->graphics->getBitmapTableInfo(g_avatar, &count, NULL);
      assert(count == 4);

      int width, height;
      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_avatar, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == PLAYER_WIDTH);
      assert(height == PLAYER_HEIGHT);

      pd->graphics->getBitmapTableInfo(g_items, &count, NULL);
      assert(count == 4 * MAX_ITEM_ID);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_items, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == ITEM_SIZE);
      assert(height == ITEM_SIZE);

      pd->graphics->getBitmapTableInfo(g_small_items, &count, NULL);
      assert(count == MAX_ITEM_ID);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_small_items, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == SMALL_ITEM_SIZE);
      assert(height == SMALL_ITEM_SIZE);

      pd->graphics->getBitmapData(g_goal, &width, &height, NULL, NULL, NULL);
      assert(width == GOAL_ITEM_SIZE);
      assert(height == GOAL_ITEM_SIZE);

      pd->graphics->getBitmapTableInfo(g_swap_sign, &count, NULL);
      assert(count == 2);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_swap_sign, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == SWAP_SIGN_WIDTH);
      assert(height == SWAP_SIGN_HEIGHT);

      pd->graphics->getBitmapTableInfo(g_direction_sign, &count, NULL);
      assert(count == 4);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_direction_sign, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == DIRECTION_SIGN_SIZE);
      assert(height == DIRECTION_SIGN_SIZE);

      pd->graphics->getBitmapTableInfo(g_floor, &count, NULL);
      assert(count == 8 * 5);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_floor, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == FLOOR_TILE_SIZE);
      assert(height == FLOOR_TILE_SIZE);

      pd->graphics->getBitmapTableInfo(g_landmark, &count, NULL);
      assert(count == LANDMARK_VARIATIONS * 8);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_landmark, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == LANDMARK_TILE_SIZE);
      assert(height == LANDMARK_TILE_SIZE);

      pd->graphics->getBitmapTableInfo(g_fireworks, &count, NULL);
      assert(count == FIREWORKS_FRAMES);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_fireworks, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == FIREWORKS_TILE_SIZE);
      assert(height == FIREWORKS_TILE_SIZE);
   #endif
}

// Initialize hint index.
static void InitHint(int game_size)
{
   TradeState state;

   memset(&state, 0, sizeof(state));
   for(int i = 0; i < g_state.player_items.item_count; i++)
      state.item_count[g_state.player_items.item[i].id]++;
   for(int i = 0; i < game_size; i++)
      state.traded[i] = g_trader[i].traded;

   g_state.hint = FindNextTrader(game_size, &state);
}

// Initialize landmark positions.
static void InitLandmarks(int game_size)
{
   // Get the center positions for each landmark.
   GetLandmarkPositions(game_size, g_state.landmarks);

   // Adjust for tile offset so that the coordinates are the center
   // of the bottom edges.  We want the coordinates of the bottom
   // edges for Z-sorting.
   for(int i = 0; i < LANDMARK_VARIATIONS; i++)
      g_state.landmarks[i].y += LANDMARK_TILE_SIZE / 2;
}

// Initialize game states.  This needs to be called after InitTraders().
void ResetWorld(int game_size)
{
   g_hash_seed = rand();

   memset(&g_state, 0, sizeof(g_state));

   // Populate items held by player.
   g_state.player_items.item_count = g_minimum_starting_items;
   for(int i = 0; i < g_minimum_starting_items; i++)
   {
      Item *new_item = &(g_state.player_items.item[i]);
      new_item->id = STARTING_ITEM_ID;
      new_item->trader_id = -1;
      new_item->current.x = new_item->target.x = -ITEM_SIZE / 2;
      new_item->current.y = new_item->target.y = -ITEM_SIZE / 2;
   }

   // Populate items held by traders.
   for(int i = 0; i < game_size; i++)
   {
      for(int j = 0; j < g_trader[i].offer_count; j++)
      {
         Item *new_item =
            &(g_state.trader_items.item[g_state.trader_items.item_count++]);
         new_item->id = g_trader[i].offers[j];
         new_item->trader_id = i;
         if( g_trader[i].offers[j] == GOAL_ITEM_ID )
         {
            assert(j == 0);
            new_item->current.x =
            new_item->target.x = g_trader[i].position.x + ITEM_SIZE;
            new_item->current.y =
            new_item->target.y = g_trader[i].position.y - ITEM_SIZE / 2;
         }
         else
         {
            new_item->current.x =
            new_item->target.x = g_trader[i].position.x + kItemPile[j][0];
            new_item->current.y =
            new_item->target.y = g_trader[i].position.y + kItemPile[j][1];
         }
      }
   }

   // Initialize hint.
   InitHint(game_size);

   // Place landmarks
   InitLandmarks(game_size);

   // Choose a random target to explore for demo mode.
   g_state.next_trader_to_visit = RAND(game_size - 1);
   g_state.next_action_start_time = AUTO_MOVE_DELAY_MS;

   #ifndef NDEBUG
      for(int i = 0; i < g_state.player_items.item_count; i++)
         assert(g_state.player_items.item[i].id == STARTING_ITEM_ID);
      for(int i = 0; i < g_state.trader_items.item_count; i++)
      {
         assert(g_state.trader_items.item[i].id > STARTING_ITEM_ID);
         assert(g_state.trader_items.item[i].id <= MAX_ITEM_ID ||
                g_state.trader_items.item[i].id == GOAL_ITEM_ID);
      }
   #endif
}

#ifndef NDEBUG
static int IsSorted(const SpriteList *sprites)
{
   for(int i = 1; i < sprites->count; i++)
   {
      if( sprites->sprite[i - 1].z > sprites->sprite[i].z )
         return 0;
   }
   return 1;
}
#endif

// Add sprite to buffer.
//
// (x,y) = top left corner of the sprite in screen coordinates.
// z = bottom edge of the sprite in screen coordinates.
static void AddSprite(
   SpriteList *sprites, LCDBitmap *image, int x, int y, int z)
{
   // Find where to insert the sprite using binary search.
   unsigned int l = 0;
   if( sprites->count > 0 )
   {
      unsigned int h = sprites->count;
      while( l != h )
      {
         const unsigned int m = (l + h) / 2;
         assert(m < sprites->count);
         if( z < sprites->sprite[m].z )
         {
            h = m;
         }
         else
         {
            if( l == m )
               break;
            l = m;
         }
      }
      assert(l >= 0);
      assert(l < sprites->count);
      if( z >= sprites->sprite[l].z )
         l++;
      assert(l == sprites->count || z < sprites->sprite[l].z);
      if( l < sprites->count )
      {
         memmove(&(sprites->sprite[l + 1]),
                 &(sprites->sprite[l]),
                 sizeof(Sprite) * (sprites->count - l));
      }
   }
   sprites->sprite[l].image = image;
   sprites->sprite[l].screen.x = x;
   sprites->sprite[l].screen.y = y;
   sprites->sprite[l].z = z;
   sprites->count++;
   assert(IsSorted(sprites));
}

// Draw all buffered sprites.
static void DrawSortedSprites(PlaydateAPI *pd, const SpriteList *sprites)
{
   for(int i = 0; i < sprites->count; i++)
   {
      const Sprite *s = &(sprites->sprite[i]);
      pd->graphics->drawBitmap(s->image,
                               s->screen.x,
                               s->screen.y,
                               kBitmapUnflipped);
   }
}

// Draw floor tiles.
static void DrawFloor(PlaydateAPI *pd, int ticks)
{
   const int x_tile_offset = g_state.scroll.x & FLOOR_TILE_MASK;
   const int y_tile_offset = g_state.scroll.y & FLOOR_TILE_MASK;
   const int x0 = g_state.scroll.x - x_tile_offset;
   const int y0 = g_state.scroll.y - y_tile_offset;

   for(int y = -FLOOR_TILE_SIZE; y <= SCREEN_HEIGHT + FLOOR_TILE_SIZE;
       y += FLOOR_TILE_SIZE)
   {
      for(int x = -FLOOR_TILE_SIZE; x <= SCREEN_WIDTH + FLOOR_TILE_SIZE;
          x += FLOOR_TILE_SIZE)
      {
         // Each tile contains 8 variations with a 16-frame animation
         // cycle.  The 16-frame animation cycle is actually just 5
         // images looped in a ping-pong fashion, 2 frames per image.
         //
         // Here we select the variation and animation phase by
         // hashing the coordinates.  From the phase and current clock
         // tick, we derive the animation frame, which is then used to
         // derive the base offset in the image table.  Adding base
         // and variation gets us the image index.
         const uint32_t h = HashXY(x0 + x, y0 + y);
         const uint32_t variation = h & 7;
         const uint32_t frame = ((h >> 3) + ticks) & 15;
         const uint32_t base = ((frame >= 8 ? 16 - frame : frame) & 14) << 2;
         pd->graphics->drawBitmap(
            pd->graphics->getTableBitmap(g_floor, base + variation),
            x - x_tile_offset,
            y - y_tile_offset,
            kBitmapUnflipped);
      }
   }
}

// Draw black triangle with white outline.
static void DrawBlackTriangle(PlaydateAPI *pd,
                              int x0, int y0, int x1, int y1, int x2, int y2)
{
   pd->graphics->fillTriangle(x0, y0, x1, y1, x2, y2, kColorBlack);
   pd->graphics->drawLine(x0, y0, x1, y1, 1, kColorWhite);
   pd->graphics->drawLine(x1, y1, x2, y2, 1, kColorWhite);
   pd->graphics->drawLine(x2, y2, x0, y0, 1, kColorWhite);
}

// Draw white triangle with black outline.
static void DrawWhiteTriangle(PlaydateAPI *pd,
                              int x0, int y0, int x1, int y1, int x2, int y2)
{
   pd->graphics->fillTriangle(x0, y0, x1, y1, x2, y2, kColorWhite);
   pd->graphics->drawLine(x0, y0, x1, y1, 1, kColorBlack);
   pd->graphics->drawLine(x1, y1, x2, y2, 1, kColorBlack);
   pd->graphics->drawLine(x2, y2, x0, y0, 1, kColorBlack);
}

// Draw a single item, with upper left corner at (x,y) in screen coordinates.
static void DrawItem(PlaydateAPI *pd,
                     SpriteList *sprites,
                     int x,
                     int y,
                     int item_id,
                     int variation)
{
   if( item_id == GOAL_ITEM_ID )
   {
      if( sprites == NULL )
         pd->graphics->drawBitmap(g_goal, x, y, kBitmapUnflipped);
      else
         AddSprite(sprites, g_goal, x, y, y + ITEM_SIZE);
   }
   else
   {
      assert(item_id >= 1);
      assert(item_id <= MAX_ITEM_ID);
      LCDBitmap *image =
         pd->graphics->getTableBitmap(g_items, (item_id - 1) * 4 + variation);
      if( sprites == NULL )
         pd->graphics->drawBitmap(image, x, y, kBitmapUnflipped);
      else
         AddSprite(sprites, image, x, y, y + ITEM_SIZE / 2);
   }
}

// Draw a set of items offered or requested by trader.
static void DrawStaticItemList(PlaydateAPI *pd,
                               int x, int y, const int *items, int item_count)
{
   XY bitmap_position[4];

   assert(item_count <= MAX_TRANSACTION_SIZE);
   if( item_count == 1 )
   {
      // [0]
      bitmap_position[0].x = x + ITEM_SIZE / 2;
      bitmap_position[0].y = y + ITEM_SIZE / 2;
   }
   else if( item_count == 2 )
   {
      // [0] [1]
      bitmap_position[0].x = x;
      bitmap_position[0].y = y + ITEM_SIZE / 2;
      bitmap_position[1].x = x + ITEM_SIZE;
      bitmap_position[1].y = y + ITEM_SIZE / 2;
   }
   else if( item_count == 3 )
   {
      //   [0]
      // [1] [2]
      bitmap_position[0].x = x + ITEM_SIZE / 2;
      bitmap_position[0].y = y;
      bitmap_position[1].x = x;
      bitmap_position[1].y = y + ITEM_SIZE;
      bitmap_position[2].x = x + ITEM_SIZE;
      bitmap_position[2].y = y + ITEM_SIZE;
   }
   else if( item_count == 4 )
   {
      // [0] [1]
      // [2] [3]
      bitmap_position[0].x = x;
      bitmap_position[0].y = y;
      bitmap_position[1].x = x + ITEM_SIZE;
      bitmap_position[1].y = y;
      bitmap_position[2].x = x;
      bitmap_position[2].y = y + ITEM_SIZE;
      bitmap_position[3].x = x + ITEM_SIZE;
      bitmap_position[3].y = y + ITEM_SIZE;
   }
   for(int i = 0; i < item_count; i++)
   {
      DrawItem(
         pd, NULL, bitmap_position[i].x, bitmap_position[i].y, items[i], 0);
   }
}

// Draw all in-motion items.
static void DrawMovableItemList(PlaydateAPI *pd,
                                SpriteList *sprites,
                                const ItemList *items)
{
   // Draw items from back to front, such that the front of the list is
   // drawn on top of items behind it.
   for(int i = items->item_count; i--;)
   {
      assert(items->item[i].id >= 1);
      assert(items->item[i].id <= MAX_ITEM_ID ||
             items->item[i].id == GOAL_ITEM_ID);
      const int draw_x =
         items->item[i].current.x - g_state.scroll.x + SCREEN_WIDTH / 2;
      const int draw_y =
         items->item[i].current.y - g_state.scroll.y + SCREEN_HEIGHT / 2;
      if( items->item[i].id == GOAL_ITEM_ID )
      {
         DrawItem(pd,
                  sprites,
                  draw_x - ITEM_SIZE / 2,
                  draw_y - ITEM_SIZE / 2,
                  GOAL_ITEM_ID,
                  0);
      }
      else
      {
         DrawItem(pd, sprites, draw_x, draw_y, items->item[i].id, 0);
      }
   }
}

// Draw landmarks.
static void DrawLandmarks(PlaydateAPI *pd, SpriteList *sprites, int ticks)
{
   const int tile_base = ((ticks & 15) >> 1) * LANDMARK_VARIATIONS;
   for(int i = 0; i < LANDMARK_VARIATIONS; i++)
   {
      const XY *p = &(g_state.landmarks[i]);
      if( p->x == 0 && p->y == 0 )
         continue;

      // Convert world coordinates to screen coordinates.
      const int x = p->x - g_state.scroll.x + SCREEN_WIDTH / 2;
      const int y = p->y - g_state.scroll.y + SCREEN_HEIGHT / 2;

      // Exclude landmarks that are out of bounds.
      if( x + LANDMARK_TILE_SIZE / 2 < 0 ||
          x - LANDMARK_TILE_SIZE / 2 >= SCREEN_WIDTH ||
          y < 0 ||
          y - LANDMARK_TILE_SIZE >= SCREEN_HEIGHT )
      {
         continue;
      }

      // Add landmark to buffer.
      AddSprite(
         sprites,
         pd->graphics->getTableBitmap(g_landmark, tile_base + i),
         x - LANDMARK_TILE_SIZE / 2,
         y - LANDMARK_TILE_SIZE,
         y);
   }
}

// Draw directional signs for each trader.
static void DrawSigns(PlaydateAPI *pd, int trader_count)
{
   for(int i = 0; i < trader_count; i++)
   {
      const Trader *t = &(g_trader[i]);
      for(int j = 0; j < g_trader[i].sign_count; j++)
      {
         // Convert to screen coordinates.
         const XY *s = &(g_trader[i].sign[j]);
         const int sx = s->x - g_state.scroll.x + SCREEN_WIDTH / 2;
         if( sx + DIRECTION_SIGN_SIZE / 2 < 0 ||
             sx - DIRECTION_SIGN_SIZE / 2 >= SCREEN_WIDTH )
         {
            continue;
         }
         const int sy = s->y - g_state.scroll.y + SCREEN_HEIGHT / 2;
         if( sy + DIRECTION_SIGN_SIZE / 2 < 0 ||
             sy - DIRECTION_SIGN_SIZE / 2 >= SCREEN_HEIGHT )
         {
            continue;
         }

         // Draw directional sign.
         const int direction = s->x == t->position.x
            ? s->y > t->position.y ? 0 : 1
            : s->x > t->position.x ? 2 : 3;
         pd->graphics->drawBitmap(
            pd->graphics->getTableBitmap(g_direction_sign, direction),
            sx - DIRECTION_SIGN_SIZE / 2,
            sy - DIRECTION_SIGN_SIZE / 2,
            kBitmapUnflipped);

         // Draw items.
         assert(t->offer_count > 0);
         switch( t->offer_count )
         {
            case 1:
               // [0]
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[0] - 1),
                  sx - SMALL_ITEM_SIZE / 2,
                  sy - SMALL_ITEM_SIZE / 2,
                  kBitmapUnflipped);
               break;
            case 2:
               // [0] [1]
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[0] - 1),
                  sx - SMALL_ITEM_SIZE,
                  sy - SMALL_ITEM_SIZE / 2,
                  kBitmapUnflipped);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[1] - 1),
                  sx,
                  sy - SMALL_ITEM_SIZE / 2,
                  kBitmapUnflipped);
               break;
            case 3:
               //   [0]
               // [1] [2]
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[0] - 1),
                  sx - SMALL_ITEM_SIZE / 2,
                  sy - SMALL_ITEM_SIZE,
                  kBitmapUnflipped);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[1] - 1),
                  sx - SMALL_ITEM_SIZE,
                  sy,
                  kBitmapUnflipped);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[2] - 1),
                  sx,
                  sy,
                  kBitmapUnflipped);
               break;
            default:
               // [0] [1]
               // [2] [3]
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[0] - 1),
                  sx - SMALL_ITEM_SIZE,
                  sy - SMALL_ITEM_SIZE,
                  kBitmapUnflipped);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[1] - 1),
                  sx,
                  sy - SMALL_ITEM_SIZE,
                  kBitmapUnflipped);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[2] - 1),
                  sx - SMALL_ITEM_SIZE,
                  sy,
                  kBitmapUnflipped);
               pd->graphics->drawBitmap(
                  pd->graphics->getTableBitmap(g_small_items, t->offers[3] - 1),
                  sx,
                  sy,
                  kBitmapUnflipped);
               break;
         }
      }
   }
}

// Draw all visible traders and items.
static void DrawTradersAndItems(PlaydateAPI *pd, int trader_count, int ticks)
{
   int more_up_left = 0, more_up = 0, more_up_right = 0,
       more_left = 0, more_right = 0,
       more_down_left = 0, more_down = 0, more_down_right = 0;

   for(int i = 0; i < trader_count; i++)
   {
      const Trader *t = &(g_trader[i]);

      // Convert to screen coordinates.
      const int tx = t->position.x - g_state.scroll.x + SCREEN_WIDTH / 2;
      const int ty = t->position.y - g_state.scroll.y + SCREEN_HEIGHT / 2;

      // Check visibility.
      if( tx + TRADER_SEPARATION < 0 )
      {
         if( ty + TRADER_SEPARATION < 0 )
         {
            more_up_left = 1;
         }
         else if( ty - TRADER_SEPARATION > SCREEN_HEIGHT )
         {
            more_down_left = 1;
         }
         else
         {
            more_left = 1;
         }
         continue;
      }
      else if( tx - TRADER_SEPARATION > SCREEN_WIDTH )
      {
         if( ty + TRADER_SEPARATION < 0 )
         {
            more_up_right = 1;
         }
         else if( ty - TRADER_SEPARATION > SCREEN_HEIGHT )
         {
            more_down_right = 1;
         }
         else
         {
            more_right = 1;
         }
         continue;
      }
      else
      {
         if( ty + TRADER_SEPARATION < 0 )
         {
            more_up = 1;
            continue;
         }
         else if( ty - TRADER_SEPARATION > SCREEN_HEIGHT )
         {
            more_down = 1;
            continue;
         }
      }

      // Draw pile of random items.
      if( t->offers[0] != GOAL_ITEM_ID )
      {
         const uint32_t h1 = HashXY(t->position.x, t->position.y);

         // Index here start at 4 since we skip the positions reserved for
         // offered items.
         for(int i = 4; i < ITEM_PILE_SIZE; i++)
         {
            const uint32_t h2 = HashXY(h1, i);
            DrawItem(pd,
                     NULL,
                     tx + kItemPile[i][0],
                     ty + kItemPile[i][1],
                     t->offers[h2 % t->offer_count],
                     (h2 >> 8) & 3);
         }
      }

      // Draw trade offer.
      pd->graphics->drawBitmap(
         pd->graphics->getTableBitmap(g_swap_sign, t->traded),
         tx - SWAP_SIGN_WIDTH / 2,
         ty - SWAP_SIGN_HEIGHT / 2,
         kBitmapUnflipped);
      DrawStaticItemList(pd,
                         tx - (ITEM_SIZE * 5 / 2),
                         ty - ITEM_SIZE,
                         t->requests,
                         t->request_count);
      if( t->offers[0] != GOAL_ITEM_ID )
      {
         DrawStaticItemList(pd,
                            tx + ITEM_SIZE / 2,
                            ty - ITEM_SIZE,
                            t->offers,
                            t->offer_count);
      }

      // Update seen_trader state.
      if( g_state.seen_trader[i] )
         continue;
      if( tx - ITEM_SIZE * 5 / 2 >= 0 &&
          tx + ITEM_SIZE * 5 / 2 < SCREEN_WIDTH &&
          ty - ITEM_SIZE >= 0 &&
          ty + ITEM_SIZE < SCREEN_HEIGHT )
      {
         g_state.seen_trader[i] = 1;
         g_state.seen_trader_count++;
         #ifndef NDEBUG
            pd->system->logToConsole("seen_trader[%d] = 1 -> seen %d/%d signs",
                                     i,
                                     g_state.seen_trader_count,
                                     trader_count);
         #endif
      }
   }

   // Draw all in-motion items.
   //
   // Note that we have the option of making the trader-owned items
   // participate in Z-sort just by changing the second argument to
   // DrawMovableItemList from "NULL" to "&sprites", but we are not
   // doing that because it was confusing to see the offered items mix
   // with the player items.
   SpriteList sprites;
   sprites.count = 0;
   DrawMovableItemList(pd, NULL, &(g_state.trader_items));
   DrawMovableItemList(pd, &sprites, &(g_state.player_items));

   // Add player sprite.
   AddSprite(
      &sprites,
      pd->graphics->getTableBitmap(g_avatar,
                                   g_state.direction + ((ticks >> 3) & 1)),
      SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2,
      SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2,
      SCREEN_HEIGHT / 2 + PLAYER_HEIGHT / 2);

   // Add landmarks.
   DrawLandmarks(pd, &sprites, ticks);

   // Flush all buffered sprites.
   DrawSortedSprites(pd, &sprites);

   // Draw off-screen indicators.
   if( more_up_left )
   {
      DrawWhiteTriangle(pd,
                        OFFSCREEN_MARGIN, OFFSCREEN_MARGIN,
                        OFFSCREEN_MARGIN + TRIANGLE_LEG, OFFSCREEN_MARGIN,
                        OFFSCREEN_MARGIN, OFFSCREEN_MARGIN + TRIANGLE_LEG);
   }
   if( more_up )
   {
      DrawWhiteTriangle(pd,
                        SCREEN_WIDTH / 2,
                        OFFSCREEN_MARGIN,
                        SCREEN_WIDTH / 2 + TRIANGLE_HEIGHT,
                        OFFSCREEN_MARGIN + TRIANGLE_HEIGHT,
                        SCREEN_WIDTH / 2 - TRIANGLE_HEIGHT,
                        OFFSCREEN_MARGIN + TRIANGLE_HEIGHT);
   }
   if( more_up_right )
   {
      DrawWhiteTriangle(pd,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN,
                        OFFSCREEN_MARGIN,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN - TRIANGLE_LEG,
                        OFFSCREEN_MARGIN,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN,
                        OFFSCREEN_MARGIN + TRIANGLE_LEG);
   }
   if( more_down_left )
   {
      DrawWhiteTriangle(pd,
                        OFFSCREEN_MARGIN,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN,
                        OFFSCREEN_MARGIN + TRIANGLE_LEG,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN,
                        OFFSCREEN_MARGIN,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN - TRIANGLE_LEG);
   }
   if( more_down )
   {
      DrawWhiteTriangle(pd,
                        SCREEN_WIDTH / 2,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN,
                        SCREEN_WIDTH / 2 + TRIANGLE_HEIGHT,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN - TRIANGLE_HEIGHT,
                        SCREEN_WIDTH / 2 - TRIANGLE_HEIGHT,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN - TRIANGLE_HEIGHT);
   }
   if( more_down_right )
   {
      DrawWhiteTriangle(pd,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN - TRIANGLE_LEG,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN,
                        SCREEN_HEIGHT - OFFSCREEN_MARGIN - TRIANGLE_LEG);
   }
   if( more_left )
   {
      DrawWhiteTriangle(pd,
                        OFFSCREEN_MARGIN,
                        SCREEN_HEIGHT / 2,
                        OFFSCREEN_MARGIN + TRIANGLE_HEIGHT,
                        SCREEN_HEIGHT / 2 - TRIANGLE_HEIGHT,
                        OFFSCREEN_MARGIN + TRIANGLE_HEIGHT,
                        SCREEN_HEIGHT / 2 + TRIANGLE_HEIGHT);
   }
   if( more_right )
   {
      DrawWhiteTriangle(pd,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN,
                        SCREEN_HEIGHT / 2,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN - TRIANGLE_HEIGHT,
                        SCREEN_HEIGHT / 2 - TRIANGLE_HEIGHT,
                        SCREEN_WIDTH - OFFSCREEN_MARGIN - TRIANGLE_HEIGHT,
                        SCREEN_HEIGHT / 2 + TRIANGLE_HEIGHT);
   }
}

// Update item positions.
void UpdateWorld(void)
{
   // Set item target positions.  Items held by traders never need to move,
   // so we just need to update items held by players.
   int h = g_state.position_index;
   for(int i = 0; i < g_state.player_items.item_count; i++)
   {
      h -= ITEM_FOLLOW_DELAY;
      if( h < 0 )
         h += MOVEMENT_HISTORY_SIZE;
      g_state.player_items.item[i].target.x =
         g_state.position[h].x - ITEM_SIZE / 2;
      g_state.player_items.item[i].target.y =
         g_state.position[h].y - ITEM_SIZE / 2;
   }

   UpdateItemPositions(&(g_state.player_items));
   UpdateItemPositions(&(g_state.trader_items));
}

// Draw game graphics.
void DrawWorld(PlaydateAPI *pd, int game_size, int ticks)
{
   // Don't need to clear screen, since DrawFloor will cover every pixel.
   DrawFloor(pd, ticks);

   DrawSigns(pd, game_size);
   DrawTradersAndItems(pd, game_size, ticks);
}

// Draw hint triangle with base at the specified coordinates.
static void DrawHintTriangle(PlaydateAPI *pd,
                             int base_x, int base_y, int dx, int dy)
{
   const float f = hypotf(dx, dy);
   const int tip_x = base_x + (int)((dx * HINT_HEIGHT) / f);
   const int tip_y = base_y + (int)((dy * HINT_HEIGHT) / f);

   const int side_x = (tip_y - base_y) / 2;
   const int side_y = (base_x - tip_x) / 2;

   DrawBlackTriangle(pd,
                     tip_x, tip_y,
                     base_x + side_x, base_y + side_y,
                     base_x - side_x, base_y - side_y);
}

// Draw arrow pointing at next recommended trader.
void DrawHint(PlaydateAPI *pd)
{
   if( g_state.hint < 0 )
      return;
   const int dx = g_trader[g_state.hint].position.x - g_state.scroll.x;
   const int dy = g_trader[g_state.hint].position.y - g_state.scroll.y;

   // Try placing hint triangle near the edges of swap sign.
   //
   // We do this first before attempting the more general direction
   // checks.  This is to avoid placing the hint triangle inside the
   // swap sign.  We could do this at the end after the general
   // direction checks are done, but there we run into the problem
   // where the majority of the swap sign is visible, but where we
   // want to place the hint triangle is still off-screen.
   if( abs(dx) < SCREEN_WIDTH / 2 - HINT_MARGIN )
   {
      const int margin = SCREEN_HEIGHT / 2 + SWAP_SIGN_HEIGHT / 2 - HINT_MARGIN;
      if( dy < 0 && dy > -margin )
      {
         DrawHintTriangle(
            pd,
            dx + SCREEN_WIDTH / 2,
            dy + SCREEN_HEIGHT / 2 + SWAP_SIGN_HEIGHT / 2 + HINT_MARGIN,
            0,
            -1);
         return;
      }
      if( dy > 0 && dy < margin )
      {
         DrawHintTriangle(
            pd,
            dx + SCREEN_WIDTH / 2,
            dy + SCREEN_HEIGHT / 2 - (SWAP_SIGN_HEIGHT / 2 + HINT_MARGIN),
            0,
            1);
         return;
      }
   }
   if( abs(dy) < SCREEN_HEIGHT / 2 - HINT_MARGIN )
   {
      const int margin = SCREEN_WIDTH / 2 + SWAP_SIGN_WIDTH / 2 - HINT_MARGIN;
      if( dx < 0 && dx > -margin )
      {
         DrawHintTriangle(
            pd,
            dx + SCREEN_WIDTH / 2 + SWAP_SIGN_WIDTH / 2 + HINT_MARGIN,
            dy + SCREEN_HEIGHT / 2,
            -1,
            0);
         return;
      }
      if( dx > 0 && dx < margin )
      {
         DrawHintTriangle(
            pd,
            dx + SCREEN_WIDTH / 2 - (SWAP_SIGN_WIDTH / 2 + HINT_MARGIN),
            dy + SCREEN_HEIGHT / 2,
            1,
            0);
         return;
      }
   }

   // Draw hint triangle at an oblique angle, point at center of sign.
   if( dx < -(SCREEN_WIDTH / 2 - HINT_MARGIN) )
   {
      // Project dy onto left margin.
      // py/half_width = dy/dx
      const int py = dy * (SCREEN_WIDTH / 2 - HINT_MARGIN) / -dx;
      if( py < -(SCREEN_HEIGHT / 2 - HINT_MARGIN) )
      {
         // Project dx onto top margin.
         // px/half_height = dx/dy
         assert(dy < 0);
         const int px = dx * (SCREEN_HEIGHT / 2 - HINT_MARGIN) / -dy;
         DrawHintTriangle(pd, px + SCREEN_WIDTH / 2, HINT_MARGIN, dx, dy);
      }
      else if( py > SCREEN_HEIGHT / 2 - HINT_MARGIN )
      {
         // Project dx onto bottom margin.
         assert(dy > 0);
         const int px = dx * (SCREEN_HEIGHT / 2 - HINT_MARGIN) / dy;
         DrawHintTriangle(
            pd, px + SCREEN_WIDTH / 2, SCREEN_HEIGHT - HINT_MARGIN, dx, dy);
      }
      else
      {
         DrawHintTriangle(pd, HINT_MARGIN, py + SCREEN_HEIGHT / 2, dx, dy);
      }
   }
   else if( dx > SCREEN_WIDTH / 2 - HINT_MARGIN )
   {
      // Project dy onto right margin.
      const int py = dy * (SCREEN_WIDTH / 2 - HINT_MARGIN) / dx;
      if( py < -(SCREEN_HEIGHT / 2 - HINT_MARGIN) )
      {
         // Project dx onto top margin.
         assert(dy < 0);
         const int px = dx * (SCREEN_HEIGHT / 2 - HINT_MARGIN) / -dy;
         DrawHintTriangle(pd, px + SCREEN_WIDTH / 2, HINT_MARGIN, dx, dy);
      }
      else if( py > SCREEN_HEIGHT / 2 - HINT_MARGIN )
      {
         // Project dx onto bottom margin.
         assert(dy > 0);
         const int px = dx * (SCREEN_HEIGHT / 2 - HINT_MARGIN) / dy;
         DrawHintTriangle(
            pd, px + SCREEN_WIDTH / 2, SCREEN_HEIGHT - HINT_MARGIN, dx, dy);
      }
      else
      {
         DrawHintTriangle(
            pd, SCREEN_WIDTH - HINT_MARGIN, py + SCREEN_HEIGHT / 2, dx, dy);
      }
   }
   else
   {
      if( dy < -(SCREEN_HEIGHT / 2 - HINT_MARGIN) )
      {
         const int px = dx * (SCREEN_HEIGHT / 2 - HINT_MARGIN) / -dy;
         DrawHintTriangle(pd, px + SCREEN_WIDTH / 2, HINT_MARGIN, dx, dy);
      }
      else if( dy > SCREEN_HEIGHT / 2 - HINT_MARGIN )
      {
         const int px = dx * (SCREEN_HEIGHT / 2 - HINT_MARGIN) / dy;
         DrawHintTriangle(
            pd, px + SCREEN_WIDTH / 2, SCREEN_HEIGHT - HINT_MARGIN, dx, dy);
      }
      else
      {
         // Trader should be visible on screen, and this case should
         // have already been covered by the start of the function,
         // but we check it again here just in case.
         if( dx < -SWAP_SIGN_WIDTH / 2 )
         {
            DrawHintTriangle(
               pd,
               dx + SCREEN_WIDTH / 2 + SWAP_SIGN_WIDTH / 2 + HINT_MARGIN,
               dy + SCREEN_HEIGHT / 2,
               -1,
               0);
         }
         else if( dx > SWAP_SIGN_WIDTH / 2 )
         {
            DrawHintTriangle(
               pd,
               dx + SCREEN_WIDTH / 2 - SWAP_SIGN_WIDTH / 2 - HINT_MARGIN,
               dy + SCREEN_HEIGHT / 2,
               1,
               0);
         }
         else
         {
            if( dy > 0 )
            {
               DrawHintTriangle(
                  pd,
                  dx + SCREEN_WIDTH / 2,
                  dy + SCREEN_HEIGHT / 2 - SWAP_SIGN_HEIGHT / 2 - HINT_MARGIN,
                  0,
                  1);
            }
            else
            {
               DrawHintTriangle(
                  pd,
                  dx + SCREEN_WIDTH / 2,
                  dy + SCREEN_HEIGHT / 2 + SWAP_SIGN_HEIGHT / 2 + HINT_MARGIN,
                  0,
                  -1);
            }
         }
      }
   }
}

// Draw and animate fireworks.
void DrawAndUpdateFireworks(PlaydateAPI *pd, int frames, int beats)
{
   assert(kBitmapUnflipped == 0);
   assert(kBitmapFlippedX == 1);
   assert(kBitmapFlippedY == 2);
   assert(kBitmapFlippedXY == 3);

   // Draw all live fireworks.
   const int dx =
      -g_state.scroll.x + SCREEN_WIDTH / 2 - FIREWORKS_TILE_SIZE / 2;
   const int dy =
      -g_state.scroll.y + SCREEN_HEIGHT / 2 - FIREWORKS_TILE_SIZE / 2;
   for(int i = 0; i < FIREWORKS_COUNT; i++)
   {
      const int f = frames - g_state.fireworks[i].t;
      if( f < 0 || f >= FIREWORKS_FRAMES )
         continue;
      const int sx = g_state.fireworks[i].position.x + dx;
      if( sx >= SCREEN_WIDTH || sx + FIREWORKS_TILE_SIZE < 0 )
         continue;
      const int sy = g_state.fireworks[i].position.y + dy;
      if( sy >= SCREEN_HEIGHT || sy + FIREWORKS_TILE_SIZE < 0 )
         continue;

      pd->graphics->drawBitmap(pd->graphics->getTableBitmap(g_fireworks, f),
                               sx,
                               sy,
                               (LCDBitmapFlip)(i & 3));
   }

   // Spawn new fireworks at each beat.
   if( g_state.last_firework_beat == beats )
      return;
   g_state.last_firework_beat = beats;

   for(int i = 0; i < FIREWORKS_PER_BEAT; i++)
   {
      Firework *f = &(g_state.fireworks[g_state.next_firework_index]);
      for(int j = 0; j < 3; j++)
      {
         f->position.x = RAND_RANGE(-SCREEN_WIDTH / 2, SCREEN_WIDTH / 2);
         f->position.y = RAND_RANGE(-SCREEN_HEIGHT / 2, SCREEN_HEIGHT / 2);

         // Prefer fireworks that don't overlap with the center of the
         // screen, so that they are not spawned on top of players.
         if( f->position.x < -PLAYER_WIDTH / 2 - FIREWORKS_TILE_SIZE / 2 ||
             f->position.x > PLAYER_WIDTH / 2 + FIREWORKS_TILE_SIZE / 2 ||
             f->position.y < -PLAYER_HEIGHT / 2 - FIREWORKS_TILE_SIZE / 2 ||
             f->position.y > PLAYER_HEIGHT / 2 + FIREWORKS_TILE_SIZE / 2 )
         {
            break;
         }
      }

      // Convert screen coordinates to world coordinates.
      f->position.x += g_state.scroll.x;
      f->position.y += g_state.scroll.y;

      // Set birth timestamp.
      f->t = frames;

      g_state.next_firework_index =
         (g_state.next_firework_index + 1) % FIREWORKS_COUNT;
   }
}

// Apply movement.
void MakeMove(int dx, int dy)
{
   assert(-1 <= dx && dx <= 1);
   assert(-1 <= dy && dy <= 1);
   if( dx == 0 && dy == 0 )
      return;

   if( (dx & dy) == 0 )
   {
      // Either dx or dy is zero.
      dx *= ORTHOGONAL_SPEED;
      dy *= ORTHOGONAL_SPEED;
   }
   else
   {
      // Both dx and dy are nonzero.
      dx *= DIAGONAL_SPEED;
      dy *= DIAGONAL_SPEED;
   }

   if( dx != 0 )
      g_state.direction = dx < 0 ? 0 : 2;

   g_state.position[g_state.position_index].x = g_state.scroll.x;
   g_state.position[g_state.position_index].y = g_state.scroll.y;
   g_state.position_index =
      (g_state.position_index + 1) % MOVEMENT_HISTORY_SIZE;

   g_state.scroll.x += dx;
   g_state.scroll.y += dy;
}

// If player is currently on top of a swap sign, return index of the trader
// that owns it, otherwise return -1.
static int FindTrader(int game_size)
{
   for(int i = 0; i < game_size; i++)
   {
      if( abs(g_trader[i].position.x - g_state.scroll.x) <=
             SWAP_SIGN_WIDTH / 2 + PLAYER_WIDTH / 2 &&
          abs(g_trader[i].position.y - g_state.scroll.y) <=
             SWAP_SIGN_HEIGHT / 2 + PLAYER_HEIGHT / 2 )
      {
         return i;
      }
   }
   return -1;
}

// Find the list of item indices that would cover the offer or request list.
// Returns nonzero if all items are found.
//
// found_indices will be sorted in ascending order.
static int FindItemIndices(int *request,
                           int request_size,
                           const ItemList *items,
                           int trader_id,
                           int *found_indices)
{
   assert(request_size <= MAX_TRANSACTION_SIZE);

   int covered[MAX_TRANSACTION_SIZE];
   memset(covered, 0, sizeof(covered));

   int number_found = 0;
   for(int i = 0; i < items->item_count; i++)
   {
      if( items->item[i].trader_id != trader_id )
         continue;
      for(int j = 0; j < request_size; j++)
      {
         if( covered[j] )
            continue;
         if( items->item[i].id == request[j] )
         {
            found_indices[number_found++] = i;
            covered[j] = 1;
            break;
         }
      }
   }
   return number_found == request_size;
}

#ifndef NDEBUG
// Return readable label for an item ID.
static const char *ItemLabel(int item_id)
{
   static const char *kItemLabel[MAX_ITEM_ID + 2] =
   {
      "?",           // 0
      "coin",        // 1
      "mushroom",    // 2
      "apple",       // 3
      "watermelon",  // 4
      "carrot",      // 5
      "pear",        // 6
      "grape",       // 7
      "banana",      // 8
      "strawberry",  // 9
      "cheese",      // 10
      "donut",       // 11
      "cup",         // 12
      "cookie",      // 13
      "candy",       // 14
      "taiyaki",     // 15
      "onirigi",     // 16
      "senbei",      // 17
      "pretzel",     // 18
      "dorito",      // 19
      "grass",       // 20
      "ice cream",   // 21
      "guitar"       // 22
   };
   return kItemLabel[item_id < 0 || item_id >= MAX_ITEM_ID + 2 ? 0 : item_id];
}
#endif

// Attempt to make a trade at current location.
#ifndef NDEBUG
int MakeTrade(PlaydateAPI *pd, int game_size, int direction)
#else
int MakeTrade(int game_size, int direction)
#endif
{
   const int trader_index = FindTrader(game_size);
   if( trader_index < 0 )
   {
      // No trader nearby, rotate item list so that player can get a better
      // view of what they have.
      if( direction > 0 )
      {
         RotateListHead(&(g_state.player_items), 1);
      }
      else
      {
         RotateListTail(&(g_state.player_items), 1);
      }
      return 0;
   }
   Trader *t = &(g_trader[trader_index]);

   int give_list[MAX_TRANSACTION_SIZE];
   int take_list[MAX_TRANSACTION_SIZE];
   int give_list_size;
   int take_list_size;
   if( t->traded )
   {
      // Checking player items against trader's offered list, getting
      // requested items in return.  This is basically an undo operation.
      if( !FindItemIndices(t->offers,
                           t->offer_count,
                           &(g_state.player_items),
                           -1,
                           give_list) )
      {
         return 0;
      }
      give_list_size = t->offer_count;

      FindItemIndices(t->requests,
                      t->request_count,
                      &(g_state.trader_items),
                      trader_index,
                      take_list);
      take_list_size = t->request_count;
   }
   else
   {
      // Checking player items against trader's request list, getting
      // offered items in return.  This is a normal trade.
      if( !FindItemIndices(t->requests,
                           t->request_count,
                           &(g_state.player_items),
                           -1,
                           give_list) )
      {
         return 0;
      }
      give_list_size = t->request_count;

      FindItemIndices(t->offers,
                      t->offer_count,
                      &(g_state.trader_items),
                      trader_index,
                      take_list);
      take_list_size = t->offer_count;
   }

   #ifndef NDEBUG
      for(int i = 0; i < g_state.player_items.item_count; i++)
      {
         pd->system->logToConsole("player_item[%d] = %d (%s)",
                                  i,
                                  g_state.player_items.item[i].id,
                                  ItemLabel(g_state.player_items.item[i].id));
      }
      for(int i = 0; i < g_state.trader_items.item_count; i++)
      {
         pd->system->logToConsole("trader_item[%d] = %d (%s) @ %d",
                                  i,
                                  g_state.trader_items.item[i].id,
                                  ItemLabel(g_state.trader_items.item[i].id),
                                  g_state.trader_items.item[i].trader_id);
      }
      for(int i = give_list_size; i--;)
      {
         pd->system->logToConsole(
            "give item[%d]{id=%d (%s)} to %d",
            give_list[i],
            g_state.player_items.item[give_list[i]].id,
            ItemLabel(g_state.player_items.item[give_list[i]].id),
            trader_index);
      }
      for(int i = take_list_size; i--;)
      {
         pd->system->logToConsole(
            "take item[%d]{id=%d (%s)} from %d",
            take_list[i],
            g_state.trader_items.item[take_list[i]].id,
            ItemLabel(g_state.trader_items.item[take_list[i]].id),
            trader_index);
      }
   #endif

   // Move items from player to trader.
   int trader_item_position = 0;
   for(int i = give_list_size; i--;)
   {
      Item *a = MoveItem(&(g_state.player_items),
                         give_list[i],
                         &(g_state.trader_items));
      a->trader_id = trader_index;
      a->target.x = t->position.x + kItemPile[trader_item_position][0];
      a->target.y = t->position.y + kItemPile[trader_item_position][1];
      trader_item_position++;
   }

   // Move items from trader to player.
   for(int i = take_list_size; i--;)
   {
      Item *a = MoveItem(&(g_state.trader_items),
                         take_list[i],
                         &(g_state.player_items));
      a->trader_id = -1;
   }
   t->traded ^= 1;

   // Rotate player items, such that newly acquired items appear at the front.
   RotateListTail(&(g_state.player_items), take_list_size);

   // Update hint after each trade.
   InitHint(game_size);

   // Update counter.
   g_state.total_trades++;
   return 1;
}

// Check total number of trades performed.
int GetTotalTrades(void)
{
   return g_state.total_trades;
}

// Check if player holds the goal item.  Returns 1 if so.
int ReachedGoal(void)
{
   for(int i = 0; i < g_state.player_items.item_count; i++)
   {
      if( g_state.player_items.item[i].id == GOAL_ITEM_ID )
         return 1;
   }
   return 0;
}

// Make an automated movement, with some added noise in the movement.
//
// For diagonal movements only, we will randomly drop one of the axes
// for a few consecutive frames, such that player moves orthogonally
// instead of diagonally, but still roughly in the same direction that
// they were planning to go before.  This random drop makes the
// automated movement slightly less monotonous.
//
// We could do the same for orthogonal movements, i.e. adding diagonal
// component with some random probability.  But this change causes the
// player to diverge from the intended path, and what we observed is
// that the automated player will do a twitch to stray from the
// orthogonal path, and then quickly return to the original path with
// a diagonal movement.  While it does achieve the same effect of
// making orthogonal movements less monotonous, this type of jerky
// movement just seem unnatural, so we don't do anything with the
// orthogonal movements.
static void MakeAutoMove(int dx, int dy, int game_time_ms)
{
   if( (dx & dy) != 0 )
   {
      // Detected diagonal movement.  Here we drop the lowest few bits
      // of the millisecond timestamp such that the same adjustment
      // applies over several consecutive frames.  The quantized
      // timestamp is then hashed to determine whether to make the
      // adjustment or not.
      const uint32_t h = HashXY(game_time_ms >> 7, 0);

      // At a probability of 2/8, drop either the vertical or
      // horizontal movement component.
      if( (h & 7) == 1 )
      {
         MakeMove(dx, 0);
         return;
      }
      else if( (h & 7) == 2 )
      {
         MakeMove(0, dy);
         return;
      }
   }

   // All orthogonal movements will pass through without adjustments.
   //
   // Diagonal movements will also fall through to here at a probability of 6/8.
   MakeMove(dx, dy);
}

// Explore all traders before following hints.
static void AutoExplore(int game_size, int game_time_ms)
{
   // Pause a bit if we have seen a sign that we haven't seen before.
   if( g_state.last_seen_trader_count < g_state.seen_trader_count )
   {
      g_state.last_seen_trader_count = g_state.seen_trader_count;
      g_state.next_action_start_time = game_time_ms + AUTO_MOVE_DELAY_MS;
      return;
   }

   // Find the nearest unvisited trader.
   int nearest_target = -1;
   int nearest_distance = 0;
   for(int i = 0; i < game_size; i++)
   {
      if( g_state.seen_trader[i] )
         continue;

      // Ignore this trader unless it is at least partially visible.
      const int dx = abs(g_trader[i].position.x - g_state.scroll.x);
      if( dx > SCREEN_WIDTH / 2 + SWAP_SIGN_WIDTH / 2 )
         continue;
      const int dy = abs(g_trader[i].position.y - g_state.scroll.y);
      if( dy > SCREEN_HEIGHT / 2 + SWAP_SIGN_HEIGHT / 2 )
         continue;

      // Mark this trader as a potential target to follow, unless we
      // already have a closer candidate.
      if( nearest_target < 0 || nearest_distance > dx + dy )
      {
         nearest_distance = dx + dy;
         nearest_target = i;
      }
   }

   int target;
   if( g_state.seen_trader[g_state.next_trader_to_visit] )
   {
      if( nearest_target >= 0 )
      {
         // The trader we were going to visit has been covered, so we
         // will go toward the nearest unvisited trader instead.
         assert(!g_state.seen_trader[nearest_target]);
         g_state.next_trader_to_visit = nearest_target;
      }
      else
      {
         // The trader we were going to visit has been covered, and
         // there isn't another trader nearby.  We will pick a new
         // unvisited target at random.
         int i = RAND(game_size - 1);
         while( g_state.seen_trader[i] )
            i = (i + 1) % game_size;
         g_state.next_trader_to_visit = i;
      }
      target = g_state.next_trader_to_visit;
   }
   else
   {
      // The trader we were planning on visiting is still a valid target,
      // but we will go with a nearby target if we happened to see one.
      target = nearest_target >= 0 ? nearest_target
                                   : g_state.next_trader_to_visit;
   }
   assert(target >= 0);
   assert(target < game_size);

   // Move toward the center of the selected target.
   const int dx = g_trader[target].position.x - g_state.scroll.x;
   const int dy = g_trader[target].position.y - g_state.scroll.y;
   MakeAutoMove(dx > DIAGONAL_SPEED ? 1 : dx < -DIAGONAL_SPEED ? -1 : 0,
                dy > DIAGONAL_SPEED ? 1 : dy < -DIAGONAL_SPEED ? -1 : 0,
                game_time_ms);
}

// Follow hints to solution.
#ifndef NDEBUG
static int AutoFollowHint(PlaydateAPI *pd, int game_size, int game_time_ms)
#else
static int AutoFollowHint(int game_size, int game_time_ms)
#endif
{
   // Stop demo mode once we have reach the goal.
   if( g_state.hint < 0 )
      return 1;

   // Check player distance to target sign.
   //
   // Note that unlike FindTrader where player only needs to touch the
   // edge of the sign, here we require the player to be fully within the
   // sign.  This is to avoid trouble with signs that are placed too close
   // to each other.
   const int dx = g_trader[g_state.hint].position.x - g_state.scroll.x;
   const int dy = g_trader[g_state.hint].position.y - g_state.scroll.y;
   #define HORIZONTAL_MARGIN  (SWAP_SIGN_WIDTH / 2 - PLAYER_WIDTH / 2)
   #define VERTICAL_MARGIN    (SWAP_SIGN_HEIGHT / 2 - PLAYER_HEIGHT / 2)
   if( abs(dx) <= HORIZONTAL_MARGIN && abs(dy) <= VERTICAL_MARGIN )
   {
      // Player is sufficiently close to target trader, perform the trade.
      #ifndef NDEBUG
         MakeTrade(pd, game_size, 1);
      #else
         MakeTrade(game_size, 1);
      #endif
      g_state.next_action_start_time = game_time_ms + AUTO_MOVE_DELAY_MS;
   }
   else
   {
      // Player is still not close enough, move toward trader.
      MakeAutoMove(
         dx > HORIZONTAL_MARGIN ? 1 : dx < -HORIZONTAL_MARGIN ? -1 : 0,
         dy > VERTICAL_MARGIN ? 1 : dy < -VERTICAL_MARGIN ? -1 : 0,
         game_time_ms);
   }
   #undef HORIZONTAL_MARGIN
   #undef VERTICAL_MARGIN
   return 0;
}

// Move automatically, used for demo mode.
#ifndef NDEBUG
int AutoMove(PlaydateAPI *pd, int game_size, int game_time_ms, int use_hints)
#else
int AutoMove(int game_size, int game_time_ms, int use_hints)
#endif
{
   if( g_state.next_action_start_time > game_time_ms )
      return 0;

   // If hint display is disabled, demo mode will make sure all the traders
   // are visited before following hints.  This simulates the process of
   // learning all the possible trades before making those trades.
   //
   // If hint display is enabled, demo mode will make the right trades
   // immediately without having seen all possible traders, because it would
   // seem weird to ignore the hints that are visible on screen.  Humans may
   // do this because they might be able to plan a shorter path than the one
   // that's found by the hint system, but the demo system doesn't know any
   // better.
   //
   // If hint display was initially enabled, and then disabled while the
   // demo is running, we will see the demo visit all the traders that were
   // not visited earlier before continuing on to make all the trades.
   if( !use_hints && g_state.last_seen_trader_count < game_size )
   {
      AutoExplore(game_size, game_time_ms);
      return 0;
   }

   #ifndef NDEBUG
      return AutoFollowHint(pd, game_size, game_time_ms);
   #else
      return AutoFollowHint(game_size, game_time_ms);
   #endif
}
