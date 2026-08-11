#include"world.h"
#include<math.h>
#include"common.h"
#include"data.h"
#include"item.h"
#include"trade.h"

// Image dimensions in pixels.
#define PLAYER_WIDTH       80
#define PLAYER_HEIGHT      48
#define ITEM_SIZE          48
#define GOAL_ITEM_SIZE     (ITEM_SIZE * 2)
#define SWAP_SIGN_WIDTH    256
#define SWAP_SIGN_HEIGHT   112
#define FLOOR_TILE_SIZE    32
#define FLOOR_TILE_MASK    (FLOOR_TILE_SIZE - 1)

// Triangle edge length in pixels.
#define TRIANGLE_LEG       10
#define TRIANGLE_HEIGHT    ((int)(TRIANGLE_LEG / 1.4142135623730951))
#define HINT_HEIGHT        16

// Triangle positions in pixels.
#define OFFSCREEN_MARGIN   10
#define HINT_MARGIN        18

// Separation between items in frames.  Each item takes the previous
// position of an earlier item after this many frames.
#define ITEM_FOLLOW_DELAY     5

// Number of points to track in player movement history.
#define MOVEMENT_HISTORY_SIZE    ((MAX_ITEM_COUNT + 1) * ITEM_FOLLOW_DELAY)

// All game states.
typedef struct
{
   // Player position.  Game will scroll to always keep this at the
   // center of the screen.
   int scroll_x, scroll_y;

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
} GameState;
static GameState g_state;

// Image handles.
static LCDBitmapTable *g_avatar = NULL;
static LCDBitmapTable *g_items = NULL;
static LCDBitmap *g_goal = NULL;
static LCDBitmapTable *g_swap_sign = NULL;
static LCDBitmapTable *g_floor = NULL;

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

   g_goal = pd->graphics->loadBitmap("goal", &error);
   assert(g_goal != NULL);

   g_swap_sign = pd->graphics->loadBitmapTable("swap.png", &error);
   assert(g_swap_sign != NULL);

   g_floor = pd->graphics->loadBitmapTable("floor.png", &error);
   assert(g_floor != NULL);

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

      pd->graphics->getBitmapTableInfo(g_floor, &count, NULL);
      assert(count == 32);

      pd->graphics->getBitmapData(
         pd->graphics->getTableBitmap(g_floor, 0),
         &width,
         &height,
         NULL,
         NULL,
         NULL);
      assert(width == FLOOR_TILE_SIZE);
      assert(height == FLOOR_TILE_SIZE);
   #endif
}

// Initialize hint index.
static void InitHint(void)
{
   int items[MAX_ITEM_COUNT];
   for(int i = 0; i < g_state.player_items.item_count; i++)
      items[i] = g_state.player_items.item[i].id;
   g_state.hint = FindNextTrader(items, g_state.player_items.item_count);
}

// Initialize game states.
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
            new_item->target.x = g_trader[i].x + ITEM_SIZE;
            new_item->current.y =
            new_item->target.y = g_trader[i].y - ITEM_SIZE / 2;
         }
         else
         {
            new_item->current.x =
            new_item->target.x = g_trader[i].x + kItemPile[j][0];
            new_item->current.y =
            new_item->target.y = g_trader[i].y + kItemPile[j][1];
         }
      }
   }

   // Initialize hint.
   InitHint();

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

