#!/usr/bin/perl -w
# Generate offsets for the random pile of items behind each exchange sign.

use strict;
use constant PI => 3.14159265358979323846264338327950288419716939937510;

# See TRADER_SEPARATION in trade.h.
use constant TRADER_RADIUS => 150;

# Number of items in pile.
use constant PILE_SIZE => 40;

# Item size in pixels.
use constant ITEM_SIZE => 48;

# Size of trade sign in pixels.
use constant SWAP_SIGN_WIDTH => 256;
use constant SWAP_SIGN_HEIGHT => 112;

# Lowest Y coordinate of item pile.
use constant FLOOR_Y => SWAP_SIGN_HEIGHT / 2;

# First 4 positions are reserved for the offered items.
print "const int kItemPile[", PILE_SIZE, "][2] =\n",
      "{\n",
      "\t{4, ", FLOOR_Y, "},\n",                            # Center right.
      "\t{", -ITEM_SIZE - 4, ", ", FLOOR_Y, "},\n",         # Center left.
      "\t{", ITEM_SIZE + 12, ", ", FLOOR_Y, "},\n",         # Right.
      "\t{", -ITEM_SIZE * 2 - 12, ", ", FLOOR_Y, "},\n";    # Left.

# Place the remaining items deterministically but randomly.
srand(1);

my @positions = ();
for(my $i = 4; $i < PILE_SIZE; $i++)
{
   my ($x, $y);
   for(my $has_overlap = 1; $has_overlap;)
   {
      my $r = rand(TRADER_RADIUS);
      my $a = rand(PI);
      $x = $r * cos($a) - ITEM_SIZE / 2;
      $y = FLOOR_Y - ITEM_SIZE * 0.5 - $r * sin($a);

      $has_overlap = 0;
      foreach my $p (@positions)
      {
         my ($px, $py) = @$p;
         my $dx = $x - $px;
         my $dy = $y - $py;

         if( $dx * $dx + $dy * $dy < (ITEM_SIZE / 2) * (ITEM_SIZE / 2) )
         {
            $has_overlap = 1;
            last;
         }
      }
   }

   push @positions, [$x, $y];
   print "\t{", int($x), ", ", int($y), "},\n";
}

print "};\n";
