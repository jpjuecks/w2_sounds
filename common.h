#ifndef W2DIR_COMMON_H
#define W2DIR_COMMON_H
// Catch-all common types/constants for WetSpot 2 DIR
//---------------------------------------------------

// Heavily-used stdlibc++/STL types
#include <array>
#include <vector>
#include <exception>

// Allegro color types
#include <allegro5/color.h>

// VGA Mode 13h constants (since all original graphics were tied to that mode)
static constexpr size_t VGA13_WIDTH = 320, VGA13_HEIGHT = 200, VGA13_COLORS = 256;

// Convenience type alias for "expandable buffer of raw octets/bytes"
using Buffer = std::vector<uint8_t>;

// A VGA Mode 13h color palette type (256 Allegro color definitions)
using Palette = std::array<ALLEGRO_COLOR, VGA13_COLORS>;



#endif
