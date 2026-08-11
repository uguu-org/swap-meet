#!/bin/bash

if [[ $# -ne 1 ]]; then
   echo "$0 {dither.exe}"
   exit 1
fi
TOOL=$1

set -euo pipefail
TEST_DIR=$(mktemp -d)
INPUT_PIXELS="$TEST_DIR/input_pixels.ppm"
INPUT_ALPHA="$TEST_DIR/input_alpha.ppm"
INPUT_IMAGE="$TEST_DIR/input_image.png"
EXPECTED_PIXELS="$TEST_DIR/expected_pixels.ppm"
EXPECTED_ALPHA="$TEST_DIR/expected_alpha.ppm"
ACTUAL_OUTPUT="$TEST_DIR/actual.ppm"

function die
{
   echo "$1"
   rm -rf "$TEST_DIR"
   exit 1
}

function check_output
{
   local test_id=$1
   local expected=$(ppmtoppm < "$EXPECTED_PIXELS" | ppmtopgm -plain)
   local actual=$(pngtopnm "$ACTUAL_OUTPUT" | ppmtopgm -plain)
   if [[ "$expected" != "$actual" ]]; then
      echo "Expected pixels:"
      echo "$expected"
      echo "Actual pixels:"
      echo "$actual"
      die "FAIL: $test_id"
   fi
   expected=$(ppmtoppm < "$EXPECTED_ALPHA" | ppmtopgm -plain)
   actual=$(pngtopnm -alpha "$ACTUAL_OUTPUT" | ppmtopgm -plain)
   if [[ "$expected" != "$actual" ]]; then
      echo "Expected alpha:"
      echo "$expected"
      echo "Actual alpha:"
      echo "$actual"
      die "FAIL: $test_id"
   fi
}

# ................................................................
# Test basic input/output.

cat <<EOT > "$INPUT_PIXELS"
P1
4 3
1 1 1 1
0 0 0 0
1 0 1 0
EOT
cat <<EOT > "$INPUT_ALPHA"
P1
4 3
1 0 0 1
1 0 1 0
1 0 0 1
EOT
pnmtopng -alpha="$INPUT_ALPHA" "$INPUT_PIXELS" > "$INPUT_IMAGE"
cat <<EOT > "$EXPECTED_PIXELS"
P1
4 3
1 1 1 1
1 0 1 0
1 0 1 1
EOT
cp "$INPUT_ALPHA" "$EXPECTED_ALPHA"

"./$TOOL" "$INPUT_IMAGE" "$ACTUAL_OUTPUT"
check_output "$LINENO: file in + file out"

cat "$INPUT_IMAGE" | "./$TOOL" - "$ACTUAL_OUTPUT"
check_output "$LINENO: stdin + file out"

"./$TOOL" "$INPUT_IMAGE" - > "$ACTUAL_OUTPUT"
check_output "$LINENO: file in + stdout"

cat "$INPUT_IMAGE" | "./$TOOL" - - > "$ACTUAL_OUTPUT"
check_output "$LINENO: stdin + stdout"

# ................................................................
# Test dither pattern.

# All white.
ppmmake rgb:ff/ff/ff 8 8 | pnmtopng > "$INPUT_IMAGE"
cat <<EOT > "$EXPECTED_PIXELS"
P1
8 8
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
0 0 0 0 0 0 0 0
EOT
cp "$EXPECTED_PIXELS" "$EXPECTED_ALPHA"
"./$TOOL" "$INPUT_IMAGE" "$ACTUAL_OUTPUT"
check_output "$LINENO: pixel=1 alpha=1"

# All black.
ppmmake rgb:00/00/00 8 8 | pnmtopng > "$INPUT_IMAGE"
cat <<EOT > "$EXPECTED_PIXELS"
P1
8 8
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1
EOT
"./$TOOL" "$INPUT_IMAGE" "$ACTUAL_OUTPUT"
check_output "$LINENO: pixel=0 alpha=1"

# Mix of black and white.  Regardless of dithering scheme, all pure black/white
# pixels input must map to black/white pixels in output.
cat <<EOT > "$EXPECTED_PIXELS"
P1
8 8
1 1 0 0 1 1 0 0
1 1 0 0 1 1 0 0
0 0 1 1 1 1 1 1
0 0 0 1 1 1 1 1
0 0 0 0 1 1 1 1
0 0 0 0 0 1 1 1
0 0 0 0 0 0 1 1
0 0 0 0 0 0 0 1
EOT
pnmtopng "$EXPECTED_PIXELS" > "$INPUT_IMAGE"
"./$TOOL" "$INPUT_IMAGE" "$ACTUAL_OUTPUT"
check_output "$LINENO: mixed pixels"

# 50% gray.  We don't know what kind pixel distribution we will get,
# but we expect the number of black and white pixels to be roughly
# half of input pixel count.
ppmmake rgb:80/80/80 10 10 | pnmtopng > "$INPUT_IMAGE"
"./$TOOL" "$INPUT_IMAGE" "$ACTUAL_OUTPUT"

RAW_PIXELS=$(pngtopnm "$ACTUAL_OUTPUT" \
             | pgmtopbm -threshold -plain \
             | sed -e '1,2d')
BLACK_PIXEL_COUNT=$(echo $RAW_PIXELS | perl -ne 's/[^1]//sg;print' | wc -c)
WHITE_PIXEL_COUNT=$(echo $RAW_PIXELS | perl -ne 's/[^0]//sg;print' | wc -c)
if [[ $BLACK_PIXEL_COUNT -lt 40 ]] || \
   [[ $BLACK_PIXEL_COUNT -gt 60 ]] || \
   [[ $WHITE_PIXEL_COUNT -lt 40 ]] || \
   [[ $WHITE_PIXEL_COUNT -gt 60 ]]; then \
   die "FAIL: $LINENO: gray=50%, black=$BLACK_PIXEL_COUNT, white=$WHITE_PIXEL_COUNT"
fi

# 25% gray.
ppmmake rgb:40/40/40 10 10 | pnmtopng > "$INPUT_IMAGE"
"./$TOOL" "$INPUT_IMAGE" "$ACTUAL_OUTPUT"

RAW_PIXELS=$(pngtopnm "$ACTUAL_OUTPUT" \
             | pgmtopbm -threshold -plain \
             | sed -e '1,2d')
BLACK_PIXEL_COUNT=$(echo $RAW_PIXELS | perl -ne 's/[^1]//sg;print' | wc -c)
WHITE_PIXEL_COUNT=$(echo $RAW_PIXELS | perl -ne 's/[^0]//sg;print' | wc -c)
if [[ $BLACK_PIXEL_COUNT -lt 65 ]] || \
   [[ $BLACK_PIXEL_COUNT -gt 85 ]] || \
   [[ $WHITE_PIXEL_COUNT -lt 15 ]] || \
   [[ $WHITE_PIXEL_COUNT -gt 35 ]]; then \
   die "FAIL: $LINENO: gray=25%, black=$BLACK_PIXEL_COUNT, white=$WHITE_PIXEL_COUNT"
fi

# ................................................................
# Cleanup.
rm -rf "$TEST_DIR"
exit 0
