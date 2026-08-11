// Generate trading graph using trade.c

#include<stdio.h>
#include<stdlib.h>
#include"trade.h"

static int HasEdge(int a, int b)
{
   for(int i = 0; i < g_trader[a].request_count; i++)
   {
      for(int j = 0; j < g_trader[b].offer_count; j++)
      {
         if( g_trader[a].requests[i] == g_trader[b].offers[j] )
            return 1;
      }
   }
   return 0;
}

int main(int argc, char **argv)
{
   if( argc != 3 )
      return printf("%s {size} {seed}\n", *argv);
   const int size = atoi(argv[1]);
   srand(atoi(argv[2]));
   InitTraders(size);

   puts("digraph G {\nrankdir = BT");
   for(int i = 0; i < size; i++)
   {
      printf("n%d [label=\"offers:", i);
      for(int j = 0; j < g_trader[i].offer_count; j++)
         printf(" %d", g_trader[i].offers[j]);
      printf("\\nrequests:");
      for(int j = 0; j < g_trader[i].request_count; j++)
         printf(" %d", g_trader[i].requests[j]);
      printf("\"]\n");
   }
   for(int i = 0; i < size; i++)
   {
      for(int j = 0; j < size; j++)
      {
         if( HasEdge(i, j) )
            printf("n%d -> n%d\n", j, i);
      }
   }

   puts("}");
   return 0;
}
