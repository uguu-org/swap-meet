// Runs a quick test to verify the generated maps are solvable.

#include"trade.h"
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"common.h"

#define HARD_SIZE    MAX_TRADER_COUNT
#define EASY_SIZE    (HARD_SIZE / 2)

// Write trader state to stdout.
static void DumpTrader(int trader_index)
{
   printf("trader[%d]: traded=%d, {",
          trader_index, g_trader[trader_index].traded);
   for(int i = 0; i < g_trader[trader_index].request_count; i++)
      printf(i > 0 ? ", %d" : "%d", g_trader[trader_index].requests[i]);
   printf(g_trader[trader_index].traded ? "} <- {" : "} -> {");
   for(int i = 0; i < g_trader[trader_index].offer_count; i++)
      printf(i > 0 ? ", %d" : "%d", g_trader[trader_index].offers[i]);
   putchar('}');
}

// Write all trader states to stdout.
static void DumpAllTraders(int trader_count)
{
   for(int i = 0; i < trader_count; i++)
   {
      DumpTrader(i);
      putchar('\n');
   }
}

// Add items to list.
static void AddItems(TradeState *state,
                     const int *items, const int item_count)
{
   for(int i = 0; i < item_count; i++)
   {
      const int item_id = items[i];
      assert(state->item_count[item_id] <
             MAX_TRADER_COUNT * MAX_TRANSACTION_SIZE);
      state->item_count[item_id]++;
   }
}

// Remove items from list.
static void RemoveItems(TradeState *state,
                        const int *items, const int item_count)
{
   for(int i = 0; i < item_count; i++)
   {
      const int item_id = items[i];
      assert(state->item_count[item_id] > 0);
      state->item_count[item_id]--;
   }
}

// Disregard the suggested hints to try to get off the known solution path.
static void GetDistracted(int trader_count, TradeState *state)
{
   for(int step = 0; step < trader_count; step++)
   {
      const int trader_index = FindNextTrader(trader_count, state);

      // Try trading with a random trader.
      int selected_trader = 0;
      for(int i = 0; i < 10; i++)
      {
         // Select a random trader, skipping over 0 since that
         // would make us reach the goal.
         selected_trader = RAND_RANGE(1, trader_count - 1);

         // Skip over the trader suggested by the hint system.
         if( selected_trader == trader_index )
         {
            selected_trader = 0;
            continue;
         }

         // Skip over traders that we have already traded with.  This
         // is so that we will not undo any diversions we have caused.
         if( g_trader[selected_trader].traded )
         {
            selected_trader = 0;
            continue;
         }

         // Skip over traders where the trade is not eligible.
         int requirements[MAX_ITEM_ID + 1];
         memset(requirements, 0, sizeof(requirements));
         for(int j = 0; j < g_trader[selected_trader].request_count; j++)
            requirements[g_trader[selected_trader].requests[j]]++;

         for(int j = STARTING_ITEM_ID; j <= MAX_ITEM_ID; j++)
         {
            if( state->item_count[j] < requirements[j] )
            {
               selected_trader = 0;
               break;
            }
         }
         if( selected_trader != 0 )
            break;
      }

      // Stop when we couldn't find an eligible trader.
      if( selected_trader == 0 )
         return;

      // Perform trade.
      Trader *t = &(g_trader[selected_trader]);
      assert(t->traded == 0);
      RemoveItems(state, t->requests, t->request_count);
      AddItems(state, t->offers, t->offer_count);
      t->traded ^= 1;
      state->traded[selected_trader] ^= 1;
   }
}

