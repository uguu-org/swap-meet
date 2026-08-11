#!/usr/bin/perl -w
# Generate rotated clones for all sprite groups.
#
# ./rotate_sprites.pl {rotation_steps} {input.svg} > {output.svg}
#
# Sprite tiles are assumed to be square size.
#
# Page width must be the same as the sprite tile width.

use strict;
use XML::LibXML;

use constant PI => 3.14159265358979323846264338327950288419716939937510;

# Label prefix for groups to be rotated.
use constant INPUT_PREFIX => "rotate input";

# Amount of rotation to perform per step, in degrees.
use constant ROTATION_ANGLE => 1;

# Serial number for generating unique element IDs.
my $serial_number = 1;


# Guess the center of the texture tile.  Returns (x,y) pair.
sub get_tile_center($$);
sub get_tile_center($$)
{
   my ($tile_size, $node) = @_;

   # Look for a <rect> element contained inside this subtree.
   my $name = eval('$node->nodeName');
   if( defined($name) && $name eq "rect" )
   {
      # Find coordinate of center of rectangle.
      my $x = $node->{"x"};
      my $y = $node->{"y"};
      my $w = $node->{"width"};
      my $h = $node->{"height"};
      defined($x) && defined($y) or die;

      if( defined($w) ) { $x += $w / 2; }
      if( defined($h) ) { $x += $h / 2; }

      # Align coordinate to top left corner of containing tile,
      # then return the center of that tile.
      return ($x - ($x % $tile_size) + $tile_size / 2,
              $y - ($y % $tile_size) + $tile_size / 2);
   }

   # Recursively search children for <rect>.
   foreach my $child ($node->childNodes())
   {
      my ($tx, $ty) = get_tile_center($tile_size, $child);
      if( defined($tx) )
      {
         return ($tx, $ty);
      }
   }

   return (undef, undef);
}

# Add rotated clones to SVG.
sub add_rotated_clones($$)
{
   my ($dom, $rotation_steps) = @_;

   # Get texture tile dimensions from SVG page dimensions, also adjust
   # page size to fit all rotated groups.
   my $tile_size = undef;
   foreach my $root ($dom->getElementsByTagName("svg"))
   {
      my $width = $root->{"width"};
      my $height = $root->{"height"};
      defined($width) or die;
      defined($height) or die;
      $root->{"width"} = $width * $rotation_steps;
      $root->{"viewBox"} = "0 0 " . ($width * $rotation_steps) . " $height";

      # Make sure root note has xlink namespace.
      $root->{"xmlns:xlink"} = "http://www.w3.org/1999/xlink";

      $tile_size = $width;
   }
   unless( defined($tile_size) )
   {
      die "Did not find root <svg> element\n";
   }

   # Generate clones of each group with "texture" prefix in their labels.
   foreach my $g ($dom->getElementsByTagName("g"))
   {
      if( defined($g->{"inkscape:groupmode"}) &&
          $g->{"inkscape:groupmode"} eq "layer" )
      {
         next;
      }
      my $label = $g->{"inkscape:label"};
      unless( defined($label) &&
              substr($label, 0, length(INPUT_PREFIX)) eq INPUT_PREFIX )
      {
         next;
      }
      defined($g->{"id"}) or die;

      # Get center of original tile.
      my ($x, $y) = get_tile_center($tile_size, $g);
      unless( defined($x) && defined($y) )
      {
         die "Can not get the tile center for group $label.\n" .
             "Need at least one <rect> element inside group.\n";
      }

      # Create clones.  Note that we skip over the zero rotation step since
      # that's the original.
      for(my $i = 1; $i < $rotation_steps; $i++)
      {
         my $clone = XML::LibXML::Element->new("use");
         $clone->{"id"} = "use" . $serial_number++;
         $clone->{"x"} = 0;
         $clone->{"y"} = 0;
         $clone->{"xlink:href"} = "#" . $g->{"id"};

         my $angle_degrees = $i * ROTATION_ANGLE;
         my $angle_radians = $angle_degrees * PI / 180;

         # Rotation causes a coordinate to be transformed as follows:
         #
         #   nx = (x - rcx) * cos(a) - (y - rcy) * sin(a) + rcx
         #   ny = (x - rcx) * sin(a) + (y - rcy) * cos(a) + rcy
         #
         # Where (rcx,rcy) is the center of rotation.
         # We want to solve for (rcx,rcy) such that:
         #
         #   nx = x + h_offset
         #   ny = y
         #
         #   x*cos(a) - rcx*cos(a) - y*sin(a) + rcy*sin(a) + rcx = x + h_offset
         #   x*sin(a) - rcx*sin(a) + y*cos(a) - rcy*cos(a) + rcy = y
         #
         #   (1-cos(a))*rcx + sin(a)*rcy = x + tile_size - x*cos(a) + y*sin(a)
         #   -sin(a)*rcx + (1-cos(a))*rcy = y - x*sin(a) - y*cos(a)
         #
         # Applying Cramer's rule.
         my $cos_a = cos($angle_radians);
         my $sin_a = sin($angle_radians);
         my $a1 = 1 - $cos_a;
         my $b1 = $sin_a;
         my $c1 = $x + $tile_size * $i - $x * $cos_a + $y * $sin_a;
         my $a2 = -$sin_a;
         my $b2 = 1 - $cos_a;
         my $c2 = $y - $x * $sin_a - $y * $cos_a;

         my $d = $a1 * $b2 - $b1 * $a2;
         my $rcx = ($c1 * $b2 - $b1 * $c2) / $d;
         my $rcy = ($a1 * $c2 - $c1 * $a2) / $d;

         $clone->{"transform"} = "rotate($angle_degrees,$rcx,$rcy)";
         $g->parentNode->addChild($clone);
      }
   }
}


if( $#ARGV < 0 )
{
   die "$0 {rotation_steps} {input.svg} > {output.svg}\n";
}

# Set rotation steps from first argument.
my $rotation_steps = shift @ARGV;

# Load SVG text from stdin or second argument.
my $text = join "", <ARGV>;
my $dom = XML::LibXML->load_xml(string => $text, no_blanks => 1);

# Update SVG and output.
add_rotated_clones($dom, $rotation_steps);
print $dom->toString();
