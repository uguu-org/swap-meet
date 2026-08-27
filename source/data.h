// Collection of various generated data.

#ifndef DATA_H_
#define DATA_H_

#include<stdint.h>

// generate_gray_patterns.pl
extern const uint8_t kTranslucentWhite[65][16];
extern const uint8_t kTranslucentBlack[65][16];
extern const uint8_t kOpaqueGray[65][16];

// generate_item_positions.pl
#define ITEM_PILE_SIZE 50
extern const int kItemPile[ITEM_PILE_SIZE][2];

// Build version.
extern const char *kVersion;

#endif  // DATA_H_