// Generate a map and run solver repeatedly.  Returns number of trades needed
// to reach goal.
static int SolveSeed(int seed, int trader_count, int test_recovery, int verbose)
{
   srand(seed);
   InitTraders(trader_count);
   if( verbose )
      DumpAllTraders(trader_count);

   TradeState state;
   memset(&state, 0, sizeof(state));
   state.item_count[STARTING_ITEM_ID] = g_minimum_starting_items;

   if( test_recovery )
   {
      GetDistracted(trader_count, &state);
      if( verbose )
      {
         puts("Distracted state:");
         DumpAllTraders(trader_count);
         printf("Items:");
         for(int i = STARTING_ITEM_ID; i <= MAX_ITEM_ID; i++)
         {
            if( state.item_count[i] == 0 )
               continue;
            for(int j = 0; j < (int)(state.item_count[i]); j++)
               printf(" %d", i);
         }
         putchar('\n');
      }
   }

   int trades = 0;
   for(; trades < trader_count * 3; trades++)
   {
      const int trader_index = FindNextTrader(trader_count, &state);
      if( trader_index < 0 )
         break;

      if( verbose )
      {
         printf("Step %d: trade with ", trades);
         DumpTrader(trader_index);
      }
      if( trader_index == 0 )
      {
         if( verbose )
            puts(", reached goal");
         return trades;
      }

      Trader *t = &(g_trader[trader_index]);
      if( t->traded )
      {
         // Undo previous trade.
         RemoveItems(&state, t->offers, t->offer_count);
         AddItems(&state, t->requests, t->request_count);
      }
      else
      {
         // Perform trade.
         RemoveItems(&state, t->requests, t->request_count);
         AddItems(&state, t->offers, t->offer_count);
      }

      t->traded ^= 1;
      state.traded[trader_index] ^= 1;

      if( verbose )
      {
         printf(", results:");
         for(int i = STARTING_ITEM_ID; i <= MAX_ITEM_ID; i++)
         {
            if( state.item_count[i] == 0 )
               continue;
            for(int j = 0; j < (int)(state.item_count[i]); j++)
               printf(" %d", i);
         }
         printf(", traded:");
         for(int i = 0; i < trader_count; i++)
         {
            if( state.traded[i] )
               printf(" %d", i);
            else
               printf(" -");
         }
         putchar('\n');
      }
   }

   printf("Unsolvable: seed=%d, size=%d, progress=%d\n",
          seed, trader_count, trades);
   return -1;
}

// Test different seeds for a quick test.
static void ShortTest(int trader_count)
{
   for(int seed = 1; seed <= 1000; seed++)
   {
      assert(SolveSeed(seed, trader_count, 0, 0) >= 0);
      assert(SolveSeed(seed, trader_count, 1, 0) >= 0);
   }
}

// Test a range of seeds and print how many trades are needed to reach solution.
static void Benchmark(
   int trader_count, int seed_start, int seed_end, int test_distractions)
{
   // Solve the seed in verbose mode if we specify exactly one seed.
   if( seed_start == seed_end )
   {
      SolveSeed(seed_start, trader_count, test_distractions, 1);
      return;
   }

   int sum = 0;
   for(int i = seed_start; i <= seed_end; i++)
   {
      const int t = SolveSeed(i, trader_count, test_distractions, 0);
      printf("seed=%d, size=%d: %d trades\n", i, trader_count, t);
      sum += t;
   }
   printf("Average trades = %.2f\n", (double)sum / (seed_end - seed_start + 1));
}

int main(int argc, char **argv)
{
   if( argc == 1 )
   {
      ShortTest(EASY_SIZE);
      ShortTest(HARD_SIZE);
      return 0;
   }

   assert(argc > 1);
   const int size = atoi(argv[1]);
   if( size > 2 && size <= MAX_TRADER_COUNT )
   {
      if( argc == 4 )
      {
         Benchmark(size, atoi(argv[2]), atoi(argv[3]), 0);
         return 0;
      }
      else if( argc == 5 )
      {
         Benchmark(size, atoi(argv[2]), atoi(argv[3]), 1);
         return 0;
      }
   }

   printf("%s {size} {seed_start} {seed_end} [test_distractions]\n", *argv);
   return 1;
}
