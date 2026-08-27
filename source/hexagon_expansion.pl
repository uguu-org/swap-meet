#!/usr/bin/perl -w
# Write a SVG to stdout, with the curve points containing vertices arranged
# on a hexagonal path, like this:
#
#       31  32  33  34
#     30  15  16  17  35
#   29  14   5   6  18  36
# 28  13   4       1   7  19
#   27  12   3   2   8  20
#     26  11  10   9  21
#       25  24  23  22

use strict;
use constant EDGE_SIZE => 20;

# Movement offsets.
my @offset =
(
   [-EDGE_SIZE / 2, int(EDGE_SIZE * 0.5 * (3 ** 0.5))],
   [-EDGE_SIZE, 0],
   [-EDGE_SIZE / 2, -int(EDGE_SIZE * 0.5 * (3 ** 0.5))],
   [EDGE_SIZE / 2, -int(EDGE_SIZE * 0.5 * (3 ** 0.5))],
   [EDGE_SIZE, 0],
   [EDGE_SIZE / 2, int(EDGE_SIZE * 0.5 * (3 ** 0.5))],
);

# Collect path points.
my $x = EDGE_SIZE / 2;
my $y = 0;
my $size = 1;
my $edge_step = 0;
my $side_step = 0;

my @points = ();
for(my $i = 0; $i < 100; $i++)
{
   push @points, [$x, $y];

   $x += $offset[$side_step][0];
   $y += $offset[$side_step][1];
   $edge_step++;
   if( $edge_step == $size )
   {
      $edge_step = 0;
      $side_step++;
      if( $side_step == 6 )
      {
         $side_step = 0;
         $size++;
         if( $i > 99 )
         {
            last;
         }
         $x += EDGE_SIZE;
      }
   }
}

# Collect path range.
my $min_x = 0;
my $min_y = 0;
my $max_x = 0;
my $max_y = 0;
foreach my $p (@points)
{
   my ($px, $py) = @$p;
   if( $min_x > $px ) { $min_x = $px; }
   if( $min_y > $py ) { $min_y = $py; }
   if( $max_x < $px ) { $max_x = $px; }
   if( $max_y < $py ) { $max_y = $py; }
}
$min_x -= EDGE_SIZE;
$min_y -= EDGE_SIZE;
$max_x += EDGE_SIZE;
$max_y += EDGE_SIZE;

my $width = $max_x - $min_x;
my $height = $max_y - $min_y;

# Build path points.
my $path = undef;
foreach my $p (@points)
{
   my ($px, $py) = @$p;
   $px -= $min_x;
   $py -= $min_y;
   if( defined($path) )
   {
      $path .= " $px,$py";
   }
   else
   {
      $path = "M $px,$py L";
   }
}

# Output plot.
print <<"EOT";
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg
   width="$width"
   height="$height"
   viewBox="0 0 $width $height"
   xmlns:xlink="http://www.w3.org/1999/xlink"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
<path style="fill:none;stroke:#000000;stroke-width:1" d="$path" />
</svg>
EOT
