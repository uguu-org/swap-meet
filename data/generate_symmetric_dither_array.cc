// Generate tileable dither array, with the constraint that the dither
// array itself has rotational symmetry such rotating the dither array
// by 180 degrees will result in the same pattern.
//
// The rotational symmetry is useful if we want to draw some animated
// texture and also flip by XY axes.  Usually we would use ordered
// dithering (as opposed to Floyd-Steinberg) so that the dither
// pattern remains consistent across animation frames, but naively
// flipping a Bayer dithered texture across XY axes wouldn't work
// because the dither pattern would also be flipped.  This tool solves
// both problems:
//
// - Dither pattern will be consistent across all frames since the
//   dither array remain constant, which makes the output of this tool
//   suitable for animation.  This works just like other ordered
//   dithering schemes such as Bayer, but note that the target tile
//   size differs from Bayer dither patterns (see kTileSize).
//
// - Flipping across XY axes will be consistent because the dither
//   array is generated with rotational symmetry.
//
// This code mostly follows "The void-and-cluster method for dither array
// generation" by Robert Ulichney.

#include<assert.h>
#include<math.h>
#include<stdio.h>
#include<stdint.h>

#include<array>
#include<limits>
#include<random>
#include<vector>
#include<utility>

namespace {

// Tile size in pixels.
//
// Default is to generate patterns for use with 256x256 tiles.  It
// costs ~20 seconds to generate a single dither array at 256x256,
// Usually you want separate arrays for color/alpha, but the two
// arrays can be generated in parallel.
#ifndef TILE_SIZE
   static constexpr int kTileSize = 256;
#else
   static constexpr int kTileSize = TILE_SIZE;
#endif
static constexpr int kTileWidth = kTileSize;
static constexpr int kTileHeight = kTileSize / 2;

// Maximum influence distance from a single pixel.
static constexpr int kPixelRadius = kTileHeight / 2 - 1;

// Gaussian filter parameter.  Original paper said a sigma of 1.5 produced
// best results, but reducing that value appears to result in fewer clumps.
static constexpr double kSigma = 1.2;
static constexpr double kTwoSigmaSquared = 2.0 * kSigma * kSigma;

// Shared types.
using XY = std::pair<int, int>;
using BinaryPatternRow = std::array<uint8_t, kTileWidth>;
using BinaryPattern = std::array<BinaryPatternRow, kTileHeight>;
using DitherArrayRow = std::array<int, kTileSize>;
using DitherArray = std::array<DitherArrayRow, kTileSize>;

// Given an input coordinate, return an updated coordinate with wraparound.
//
// The usual way to do this would be to wrap what falls outside of any
// of the four edges to the other side using modulus, keeping one of
// the components the same.  We are still doing that for the vertical
// edges, but for the horizontal edges, we flip the coordinates around
// such that the out-of-bounds coordinates re-enter the bounding box
// via the same edge.  This is how we get our rotational symmetry.
static XY WrapCoordinate(int x, int y)
{
   if( y >= 0 && y < kTileHeight)
   {
      // Y value is within range.  Here we do a simple wraparound.
      return std::make_pair((x + kTileWidth) % kTileWidth, y);
   }

   // Y exceeds horizontal edges.  Flip both X and Y coordinates around.
   return std::make_pair((kTileWidth * 2 - 1 - x) % kTileWidth,
                         (kTileHeight * 2 - 1 - y) % kTileHeight);
}
static void TestWrapCoordinate()
{
   // Check for coordinates within bounds.
   assert(WrapCoordinate(0, 0) == std::make_pair(0, 0));
   assert(WrapCoordinate(kTileWidth - 1, kTileHeight - 1) ==
          std::make_pair(kTileWidth - 1, kTileHeight - 1));

   // Check for coordinates exceeding horizontal bounds.
   assert(WrapCoordinate(kTileWidth, 0) == std::make_pair(0, 0));
   assert(WrapCoordinate(-1, 0) == std::make_pair(kTileWidth - 1, 0));
   assert(WrapCoordinate(kTileWidth, kTileHeight - 1) ==
          std::make_pair(0, kTileHeight - 1));
   assert(WrapCoordinate(-1, kTileHeight - 1) ==
          std::make_pair(kTileWidth - 1, kTileHeight - 1));

   // Check for coordinates exceeding bottom edge.
   assert(WrapCoordinate(0, kTileHeight) ==
          std::make_pair(kTileWidth - 1, kTileHeight - 1));
   assert(WrapCoordinate(kTileWidth - 1, kTileHeight) ==
          std::make_pair(0, kTileHeight - 1));
   assert(WrapCoordinate(kTileWidth, kTileHeight) ==
          std::make_pair(kTileWidth - 1, kTileHeight - 1));
   assert(WrapCoordinate(-1, kTileHeight) ==
          std::make_pair(0, kTileHeight - 1));

   // Check for coordinates exceeding top edge.
   assert(WrapCoordinate(0, -1) == std::make_pair(kTileWidth - 1, 0));
   assert(WrapCoordinate(kTileWidth - 1, -1) == std::make_pair(0, 0));
   assert(WrapCoordinate(0, -kTileHeight) ==
          std::make_pair(kTileWidth - 1, kTileHeight - 1));
   assert(WrapCoordinate(kTileWidth - 1, -kTileHeight) ==
          std::make_pair(0, kTileHeight - 1));
}

// Maintain contributions from each pixel.
struct InfluenceGrid
{
public:
   InfluenceGrid() = default;

