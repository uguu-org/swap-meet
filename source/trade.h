#ifndef TRADE_H_
#define TRADE_H_

#include<stdint.h>

// Maximum number of items that could be requested or offered by a
// single trader.
//
// Because the item variation is not all that large, we make this limit
// relatively small.  This increases the number of traders that need
// be visited to cover the goal requirements.
//
// If we had about ~32 items, setting this to 4 would be more reasonable.
#define MAX_TRANSACTION_SIZE  3

// Number of item variations, not including goal item.
#define MAX_ITEM_ID           15

// The one unique item that player wants to get.  All other items may
// contain duplicates.
#define GOAL_ITEM_ID          (MAX_ITEM_ID + 1)

// The item(s) that player started with.
#define STARTING_ITEM_ID      1

// Maximum number of traders.
#define MAX_TRADER_COUNT      32

// Maximum number of movable items that can exist in the game.
//
// We need one count for each item offered by traders, and we assume
// player has just many starting items to fulfill all requests, hence
// the number of traders time double the transaction size limit.
#define MAX_ITEM_COUNT        (MAX_TRADER_COUNT * MAX_TRANSACTION_SIZE * 2)

// Distance between traders in pixels.
#define TRADER_SEPARATION     280

// Definitions for a single trader.
typedef struct
{
   // List of items offered for trading.  Items may contain duplicates.
   int offers[MAX_TRANSACTION_SIZE];

   // List of items requested by trader.  Items may contain duplicates.
   // Transaction is allowed if and only if player has all the requested items.
   int requests[MAX_TRANSACTION_SIZE];

   // List of traders that can fulfill a request for the current trader.
   // This is used for hint display.
   //
   // This allows the player to find a trader to visit to reach the goal,
   // but it doesn't guarantee that the solution is optimal.
   int dependents[MAX_TRANSACTION_SIZE];

   // List sizes.
   int offer_count, request_count, dependent_count;

   // Trading state.
   // 0 = player has not traded with this trader yet, so this trader has
   //     the items listed under "offers" and is waiting for "requests".
   // 1 = player has traded with this trader, so this trader has the items
   //     listed under "requests" and will exchange for "offers".
   int traded;

   // World coordinates.
   int x, y;
} Trader;

// All trade offers, statically allocated.
extern Trader g_trader[MAX_TRADER_COUNT];

// Minimum number of starting items needed to reach goal.
extern int g_minimum_starting_items;

// Initialize trading graph and trader positions, overwriting g_grader array.
void InitTraders(int trader_count);

// Given a list of items that the player already has, return index of
// the next trader that the player should attempt to trade with.
// Returns -1 if no good trader is found.
int FindNextTrader(int *player_items, int player_item_count);

#endif  // TRADE_H_
