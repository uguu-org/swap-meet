#include"item.h"
#include<string.h>
#include"common.h"

// Move item from one list to another.
Item *MoveItem(ItemList *from, int from_index, ItemList *to)
{
   assert(from->item_count > 0);
   assert(from_index < from->item_count);
   assert(to->item_count < MAX_ITEM_COUNT);

   Item *append_position = &(to->item[to->item_count++]);
   memcpy(append_position, &(from->item[from_index]), sizeof(Item));

   from->item_count--;
   if( from->item_count > from_index )
   {
      memmove(&(from->item[from_index]),
              &(from->item[from_index + 1]),
              (from->item_count - from_index) * sizeof(Item));
   }
   return append_position;
}

// Rotate item list such that first "head_count" entries are moved to the end.
void RotateListHead(ItemList *items, int head_count)
{
   assert(head_count > 0);
   assert(head_count <= 4);
   assert(head_count <= items->item_count);
   if( items->item_count <= head_count )
      return;

   Item buffer[4];
   const int split = items->item_count - head_count;
   memcpy(buffer,
          &(items->item[0]),
          head_count * sizeof(Item));
   memmove(&(items->item[0]),
           &(items->item[head_count]),
           split * sizeof(Item));
   memcpy(&(items->item[split]),
          buffer,
          head_count * sizeof(Item));
}

// Rotate item list such that last "tail_count" entries are moved to the front.
void RotateListTail(ItemList *items, int tail_count)
{
   assert(tail_count > 0);
   assert(tail_count <= 4);
   assert(tail_count <= items->item_count);
   if( items->item_count <= tail_count )
      return;

   Item buffer[4];
   const int split = items->item_count - tail_count;
   memcpy(buffer,
          &(items->item[split]),
          tail_count * sizeof(Item));
   memmove(&(items->item[tail_count]),
           &(items->item[0]),
           split * sizeof(Item));
   memcpy(&(items->item[0]),
          buffer,
          tail_count * sizeof(Item));
}

// Converge a single coordinate value.
static int Converge(int current, int target)
{
   if( abs(current - target) < 2 )
      return target;
   return (current * 3 + target) / 4;
}

// Converge all item positions to target positions.
void UpdateItemPositions(ItemList *items)
{
   Item *o = items->item;
   for(int i = 0; i < items->item_count; i++, o++)
   {
      o->current.x = Converge(o->current.x, o->target.x);
      o->current.y = Converge(o->current.y, o->target.y);
   }
}