   // Copy another grid.
   explicit InfluenceGrid(const InfluenceGrid &other)
      : grid_(other.grid_),
        one_count_(other.one_count_),
        min_point_(other.min_point_),
        max_point_(other.max_point_)
   {
   }

   // Initialize grid from random binary pattern.
   void Init(const BinaryPattern &pattern);

   // Update grid with influences from a single pixel.
   void UpdatePixel(int x, int y, int direction);

   // Update grid with influences from a single pixel, and also update
   // min_point_ and max_point_.
   void UpdateGrid(const BinaryPattern &pattern, int x, int y, int direction)
   {
      UpdatePixel(x, y, direction);
      UpdateMinMax(pattern);
   }

   // Number of "1" pixels observed during initialization.
   int one_count() const { return one_count_; }

   // Get center of largest void or cluster.
   const XY &min_point() const { return min_point_; }
   const XY &max_point() const { return max_point_; }

private:
   InfluenceGrid& operator=(const InfluenceGrid&) = delete;

   // Update min_point_ and max_point_.
   void UpdateMinMax(const BinaryPattern &pattern);

   using Row = std::array<double, kTileSize>;
   using Grid = std::array<Row, kTileHeight>;

   // Grid of pixel influence values.
   Grid grid_;

   // Number of "1" pixels observed during initialization.
   int one_count_;

