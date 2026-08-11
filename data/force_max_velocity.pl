#!/usr/bin/perl -w
# Take the output of convert_tab_to_c.pl and force all notes to be
# played with high velocity.

use strict;

while( my $line = <> )
{
   if( $line =~ /^(\s*\{\d+,\s*)(\d+)(,\s*[-]?\d+\},\s*)$/s )
   {
      print $1, -$2, $3;
   }
   else
   {
      print $line;
   }
}
