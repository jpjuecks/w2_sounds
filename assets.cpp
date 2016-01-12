// Resource loading/handling logic for the built-in WetSpot 2 assets (sprites, sounds, palettes, etc.)
//----------------------------------------------------------------------------------------------------
#include "assets.h"

// Utility function to open and read the entire [binary] contents
// of a given file into a vector<char> (resizing as necessary)
bool slurp_file(const char *filename, Buffer& dest) {
	awful::FsEntryPtr ent{ al_create_fs_entry(filename) };
	if (!ent) {
		return false;
	}

	awful::FilePtr fp{ al_fopen(filename, "rb") };
	if (!fp) {
		return false;
	}

	dest.resize(al_get_fs_entry_size(ent.get()));
	if (al_fread(fp.get(), &dest[0], dest.size()) < dest.size()) {
		return false;
	}

	return true;
}

// Like slurp_file, but specifically for loading QuickBASIC
// VGA mode 13h BSAVEd image files.
static bool bload_file(const char *filename, Buffer& dest) {
	awful::FilePtr fp{ al_fopen(filename, "rb") };
	if (!fp) { return false; }

	uint8_t bmagic = al_fgetc(fp.get());
	if (bmagic != 0xFD) { return false; }

	if (!al_fseek(fp.get(), 4, ALLEGRO_SEEK_CUR)) { return false; }

	uint16_t bsize = al_fread16le(fp.get());

	dest.resize(bsize);
	if (al_fread(fp.get(), &dest[0], bsize) < bsize) { return false; }

	return true;
}

// Convert the raw data of a BSAVEd VGA mode 13h image to an ALLEGRO_BITMAP
// using a given palette
awful::BitmapPtr bload_convert(const Buffer& data, const Palette& pal) {
	awful::BitmapPtr bmp{ al_create_bitmap(VGA13_WIDTH, VGA13_HEIGHT) };

	if (!al_lock_bitmap(bmp.get(), ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_WRITEONLY)) {
		throw std::exception("Unable to lock ALLEGRO_BITMAP for writing");
	}

	auto original = al_get_target_bitmap();
	al_set_target_bitmap(bmp.get());
	for (int y = 0; y < VGA13_HEIGHT; ++y) {
		for (int x = 0; x < VGA13_WIDTH; ++x) {
			size_t offset = (VGA13_WIDTH * y) + x;
			if (offset < data.size()) {
				al_put_pixel(x, y, pal[data[offset]]);
			}
		}
	}
	al_unlock_bitmap(bmp.get());
	al_set_target_bitmap(original);

	return bmp;
}

awful::BitmapPtr bload_image(const char *file_name, const Palette& pal) {
	Buffer temp;
	if (!bload_file(file_name, temp)) { throw std::exception("Unable to load BSAVED data from disk"); }
	return bload_convert(temp, pal);
}


// Frequency in Hertz (samples per second) for PCM WAV data
static constexpr int FREQUENCY{ 11025 };

// Locations/sizes of 8-bit PCM (11.025 KHz) audio samples in Wetspot2's RESOURCE.BIN file
static const struct sample_loc_t {
	size_t offset;
	size_t length;
} samples[] = {
	{ 4802, 1470 },
	{ 6272, 1714 },
	{ 7986, 6386 },
	{ 14372, 9456 },
	{ 23828, 9488 },
	{ 33316, 7824 },
	{ 41140, 3674 },
	{ 44814, 12338 },
	{ 57152, 3256 },
	{ 60408, 28864 },
	{ 89272, 26816 },
	{ 116088, 18048 },
	{ 134136, 3690 },
	{ 137826, 15822 },
	{ 153648, 4694 },
	{ 158342, 1754 },
	{ 160096, 10020 },
	{ 170116, 5782 },
	{ 175898, 9584 },
};
static constexpr size_t NUM_SOUNDS = sizeof(samples) / sizeof(samples[0]);

// Locations/metadata of palette data within RESOURCE.BIN
static constexpr size_t DEFAULT_PAL_OFFSET{ 636 }, MENU_PAL_OFFSET{ 4033 };
static constexpr size_t NUM_ENEMY_PALS{ 3 }, ENEMY_PAL_COLORS{ 80 }, ENEMY_PAL_START{ 64 },
ENEMY_PAL_END{ ENEMY_PAL_START + ENEMY_PAL_COLORS };

