// Generate SVG with trader positions using trade.c

#include<stdio.h>
#include<stdlib.h>
#include"trade.h"

#define RADIUS  (TRADER_SEPARATION / 2)

int main(int argc, char **argv)
{
   if( argc != 3 )
      return printf("%s {size} {seed}\n", *argv);
   const int size = atoi(argv[1]);
   srand(atoi(argv[2]));
   InitTraders(size);

   // Determine graph area.
   int min_x = -RADIUS;
   int min_y = -RADIUS;
   int max_x = RADIUS;
   int max_y = RADIUS;
   for(int i = 0; i < size; i++)
   {
      const int x = g_trader[i].x;
      const int y = g_trader[i].y;
      if( min_x > x - RADIUS ) min_x = x - RADIUS;
      if( min_y > y - RADIUS ) min_y = y - RADIUS;
      if( max_x < x + RADIUS ) max_x = x + RADIUS;
      if( max_y < y + RADIUS ) max_y = y + RADIUS;
   }

   // Add some margin.
   min_x -= RADIUS / 10;
   min_y -= RADIUS / 10;
   max_x += RADIUS / 10;
   max_y += RADIUS / 10;

   // Output header.
   printf("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
          "<svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" "
          "xmlns=\"http://www.w3.org/2000/svg\" "
          "xmlns:svg=\"http://www.w3.org/2000/svg\">\n",
          max_x - min_x, max_y - min_y,
          max_x - min_x, max_y - min_y);

   // Output circle at origin.
   printf("<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
          "style=\"fill:none;stroke:#ff0000;stroke-width:1\" />\n",
          -min_x,
          -min_y,
          RADIUS);

   // Output circles for each trader.
   for(int i = 0; i < size; i++)
   {
      printf("<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
             "style=\"fill:none;stroke:#00ff00;stroke-width:1\" />\n",
             g_trader[i].x - min_x,
             g_trader[i].y - min_y,
             RADIUS);
   }

   // Output footer.
   puts("</svg>");
   return 0;
}
