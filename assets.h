#pragma once

#ifndef W2DIR_ASSETS_H
#define W2DIR_ASSETS_H

#include "awful.h"
#include "common.h"

// Utility function to open and read the entire [binary] contents
// of a given file into a vector<char> (resizing as necessary)
bool slurp_file(const char *filename, Buffer& dest);

// Convenience function to BLOAD an image file with a given palette
awful::BitmapPtr bload_image(const char *file_name, const Palette& pal);

// Principle asset collection used in the game, containing:
// - the font
// - all the palettes
// - all the sound effects
// (and other assets not needed by this implementation)
class ResourceBin {
public:
	// Palette numbering enum
	enum PALETTE {
		PAL_DEFAULT = 0,
		PAL_RED_ENEMIES = 1,
		PAL_BLUE_ENEMIES = 2,
		PAL_DIM_ENEMIES = 3,
		
		PAL_MAX = PAL_DIM_ENEMIES,
		PAL_COUNT = PAL_MAX + 1
	};

	// Ctor (loads and parses all resources from disk)
	explicit ResourceBin(const char *pathToResourceBin = "RESOURCE.BIN");

	// Asset getters
	const Palette& menu_palette() const { return menu_pal_; }
	const Palette& game_palette(size_t index) const {
		return palettes_.at(index);
	}
	ALLEGRO_SAMPLE *sound_sample(size_t index) const {
		return wavs_.at(index).get();
	}
	size_t num_sounds() const { return wavs_.size(); }

private:
	// Raw backing store (entire file read into memory)
	Buffer data_;

	// Audio samples
	std::vector<awful::SamplePtr> wavs_;

	// Gameplay palettes (256 colors each,
	// although palettes 1 - 3 differ only in the 80 colors
	// in the range 64..143)
	std::array<Palette, PAL_COUNT> palettes_;

	// The one menu palette
	Palette menu_pal_;
};

constexpr size_t SPRITES_COLS = 20;
constexpr size_t SPRITES_ROWS = 12;
constexpr size_t SPRITE_WIDTH = 16;
constexpr size_t SPRITE_HEIGHT = 16;
constexpr size_t NUM_SPRITES = SPRITES_COLS * SPRITES_ROWS;

class SpritesBin {
	// Bitmaps containing all the data from SPRITES.BIN in the available game palettes
	std::array<awful::BitmapPtr, ResourceBin::PAL_COUNT> sprite_maps_;

	// Sub-bitmaps for each sprite (for each palette)
	std::array<std::array<awful::BitmapPtr, NUM_SPRITES>, ResourceBin::PAL_COUNT> sprites_;
public:
	// Must have loaded palette data from RESOURCE.BIN first!
	SpritesBin(const ResourceBin& rsrc, const char *pathToSpritesBin = "SPRITES.BIN");

	// Get entire grid (for special effects)
	ALLEGRO_BITMAP *sprite_map(ResourceBin::PALETTE palette = ResourceBin::PAL_DEFAULT) {
		return sprite_maps_.at(palette).get();
	}

	// Get sub-bitmap of individual sprite
	ALLEGRO_BITMAP *sprite(size_t shape, ResourceBin::PALETTE palette = ResourceBin::PAL_DEFAULT) {
		return sprites_.at(palette).at(shape).get();
	}
};

#endif