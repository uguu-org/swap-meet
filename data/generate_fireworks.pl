#!/usr/bin/perl -w
# Generate SVG for rendering firework frames.

use strict;
use Digest::MD5 qw(md5);

use constant PI => 3.14159265358979323846264338327950288419716939937510;

# Animate this many particles in each firework.
use constant PARTICLE_COUNT => 777;

# Number of frames to draw at full opacity.
use constant LIVE_FRAMES => 20;

# Number of frames to fade toward zero opacity.
use constant FADE_FRAMES => 10;

# Total number of animation frames.
use constant TOTAL_FRAMES => (LIVE_FRAMES + FADE_FRAMES);

# Firework radius.
use constant RADIUS => 50;

# Size of each output image tile in pixels.
use constant TILE_SIZE => 100;


# Random number generator seed.  I tried a few, and 12 seemed to have
# the best shape with the fewest clumps.
my $seed = 12;

# Generate a random number between 0 and max by hashing.
#
# The hashing scheme means the generated random numbers should match
# on different machines.  We do this as opposed to just calling "rand",
# because it would be a shame to get different generated images due to
# changes in rand(), after having picked a specific seed.
sub Rand($)
{
   my ($max) = @_;

   my @byte = unpack 'C*', md5(pack 'V', $seed++);
   my $r = ($byte[0] << 24) + ($byte[1] << 16) + ($byte[2] << 8) + $byte[3];
   return $r * $max / (1.0 * 0xffffffff);
}

# Compute coordinates for a single particle.
sub point($$$)
{
   my ($final_x, $final_y, $frame) = @_;

   # Start out fast and decelerate toward end position.
   my $r = $frame / TOTAL_FRAMES - 1;
   $r = 1 - ($r * $r);
   my $x = $final_x * $r;
   my $y = $final_y * $r;
   $x > -TILE_SIZE / 2 or die;
   $y > -TILE_SIZE / 2 or die;
   $x < TILE_SIZE / 2 or die;
   $y < TILE_SIZE / 2 or die;
   return ($x, $y);
}


# Output header.
my $width = TILE_SIZE * TOTAL_FRAMES;
my $height = TILE_SIZE;
print <<"EOT";
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg
   width="$width" height="$height"
   viewBox="0 0 $width $height"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
EOT

# Generate final locations for each particle.
my @particles = ();
for(my $i = 0; $i < PARTICLE_COUNT; $i++)
{
   # Generate a random position using 3D polar coordinates.
   #
   # It's purely random coordinates without any special heuristics to
   # remove clumps or anything, but the polar coordinates will get us
   # some circular distribution that looks reasonable.
   #
   # I actually did try to be more clever and added heuristics to
   # remove clumps by maximizing the distances between generated
   # points, and that actually ended up creating unnatural clumps in
   # itself, maybe because the points would converge toward certain
   # corners?  Anyways the current random scheme appears to produce
   # the best looking results.
   my $r = RADIUS * cos(Rand(0.5 * PI));
   my $a = Rand(2 * PI);
   push @particles, [$r * cos($a), $r * sin($a)];
}

# Draw lines for live particles.
for(my $f = 0; $f < TOTAL_FRAMES; $f++)
{
   for(my $i = 0; $i < PARTICLE_COUNT; $i++)
   {
      my ($x0, $y0) = point($particles[$i][0], $particles[$i][1], $f);
      my ($x1, $y1) = point($particles[$i][0], $particles[$i][1], $f + 1);

      # Alternate between black and white particles.
      my $color = ($i & 1) ? "#ffffff" : "#000000";

      $x0 += TILE_SIZE / 2 + TILE_SIZE * $f;
      $y0 += TILE_SIZE / 2;
      $x1 += TILE_SIZE / 2 + TILE_SIZE * $f;
      $y1 += TILE_SIZE / 2;
      my $style = "fill:none;stroke:$color;stroke-linecap:round";
      if( $f >= LIVE_FRAMES )
      {
         $style .= ";stroke-opacity:" . (1 - ($f - LIVE_FRAMES) / FADE_FRAMES);
      }
      print <<"EOT";
<path id="F${f}_${i}" style="$style" d="M $x0,$y0 $x1,$y1" />
EOT
   }
}

# Output footer.
print "</svg>\n";