// Locations/metadata of font data withing RESOURCE.BIN
static constexpr size_t FONT_DATA_OFFSET{ 2124 }, FONT_GLYPH_SIZE{ 8 },
FONT_NUM_GLYPHS{ 224 }, FONT_ASCII_START{ 32 },
FONT_ASCII_END{ FONT_ASCII_START + FONT_NUM_GLYPHS },
FONT_GLYPH_WIDTH{ FONT_GLYPH_SIZE },		// Width of actual glpyh image
FONT_GLYPH_HEIGHT{ FONT_GLYPH_SIZE },		// Height of actual glyph image
FONT_CELL_WIDTH{ FONT_GLYPH_SIZE + 2 },		// Width of font grid glyph cell in pixels
FONT_CELL_HEIGHT{ FONT_GLYPH_SIZE + 2 },	// Ditto for cell height
FONT_GRID_COLS{ 16 },						// Columns in font grid bitmap (arbitrary)
FONT_GRID_ROWS{ FONT_NUM_GLYPHS / FONT_GRID_COLS };

ResourceBin::ResourceBin(const char *pathToResourceBin) {
	if (!slurp_file(pathToResourceBin, data_)) {
		throw std::exception("Unable to load RESOURCE.BIN data");
	}

	// Create ALLEGRO_SAMPLE objects for each raw sample contained in our data
	for (size_t i = 0; i < NUM_SOUNDS; ++i) {
		void *sample_start = &data_[samples[i].offset];
		wavs_.emplace_back(al_create_sample(sample_start, samples[i].length, FREQUENCY, ALLEGRO_AUDIO_DEPTH_UINT8, ALLEGRO_CHANNEL_CONF_1, false));
	}

	// Create default palette colors
	size_t offset = DEFAULT_PAL_OFFSET;
	for (size_t i = 0; i < VGA13_COLORS; ++i) {
		const uint8_t *cp = &data_[offset];
		uint8_t a = (i == 0) ? 0 : 255;		// Color 0 is the transparent color for sprites
		uint8_t r = cp[0] * 4;				// Original VGA palette entries are 0-63; we need 0-255
		uint8_t g = cp[1] * 4;
		uint8_t b = cp[2] * 4;
		palettes_[0][i] = al_map_rgba(r, g, b, a);
		offset += 3;
	}

	// Create enemy palettes
	for (size_t p = PAL_RED_ENEMIES; p < PAL_COUNT; ++p) {
		// Copy colors 0..63 from the default game palette
		std::copy(begin(palettes_[PAL_DEFAULT]),
			begin(palettes_[PAL_DEFAULT]) + 64,
			begin(palettes_[p]));

		// Parse the palette-specific colors
		for (size_t i = ENEMY_PAL_START; i < ENEMY_PAL_END; ++i) {
			const uint8_t *cp = &data_[offset];
			uint8_t r = cp[0] * 4;
			uint8_t g = cp[1] * 4;
			uint8_t b = cp[2] * 4;
			offset += 3;
			palettes_[p][i] = al_map_rgba(r, g, b, 255);
		}

		// Copy colors 144..255 from the default palette
		std::copy(begin(palettes_[PAL_DEFAULT]) + ENEMY_PAL_END,
			end(palettes_[PAL_DEFAULT]),
			begin(palettes_[p]) + ENEMY_PAL_END);
	}

	// Create menu palette (at a different offset in the file)
	offset = MENU_PAL_OFFSET;
	for (size_t i = 0; i < VGA13_COLORS; ++i) {
		const uint8_t *cp = &data_[offset];
		uint8_t a = (i == 0) ? 0 : 255;		// Color 0 is the transparent color for sprites
		uint8_t r = cp[0] * 4;				// Original VGA palette entries are 0-63; we need 0-255
		uint8_t g = cp[1] * 4;
		uint8_t b = cp[2] * 4;
		menu_pal_[i] = al_map_rgba(r, g, b, a);
		offset += 3;
	}
}

SpritesBin::SpritesBin(
	const ResourceBin& rsrc,
	const char *pathToSpritesBin)
{
	Buffer raw;
	if (!bload_file(pathToSpritesBin, raw)) {
		throw std::exception("Unable to read data from SPRITES.BIN");
	}

	for (size_t i = 0; i < sprite_maps_.size(); ++i) {
		sprite_maps_[i] = bload_convert(raw, rsrc.game_palette(i));

		for (size_t n = 0; n < NUM_SPRITES; ++n) {
			auto x = (n % SPRITES_COLS) * SPRITE_WIDTH;
			auto y = (n / SPRITES_COLS) * SPRITE_HEIGHT;
			ALLEGRO_BITMAP *sprite = al_create_sub_bitmap(sprite_maps_[i].get(), x, y, SPRITE_WIDTH, SPRITE_HEIGHT);
			if (!sprite) {
				throw std::exception("Unable to create sub-bitmap sprite");
			}
			sprites_[i][n].reset(sprite);
		}
	}
}