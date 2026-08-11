#include"trade.h"
#include<string.h>
#include"common.h"

// Trading graph.
Trader g_trader[MAX_TRADER_COUNT];
int g_minimum_starting_items;

// Check if item is on the request list, returns 1 if so.
static int IsRequested(int trader_index, int item_id)
{
   for(int i = 0; i < g_trader[trader_index].request_count; i++)
   {
      if( g_trader[trader_index].requests[i] == item_id )
         return 1;
   }
   return 0;
}

// Check if item is on the offer list, returns 1 if so.
static int IsOffered(int trader_index, int item_id)
{
   for(int i = 0; i < g_trader[trader_index].offer_count; i++)
   {
      if( g_trader[trader_index].offers[i] == item_id )
         return 1;
   }
   return 0;
}

// Add item to offer list.
static void AddOffer(int trader_index, int item_id)
{
   assert(g_trader[trader_index].offer_count < MAX_TRANSACTION_SIZE);
   g_trader[trader_index].offers[g_trader[trader_index].offer_count++] =
      item_id;
}

// Add item to request list.
static void AddRequest(int trader_index, int item_id)
{
   assert(g_trader[trader_index].request_count < MAX_TRANSACTION_SIZE);
   g_trader[trader_index].requests[g_trader[trader_index].request_count++] =
      item_id;
}

// Add trader to dependent list.
static void AddDependent(int trader_index, int dependent_id)
{
   assert(g_trader[trader_index].dependent_count < MAX_TRANSACTION_SIZE);
   g_trader[trader_index].dependents[g_trader[trader_index].dependent_count++] =
      dependent_id;
}

// Check if newly added trader overlaps with the origin or any previously
// added traders, returns 1 if so.
static int LastTraderOverlapsWithEarlier(int last_index)
{
   const int x = g_trader[last_index].x;
   const int y = g_trader[last_index].y;
   if( x * x + y * y < TRADER_SEPARATION * TRADER_SEPARATION )
      return 1;
   for(int i = 0; i < last_index; i++)
   {
      const int dx = g_trader[i].x - x;
      const int dy = g_trader[i].y - y;
      if( dx * dx + dy * dy < TRADER_SEPARATION * TRADER_SEPARATION )
         return 1;
   }
   return 0;
}

// Populate trader (x,y) fields.
static void InitTraderPositions(int trader_count)
{
   // Quantize random range at this many pixels.
   //
   // This quantization is done because generating random numbers for
   // a larger range tend to result in less random numbers on systems
   // with a low RAND_MAX.
   #define GRID_UNIT    8

   int range = TRADER_SEPARATION / GRID_UNIT;
   for(int i = 0; i < trader_count; i++)
   {
      // Generate coordinates inside a bounding box, slowly increasing
      // the bounding box size if we failed to pick a coordinate without
      // overlaps.  The intent is to cluster traders together.
      for(;; range++)
      {
         g_trader[i].x = RAND_RANGE(-range, range) * GRID_UNIT;
         g_trader[i].y = RAND_RANGE(-range, range) * GRID_UNIT;

         // Stop when the newly generated trader does not overlap.
         if( !LastTraderOverlapsWithEarlier(i) )
            break;
      }
   }
}

#ifndef NDEBUG
// Check that the offers list and requests list do not overlap.
// Returns 1 if list looks fine.
static int CheckNoOverlap(int trader_index)
{
   for(int i = 0; i < g_trader[trader_index].request_count; i++)
   {
      for(int j = 0; j < g_trader[trader_index].offer_count; j++)
      {
         if( g_trader[trader_index].requests[i] ==
             g_trader[trader_index].offers[j] )
         {
            return 0;
         }
      }
   }
   return 1;
}
#endif

