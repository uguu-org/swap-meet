#ifndef ITEM_H_
#define ITEM_H_

#include"trade.h"

// A convenient pair type.
typedef struct { int x, y; } XY;

// States for a single item held by a player or trader.
typedef struct
{
   // Upper left corner of item position in world coordinates.
   XY current;

   // Upper left corner of desired item position in world coordinates.
   XY target;

   // ID of item being held.
   int id;

   // If item is currently being held by a trader, this is the index
   // of the trader that holds it.  If it's not held by a trader then
   // this field is -1.
   int trader_id;
} Item;

// List of items.  Basically what we do when we don't have std::vector.
typedef struct
{
   Item item[MAX_ITEM_COUNT];
   int item_count;
} ItemList;

// Move item from one list to another.
//
// Returns pointer to the newly added item.
Item *MoveItem(ItemList *from, int from_index, ItemList *to);

// Rotate item list such that first "head_count" entries are moved to the end.
void RotateListHead(ItemList *items, int head_count);

// Rotate item list such that last "tail_count" entries are moved to the front.
void RotateListTail(ItemList *items, int tail_count);

// Converge all item positions to target positions.
void UpdateItemPositions(ItemList *items);

#endif  // ITEM_H_
