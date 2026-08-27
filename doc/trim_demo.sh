#!/bin/bash

set -euo pipefail

ffmpeg -i playdate-20260826-233932.gif -y 'demo1_r%04d.png'

FADE_IN=40
GAME_START=55
GAME_END=635

for i in $(seq $GAME_START 1 $GAME_END); do
   mv -f \
      $(printf 'demo1_r%04d.png' $i) \
      $(printf 'demo1_f%04d.png' $(($i-$GAME_START)))
done
for i in $(seq $FADE_IN 1 $(($GAME_START-1))); do
   mv -f \
      $(printf 'demo1_r%04d.png' $i) \
      $(printf 'demo1_f%04d.png' $(($i-$FADE_IN+$GAME_END-$GAME_START+1)))
done

ffmpeg \
   -i 'demo1_f%04d.png' \
   -vf palettegen \
   -y demo1_palette.png

ffmpeg \
   -framerate 30 \
   -i 'demo1_f%04d.png' \
   -i demo1_palette.png \
   -lavfi paletteuse \
   -y playdate-20260826-233932-trimmed.gif

rm -f demo1_*.png