   // Coordinate of minimum and maximum values.
   XY min_point_;
   XY max_point_;
};

// Initialize grid from random binary pattern.
void InfluenceGrid::Init(const BinaryPattern &pattern)
{
   // Start with all zeroes.
   for(int y = 0; y < kTileHeight; y++)
      grid_[y].fill(0.0);

   // Add contributions from each 1 pixel.
   one_count_ = 0;
   for(int iy = 0; iy < kTileHeight; iy++)
   {
      for(int ix = 0; ix < kTileWidth; ix++)
      {
         if( pattern[iy][ix] == 0 )
            continue;
         UpdatePixel(ix, iy, 1);
         one_count_++;
      }
   }
   UpdateMinMax(pattern);
}

// Update grid with influences from a single pixel.
void InfluenceGrid::UpdatePixel(int x, int y, int direction)
{
   for(int dy = -kPixelRadius; dy <= kPixelRadius; dy++)
   {
      const int dy2 = dy * dy;
      for(int dx = -kPixelRadius; dx <= kPixelRadius; dx++)
      {
         const int r2 = dy2 + dx * dx;
         const double g = exp(-r2 / kTwoSigmaSquared);
         const XY p = WrapCoordinate(x + dx, y + dy);
         grid_[p.second][p.first] += direction * g;
      }
   }
}

// Update min_point_ and max_point_.
void InfluenceGrid::UpdateMinMax(const BinaryPattern &pattern)
{
   double peak = std::numeric_limits<double>::lowest();
   double nadir = std::numeric_limits<double>::max();
   for(int y = 0; y < kTileHeight; y++)
   {
      for(int x = 0; x < kTileWidth; x++)
      {
         const double value = grid_[y][x];
         if( pattern[y][x] == 0 && nadir > value )
         {
            nadir = value;
            min_point_.first = x;
            min_point_.second = y;
         }
         if( pattern[y][x] != 0 && peak < value )
         {
            peak = value;
            max_point_.first = x;
            max_point_.second = y;
         }
      }
   }
}

// See figure 2 in section 4 of the void-and-cluster paper.
static void GenerateInitialBinaryPattern(
   int seed,
   BinaryPattern *binary_pattern,
   InfluenceGrid *grid)
{
   // Initialize random pattern.
   std::mt19937 prng;
   prng.seed(seed);

   std::uniform_int_distribution<> dist(0, 9);
   int minority_count = 0;
   for(int y = 0; y < kTileHeight; y++)
   {
      for(int x = 0; x < kTileWidth; x++)
      {
         if( dist(prng) == 0 )
         {
            (*binary_pattern)[y][x] = 1;
            minority_count++;
         }
         else
         {
            (*binary_pattern)[y][x] = 0;
         }
      }
   }
   assert(minority_count > 0);

   // Convert random bit patterns to initial binary pattern.
   //
   // The loop below is bounded to avoid infinite loops, but usually we
   // will be done after relocating ~25% of the minority pixels.
   grid->Init(*binary_pattern);
   static constexpr int kMaxIterations = kTileSize * kTileSize;
   for(int iteration = 0; iteration < kMaxIterations; iteration++)
   {
      // Remove "1" from tightest cluster.
      const XY peak = grid->max_point();
      (*binary_pattern)[peak.second][peak.first] = 0;
      grid->UpdateGrid(*binary_pattern, peak.first, peak.second, -1);

      // Find largest void, stop if it's at the pixel that was just removed.
      const XY nadir = grid->min_point();
      if( peak == nadir )
      {
         // Undo the removal.
         (*binary_pattern)[peak.second][peak.first] = 1;
         grid->UpdateGrid(*binary_pattern, peak.first, peak.second, 1);
         break;
      }

      // Add "1" to largest void.
      (*binary_pattern)[nadir.second][nadir.first] = 1;
      grid->UpdateGrid(*binary_pattern, nadir.first, nadir.second, 1);
   }
}

// See figure 5(a) in void-and-cluster paper.
static void GeneratePhase1(const BinaryPattern &pattern0,
                           const InfluenceGrid &grid0,
                           DitherArray *dither_array)
{
   // Work off of copies of the initial binary pattern and influence grid,
   // since we need to reload them for phase 2.
   BinaryPattern pattern(pattern0);
   InfluenceGrid grid(grid0);

   for(int rank = grid.one_count() - 1; rank >= 0; rank--)
   {
      const XY peak = grid.max_point();
      pattern[peak.second][peak.first] = 0;
      grid.UpdateGrid(pattern, peak.first, peak.second, -1);
      (*dither_array)[peak.second][peak.first] = rank;
   }
}

// See figure 5(b) in void-and-cluster paper.
static void GeneratePhase2(BinaryPattern *pattern,
                           InfluenceGrid *grid,
                           DitherArray *dither_array)
{
   for(int rank = grid->one_count(); rank < kTileWidth * kTileHeight / 2;
       rank++)
   {
      const XY nadir = grid->min_point();
      (*pattern)[nadir.second][nadir.first] = 1;
      grid->UpdateGrid(*pattern, nadir.first, nadir.second, 1);
      (*dither_array)[nadir.second][nadir.first] = rank;
   }
}

// See figure 5(c) in void-and-cluster paper.
static void GeneratePhase3(BinaryPattern *pattern, DitherArray *dither_array)
{
   // Invert binary pattern.
   for(int y = 0; y < kTileHeight; y++)
   {
      for(int x = 0; x < kTileWidth; x++)
         (*pattern)[y][x] ^= 1;
   }
   InfluenceGrid grid;
   grid.Init(*pattern);

   for(int rank = kTileWidth * kTileHeight / 2; rank < kTileWidth * kTileHeight;
       rank++)
   {
      const XY peak = grid.max_point();
      (*pattern)[peak.second][peak.first] = 0;
      grid.UpdateGrid(*pattern, peak.first, peak.second, -1);
      (*dither_array)[peak.second][peak.first] = rank;
   }
}

// Scale dither array values to 0..255.
static void NormalizeDitherArray(DitherArray *dither_array)
{
   for(int y = 0; y < kTileHeight; y++)
   {
      for(int x = 0; x < kTileWidth; x++)
      {
         (*dither_array)[y][x] =
            (*dither_array)[y][x] * 255 / (kTileWidth * kTileHeight - 1);
      }
   }
}

// Populate bottom half of dither array by rotating the top half.
static void ApplyRotationalSymmetry(DitherArray *dither_array)
{
   for(int y = 0; y < kTileHeight; y++)
   {
      for(int x = 0; x < kTileWidth; x++)
      {
         (*dither_array)[kTileSize - 1 - y][kTileSize - 1 - x] =
            (*dither_array)[y][x];
      }
   }
}

}  // namespace

int main(int argc, char **argv)
{
   TestWrapCoordinate();

   if( argc != 3 )
      return printf("%s {identifier} {seed}\n", *argv);

   BinaryPattern binary_pattern;
   InfluenceGrid grid;
   GenerateInitialBinaryPattern(atoi(argv[2]), &binary_pattern, &grid);

   DitherArray dither_array;
   GeneratePhase1(binary_pattern, grid, &dither_array);
   GeneratePhase2(&binary_pattern, &grid, &dither_array);
   GeneratePhase3(&binary_pattern, &dither_array);

   NormalizeDitherArray(&dither_array);
   ApplyRotationalSymmetry(&dither_array);

   printf("#ifndef DITHER_ARRAY_SIZE\n"
          "#define DITHER_ARRAY_SIZE %d\n"
          "#endif\n"
          "static const uint8_t %s[%d][%d] =\n{\n",
          kTileSize, argv[1], kTileSize, kTileSize);
   for(int y = 0; y < kTileSize; y++)
   {
      for(int x = 0; x < kTileSize; x++)
         printf(x == 0 ? "\t{%d" : ",%d", dither_array[y][x]);
      puts("},");
   }
   puts("};");
   return 0;
}
