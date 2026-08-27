/* Convert PNG to black and white.

   Usage:

      ./tile_dither {input.png} {output.png} [tile_width] [tile_height]

   Use "-" for input or output to read/write from stdin/stdout.

   Given a grayscale (8bit) plus alpha (8bit) PNG, output a black and
   white (1bit) plus transparency (1bit) PNG, with ordered-dithering.

   This tool differs from bayer_dither.c in that it uses blue noise
   patterns for the dither array (as opposed to bayer pattern).  The
   blue noise are generated with generate_symmetric_dither_array.cc to
   include rotational symmetry.  If tile_width and tile_height are
   specified, this tool will try to center the dither patterns around
   each tile.
*/

#include<png.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#ifdef _WIN32
   #include<fcntl.h>
   #include<io.h>
#endif

#include"build/dither_array_pixel.c"
#include"build/dither_array_alpha.c"

static unsigned char Dither(int w, int h, int x, int y, int value, int alpha)
{
   /* Zero is always zero.  This makes a difference for the few pixels
      in the dither array that are at 255, where adding dither array
      bias to any pixel causes them to go over the threshold.          */
   if( value == 0 )
      return 0;

   /* Convert from screen coordinate to tile coordinate, with origin
      at tile center.                                                */
   x = (x % w) - w / 2;
   y = (y % h) - h / 2;

   /* Convert from tile coordinates to matrix coordinates, making sure that
      the center of the tile maps to the center of the pattern matrix.      */
   x = (x + DITHER_ARRAY_SIZE * 3 / 2) % DITHER_ARRAY_SIZE;
   y = (y + DITHER_ARRAY_SIZE * 3 / 2) % DITHER_ARRAY_SIZE;

   value += (alpha ? alpha_pattern[y][x] : pixel_pattern[y][x]) - 127;
   return value > 127 ? 255 : 0;
}

int main(int argc, char **argv)
{
   png_image image;
   png_bytep pixels, p;
   int x, y, tile_width, tile_height;

   if( argc != 3 && argc != 5 )
   {
      return printf("%s {input.png} {output.png} [tile_width] [tile_height]\n",
                    *argv);
   }
   if( strcmp(argv[2], "-") == 0 && isatty(STDOUT_FILENO) )
   {
      fputs("Not writing output to stdout because it's a tty\n", stderr);
      return 1;
   }
   #ifdef _WIN32
      setmode(STDIN_FILENO, O_BINARY);
      setmode(STDOUT_FILENO, O_BINARY);
   #endif

   if( argc == 5 )
   {
      tile_width = atoi(argv[3]);
      tile_height = atoi(argv[4]);
      if( tile_width < 1 || tile_height < 1 )
      {
         printf("Invalid tile size: %s %s\n", argv[3], argv[4]);
         return 1;
      }
   }
   else
   {
      tile_width = tile_height = DITHER_ARRAY_SIZE;
   }

   /* Load input. */
   memset(&image, 0, sizeof(image));
   image.version = PNG_IMAGE_VERSION;
   if( strcmp(argv[1], "-") == 0 )
   {
      if( !png_image_begin_read_from_stdio(&image, stdin) )
      {
         puts("Error reading from stdin");
         return 1;
      }
   }
   else
   {
      if( !png_image_begin_read_from_file(&image, argv[1]) )
         return printf("Error reading %s\n", argv[1]);
   }
   image.format = PNG_FORMAT_GA;
   pixels = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
   if( pixels == NULL )
   {
      puts("Out of memory");
      return 1;
   }
   if( !png_image_finish_read(&image, NULL, pixels, 0, NULL) )
   {
      free(pixels);
      return printf("Error loading %s\n", argv[1]);
   }

   /* Dither pixels. */
   p = pixels;
   for(y = 0; y < (int)image.height; y++)
   {
      for(x = 0; x < (int)image.width; x++, p += 2)
      {
         /* Dither color and alpha independently. */
         *p = Dither(tile_width, tile_height, x, y, (int)*p, 0);
         *(p + 1) = Dither(tile_width, tile_height, x, y, (int)*(p + 1), 1);

         /* Set color part to zero if alpha is zero. */
         if( *(p + 1) == 0 )
            *p = 0;
      }
   }

   /* Write output.  Here we set the flags to optimize for encoding speed
      rather than output size so that we can iterate faster.  This is fine
      since the output of this tool are intermediate files that are used
      only in the build process, and are not the final PNGs that will be
      committed.                                                           */
   image.flags |= PNG_IMAGE_FLAG_FAST;
   x = 0;
   if( strcmp(argv[2], "-") == 0 )
   {
      if( !png_image_write_to_stdio(&image, stdout, 0, pixels, 0, NULL) )
      {
         fputs("Error writing to stdout\n", stderr);
         x = 1;
      }
   }
   else
   {
      if( !png_image_write_to_file(&image, argv[2], 0, pixels, 0, NULL) )
      {
         printf("Error writing %s\n", argv[2]);
         x = 1;
      }
   }
   free(pixels);
   return x;
}
