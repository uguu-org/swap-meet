#ifndef TRADE_H_
#define TRADE_H_

#include<stdint.h>
#include"common.h"

// Maximum number of items that could be requested or offered by a
// single trader.
//
// This number can't be greater than 4 because we don't have enough
// space on the trading signs to place more.  And actually, setting
// this beyond 3 makes the game more difficult because player needs to
// remember more things.  As such, the hard limit here is 4, and we
// limit the request/offer sizes to 3 when game is not in hard mode.
#define MAX_TRANSACTION_SIZE  4

// Number of item variations, not including goal item.
#define MAX_ITEM_ID           21

// The one unique item that player wants to get.  All other items may
// contain duplicates.
#define GOAL_ITEM_ID          (MAX_ITEM_ID + 1)

// The item(s) that player started with.
#define STARTING_ITEM_ID      1

// Maximum number of traders.
//
// It's possible to set this higher, about 32, but the game is already
// fairly difficult at 20.
#define MAX_TRADER_COUNT      20

// Maximum number of movable items that can exist in the game.
//
// We need one count for each item offered by traders, and we assume
// player has just many starting items to fulfill all requests, hence
// the number of traders time double the transaction size limit.
#define MAX_ITEM_COUNT        (MAX_TRADER_COUNT * MAX_TRANSACTION_SIZE * 2)

// Minimum distance between traders in pixels.
#define TRADER_SEPARATION     280

// Size of sign area in pixels.
#define SIGN_RADIUS           40

// Maximum number of direction signs per trader.
#define MAX_SIGN_COUNT        4

// Number of landmarks to place.  This is the number columns inside
// landmarks image table.
#define LANDMARK_VARIATIONS   14

// Half size of landmarks for placement purposes.  Doubling this
// number results in a product that is smaller than the tile sizes,
// since we want a bit of overlap between landmarks.
#define LANDMARK_RADIUS       80

// Definitions for a single trader.
typedef struct
{
   // List of items offered for trading.  Items may contain duplicates.
   int offers[MAX_TRANSACTION_SIZE];

   // List of items requested by trader.  Items may contain duplicates.
   // Transaction is allowed if and only if player has all the requested items.
   int requests[MAX_TRANSACTION_SIZE];

   // List sizes.
   int offer_count, request_count, dependent_count;

   // Trading state.
   // 0 = player has not traded with this trader yet, so this trader has
   //     the items listed under "offers" and is waiting for "requests".
   // 1 = player has traded with this trader, so this trader has the items
   //     listed under "requests" and will exchange for "offers".
   int traded;

   // World coordinates.
   XY position;

   // Direction sign positions.
   XY sign[MAX_SIGN_COUNT];
   int sign_count;
} Trader;

// Search key for FindNextTrader.
//
// This struct is very similar to TradeNode, but interpretation of the
// fields are subtly different.
typedef struct
{
   // Unordered set of items held by the player, encoded as an array
   // of item counts indexed by item ID.
   uint8_t item_count[GOAL_ITEM_ID + 1];

   // If a trade was performed at some place, the corresponding entry
   // will be 1.
   uint8_t traded[MAX_TRADER_COUNT];

   // Index of the trader where we made the previous trade.
   int trader_index;
} TradeState;

// All trade offers, statically allocated.
extern Trader g_trader[MAX_TRADER_COUNT];

// Minimum number of starting items needed to reach goal.
extern int g_minimum_starting_items;

// Initialize trading graph and trader positions, overwriting g_grader array.
void InitTraders(int trader_count);

// Return index of the next trader that the player should attempt to trade with.
int FindNextTrader(int trader_count, const TradeState *state);

// Get range of coordinates occupied by traders.
//
// Note that returned range only accounts for the center positions of
// each trader, caller needs to add some radius to account for on-screen size.
//
// Also note that signs are excluded from range check.
void GetTraderRange(int trader_count, XY *min, XY *max);

// Update list of landmark positions.
void GetLandmarkPositions(int trader_count, XY *landmark_positions);

#endif  // TRADE_H_
