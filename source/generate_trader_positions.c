// Generate SVG with trader positions using trade.c

#include<stdio.h>
#include<stdlib.h>
#include"trade.h"

#define RADIUS          (TRADER_SEPARATION / 2)

int main(int argc, char **argv)
{
   if( argc != 3 )
      return printf("%s {size} {seed}\n", *argv);
   const int size = atoi(argv[1]);
   srand(atoi(argv[2]));
   InitTraders(size);
   XY landmarks[LANDMARK_VARIATIONS];
   GetLandmarkPositions(size, landmarks);

   // Determine graph area.
   XY min, max;
   GetTraderRange(size, &min, &max);
   for(int i = 0; i < LANDMARK_VARIATIONS; i++)
   {
      if( min.x > landmarks[i].x ) min.x = landmarks[i].x;
      if( min.y > landmarks[i].y ) min.y = landmarks[i].y;
      if( max.x < landmarks[i].x ) max.x = landmarks[i].x;
      if( max.y < landmarks[i].y ) max.y = landmarks[i].y;
   }

   // Add some margin.
   min.x -= RADIUS * 11 / 10;
   min.y -= RADIUS * 11 / 10;
   max.x += RADIUS * 11 / 10;
   max.y += RADIUS * 11 / 10;

   // Output header.
   printf("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
          "<svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" "
          "xmlns=\"http://www.w3.org/2000/svg\" "
          "xmlns:svg=\"http://www.w3.org/2000/svg\">\n",
          max.x - min.x, max.y - min.y,
          max.x - min.x, max.y - min.y);

   // Output circle at origin.
   printf("<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
          "style=\"fill:none;stroke:#ff0000;stroke-width:1\" />\n",
          -min.x,
          -min.y,
          RADIUS);

   for(int i = 0; i < size; i++)
   {
      // Output circles for each trader.
      printf("<circle cx=\"%d\" cy=\"%d\" r=\"%d\" "
             "style=\"fill:none;stroke:#00ff00;stroke-width:1\" />\n",
             g_trader[i].position.x - min.x,
             g_trader[i].position.y - min.y,
             RADIUS);

      // Output trader signs.
      for(int j = 0; j < g_trader[i].sign_count; j++)
      {
         if( g_trader[i].sign[j].x < g_trader[i].position.x )
         {
            // Sign pointing right.
            printf("<path style=\"fill:none;stroke:#0000ff;stroke-width:1\""
                   " d=\"M %d,%d l 32,0 16,16 -16,16 -32,0 z\" />\n",
                   g_trader[i].sign[j].x - min.x - 16,
                   g_trader[i].sign[j].y - min.y - 16);
         }
         else if( g_trader[i].sign[j].x > g_trader[i].position.x )
         {
            // Sign pointing left.
            printf("<path style=\"fill:none;stroke:#0000ff;stroke-width:1\""
                   " d=\"M %d,%d l 32,0 0,32 -32,0 -16,-16 z\" />\n",
                   g_trader[i].sign[j].x - min.x - 16,
                   g_trader[i].sign[j].y - min.y - 16);
         }
         else if( g_trader[i].sign[j].y < g_trader[i].position.y )
         {
            // Sign pointing down.
            printf("<path style=\"fill:none;stroke:#0000ff;stroke-width:1\""
                   " d=\"M %d,%d l 32,0 0,32 -16,16 -16,-16 z\" />\n",
                   g_trader[i].sign[j].x - min.x - 16,
                   g_trader[i].sign[j].y - min.y - 16);
         }
         else
         {
            // Sign pointing up.
            printf("<path style=\"fill:none;stroke:#0000ff;stroke-width:1\""
                   " d=\"M %d,%d l 16,-16 16,16 0,32 -32,0 z\" />\n",
                   g_trader[i].sign[j].x - min.x - 16,
                   g_trader[i].sign[j].y - min.y - 16);
         }
      }
   }

   // Output landmark positions.
   for(int i = 0; i < LANDMARK_VARIATIONS; i++)
   {
      printf("<rect style=\"fill:none;stroke:#ff00ff;stroke-width:1\""
             " x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" />\n",
             landmarks[i].x - LANDMARK_RADIUS - min.x,
             landmarks[i].y - LANDMARK_RADIUS - min.y,
             LANDMARK_RADIUS * 2,
             LANDMARK_RADIUS * 2);
   }

   // Output footer.
   puts("</svg>");
   return 0;
}