// Draw floor tiles.
static void DrawFloor(PlaydateAPI *pd)
{
   const int x_tile_offset = g_state.scroll_x & FLOOR_TILE_MASK;
   const int y_tile_offset = g_state.scroll_y & FLOOR_TILE_MASK;
   const int x0 = g_state.scroll_x - x_tile_offset;
   const int y0 = g_state.scroll_y - y_tile_offset;

   for(int y = -FLOOR_TILE_SIZE; y <= SCREEN_HEIGHT + FLOOR_TILE_SIZE;
       y += FLOOR_TILE_SIZE)
   {
      for(int x = -FLOOR_TILE_SIZE; x <= SCREEN_WIDTH + FLOOR_TILE_SIZE;
          x += FLOOR_TILE_SIZE)
      {
         const uint32_t h = HashXY(x0 + x, y0 + y);
         pd->graphics->drawBitmap(
            pd->graphics->getTableBitmap(g_floor, h & 31),
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
static void DrawItem(PlaydateAPI *pd, int x, int y, int item_id, int variation)
{
   if( item_id == GOAL_ITEM_ID )
   {
      pd->graphics->drawBitmap(g_goal, x, y, kBitmapUnflipped);
   }
   else
   {
      assert(item_id >= 1);
      assert(item_id <= MAX_ITEM_ID);
      pd->graphics->drawBitmap(
         pd->graphics->getTableBitmap(g_items, (item_id - 1) * 4 + variation),
         x,
         y,
         kBitmapUnflipped);
   }
}

// Draw a set of items offered or requested by trader.
static void DrawStaticItemList(PlaydateAPI *pd,
                               int x, int y, const int *items, int item_count)
{
   struct
   {
      int x, y;
   } bitmap_position[4];

   if( item_count == 1 )
   {
      bitmap_position[0].x = x + ITEM_SIZE / 2;
      bitmap_position[0].y = y + ITEM_SIZE / 2;
   }
   else if( item_count == 2 )
   {
      bitmap_position[0].x = x;
      bitmap_position[0].y = y + ITEM_SIZE / 2;
      bitmap_position[1].x = x + ITEM_SIZE;
      bitmap_position[1].y = y + ITEM_SIZE / 2;
   }
   else if( item_count == 3 )
   {
      bitmap_position[0].x = x + ITEM_SIZE / 2;
      bitmap_position[0].y = y;
      bitmap_position[1].x = x;
      bitmap_position[1].y = y + ITEM_SIZE;
      bitmap_position[2].x = x + ITEM_SIZE;
      bitmap_position[2].y = y + ITEM_SIZE;
   }
   else if( item_count == 4 )
   {
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
      DrawItem(pd, bitmap_position[i].x, bitmap_position[i].y, items[i], 0);
}

// Draw all in-motion items.
static void DrawMovableItemList(PlaydateAPI *pd, const ItemList *items)
{
   // Draw items from back to front, such that the front of the list is
   // drawn on top of items behind it.
   for(int i = items->item_count; i--;)
   {
      assert(items->item[i].id >= 1);
      assert(items->item[i].id <= MAX_ITEM_ID ||
             items->item[i].id == GOAL_ITEM_ID);
      const int draw_x =
         items->item[i].current.x - g_state.scroll_x + SCREEN_WIDTH / 2;
      const int draw_y =
         items->item[i].current.y - g_state.scroll_y + SCREEN_HEIGHT / 2;
      if( items->item[i].id == GOAL_ITEM_ID )
      {
         DrawItem(pd,
                  draw_x - ITEM_SIZE / 2,
                  draw_y - ITEM_SIZE / 2,
                  GOAL_ITEM_ID,
                  0);
      }
      else
      {
         DrawItem(pd, draw_x, draw_y, items->item[i].id, 0);
      }
   }
}

// Draw all visible traders and items.
static void DrawTraders(PlaydateAPI *pd, int trader_count)
{
   int more_up_left = 0, more_up = 0, more_up_right = 0,
       more_left = 0, more_right = 0,
       more_down_left = 0, more_down = 0, more_down_right = 0;

   for(int i = 0; i < trader_count; i++)
   {
      const Trader *t = &(g_trader[i]);

      // Convert to screen coordinates.
      const int tx = t->x - g_state.scroll_x + SCREEN_WIDTH / 2;
      const int ty = t->y - g_state.scroll_y + SCREEN_HEIGHT / 2;

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
         const uint32_t h1 = HashXY(t->x, t->y);

         // Index here start at 4 since we skip the positions reserved for
         // offered items.
         for(int i = 4; i < ITEM_PILE_SIZE; i++)
         {
            const uint32_t h2 = HashXY(h1, i);
            DrawItem(pd,
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
   }

   // Draw all in-motion items.
   DrawMovableItemList(pd, &(g_state.trader_items));
   DrawMovableItemList(pd, &(g_state.player_items));

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
void DrawWorld(PlaydateAPI *pd, int game_size, int frames)
{
   // Don't need to clear screen, since DrawFloor will cover every pixel.
   DrawFloor(pd);

   DrawTraders(pd, game_size);
   pd->graphics->drawBitmap(
      pd->graphics->getTableBitmap(g_avatar,
                                   g_state.direction + ((frames >> 3) & 1)),
      SCREEN_WIDTH / 2 - PLAYER_WIDTH / 2,
      SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2,
      kBitmapUnflipped);
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
   const int dx = g_trader[g_state.hint].x - g_state.scroll_x;
   const int dy = g_trader[g_state.hint].y - g_state.scroll_y;

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

// Apply movement.
void MakeMove(int dx, int dy)
{
   if( dx == 0 && dy == 0 )
      return;

   if( dx != 0 )
      g_state.direction = dx < 0 ? 0 : 2;

   g_state.position[g_state.position_index].x = g_state.scroll_x;
   g_state.position[g_state.position_index].y = g_state.scroll_y;
   g_state.position_index =
      (g_state.position_index + 1) % MOVEMENT_HISTORY_SIZE;

   g_state.scroll_x += dx;
   g_state.scroll_y += dy;
}

// If player is currently on top of a swap sign, return index of the trader
// that owns it, otherwise return -1.
static int FindTrader(int game_size)
{
   for(int i = 0; i < game_size; i++)
   {
      if( abs(g_trader[i].x - g_state.scroll_x) <=
             SWAP_SIGN_WIDTH / 2 + PLAYER_WIDTH / 2 &&
          abs(g_trader[i].y - g_state.scroll_y) <=
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
      "guitar"       // 16
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
      a->target.x = t->x + kItemPile[trader_item_position][0];
      a->target.y = t->y + kItemPile[trader_item_position][1];
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
   InitHint();
   return 1;
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