// Initialize trading graph.
void InitTraders(int trader_count)
{
   assert(trader_count > 2);
   assert(trader_count <= MAX_TRADER_COUNT);

   memset(g_trader, 0, sizeof(g_trader));
   g_minimum_starting_items = 0;

   // Initialize goal.
   AddOffer(0, GOAL_ITEM_ID);

   // Perform random traversals from goal to leaf, and add offer/request
   // pairs at each node.  This works so that the goal can be attained by
   // trading items from leaf to root.
   //
   // If we only do exactly one traversal, we get a linked list with exactly
   // one solution, which seems boring, so we do multiple traversals and
   // layer the paths on top of each other.  The result is that the root is
   // attainable if player does some type of reverse breadth-first search,
   // collecting dependent items from nodes that are further away from the
   // root to trade with items that are closer to the root.
   //
   // As a side effect of layering multiple traversals:
   //
   // - It's not guaranteed that player now needs to visit every leaf node
   //   to reach the root, since some dependencies may be satisfied via
   //   accidental overlaps in offered items.  So this algorithm guarantees
   //   that a solution exists, but doesn't guarantee that the generated
   //   solution is optimal.
   //
   // - Because player may visit a shallower node that accepts the same
   //   requests as a deeper node, but the shallower node might not have all
   //   the offers needed to satisfy upstream dependencies, an incorrect
   //   visit order may cause the player to get stuck.  For this reason, we
   //   allow trades to be reversed at any node so that players can undo
   //   their mistakes.
   //
   // - Since it's possible to undo mistakes, we also insert distraction
   //   nodes that are not on any of the generated traversal paths, for
   //   variety.  Although these are distraction nodes, there is some chance
   //   that what they offer might actually lead to a shorter path.
   uint8_t used_items[MAX_ITEM_ID + 1];
   while( g_trader[0].request_count < MAX_TRANSACTION_SIZE )
   {
      memset(used_items, 0, sizeof(used_items));
      int cursor = 0;
      for(;;)
      {
         // Select the next trader in the traversal chain.  Because the
         // selection always happens in one direction, we guarantee no
         // loops in this chain.
         //
         // We stop when the next step would request a trader that is
         // at the requested size, such that we reserve at least one
         // trader to serve as distraction node.
         const int step = RAND_RANGE(1, 5);
         if( cursor + step >= trader_count - 1 )
            break;
         const int next = cursor + step;

         // Terminate chain if next trader no longer has capacity to offer
         // or request any more items.
         if( g_trader[next].offer_count >= MAX_TRANSACTION_SIZE ||
             g_trader[next].request_count >= MAX_TRANSACTION_SIZE )
         {
            break;
         }

         // Choose an item that has not yet been requested on this chain,
         // and add it to the request for the current node.
         //
         // Also, to avoid self loops:
         // - Avoid requesting item requested by downstream trader.
         // - Avoid requesting item that the current node offers.
         int item_id = RAND_RANGE(STARTING_ITEM_ID + 1, MAX_ITEM_ID);
         for(int i = 0;
             (used_items[item_id] ||
              IsRequested(next, item_id) ||
              IsOffered(cursor, item_id)) &&
             i < MAX_ITEM_ID;
             i++)
         {
            item_id++;
            if( item_id > MAX_ITEM_ID )
               item_id = STARTING_ITEM_ID + 1;
         }
         if( used_items[item_id] ||
             IsRequested(next, item_id) ||
             IsOffered(cursor, item_id) )
         {
            break;
         }
         used_items[item_id] = 1;

         // Add node to chain.
         AddRequest(cursor, item_id);
         AddOffer(next, item_id);
         AddDependent(cursor, next);
         assert(CheckNoOverlap(cursor));
         cursor = next;
      }

      // Add starting item to leaf node.
      AddRequest(cursor, STARTING_ITEM_ID);
      assert(CheckNoOverlap(cursor));

      // Update required item count for each leaf node.  Note that this is
      // not done for any of the distraction nodes.
      g_minimum_starting_items++;
   }

   // Populate distraction nodes.
   for(int i = 0; i < trader_count; i++)
   {
      if( g_trader[i].request_count > 0 )
         continue;

      // Choose a random downstream node and copy some of their offers.
      const int next =
         i + 1 < trader_count ? RAND_RANGE(i + 1, trader_count - 1) : i;
      if( next != i )
      {
         const int copy_count = RAND(g_trader[next].offer_count);
         if( copy_count > 0 )
         {
            for(int j = 0; j < copy_count; j++)
               g_trader[i].requests[j] = g_trader[next].offers[j];
            g_trader[i].request_count = copy_count;
         }
      }
      if( g_trader[i].request_count == 0 )
         AddRequest(i, STARTING_ITEM_ID);

      // Offer random items.
      int max_offer_count = RAND_RANGE(1, MAX_TRANSACTION_SIZE);
      for(int j = 0; j < max_offer_count; j++)
      {
         int item_id = RAND_RANGE(STARTING_ITEM_ID + 1, MAX_ITEM_ID);
         for(int k = 0; IsRequested(i, item_id) && k < MAX_ITEM_ID; k++)
         {
            item_id++;
            if( item_id > MAX_ITEM_ID )
               item_id = STARTING_ITEM_ID + 1;
         }
         if( IsRequested(i, item_id) )
            break;
         AddOffer(i, item_id);
      }
      assert(CheckNoOverlap(i));
   }

   InitTraderPositions(trader_count);
}

// Check if player can fulfill all the requests for a trader, returns 1 if so.
static int FulfillsRequirements(
   int *player_items, int player_item_count, int trader_index)
{
   // Ignore traders where we have already made a trade.
   if( g_trader[trader_index].traded )
      return 0;

   int covered[MAX_TRANSACTION_SIZE];
   memset(covered, 0, sizeof(covered));
   int request_coverage = 0;
   for(int i = 0; i < player_item_count; i++)
   {
      for(int j = 0; j < g_trader[trader_index].request_count; j++)
      {
         if( covered[j] )
            continue;
         if( player_items[i] == g_trader[trader_index].requests[j] )
         {
            request_coverage++;
            if( request_coverage == g_trader[trader_index].request_count )
               return 1;
            covered[j] = 1;
            break;
         }
      }
   }
   return 0;
}

// Find next trader to visit to get the player one step closer to solution.
int FindNextTrader(int *player_items, int player_item_count)
{
   // Do a breadth-first expansion starting from the root node to find
   // the first trader where player can fulfill all requirements.
   int visit_queue[MAX_TRADER_COUNT * MAX_TRANSACTION_SIZE] = {0};
   int queue_head = 0, queue_tail = 1;
   while( queue_head < queue_tail )
   {
      const int t = visit_queue[queue_head++];
      if( FulfillsRequirements(player_items, player_item_count, t) )
         return t;

      // Enqueue the dependencies in reverse order.
      //
      // Intuitively, we can find a solution by doing the exact reverse of
      // what InitTraders did.  The dependency pointers will give us half of
      // that (by enabling breadth-first search from root), but blindly
      // resolving dependencies may still get us stuck.  This is why we
      // resolve dependencies in the reverse order of how they were added.
      for(int i = g_trader[t].dependent_count; i--;)
         visit_queue[queue_tail++] = g_trader[t].dependents[i];
   }
   return -1;
}
