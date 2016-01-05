#include <cstdlib>

#define _USE_MATH_DEFINES
#include <cmath>

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>
#include <algorithm>

#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

// Stupid macros!
static bool al_init_wrapper() {
	return al_init();
}

// Table of startup routines/messages
static const struct startup_t {
	bool (*proc)();
	const char *msg;
} startups[] = {
	{ al_init_wrapper, "Initializing Allegro system..." },
	{ al_install_keyboard, "Initializing keyboard subsystem..." },
	{ al_install_mouse, "Initializing mouse subsystem..." },
	{ al_install_audio, "Initializing audio subsystem..." },
	{ al_init_font_addon, "Initializing font subsystem..." },
	{ al_init_primitives_addon, "Initializing graphics primitives subsystem..." },
	{ nullptr, nullptr }
};

void startup() {
	for (const startup_t *s = startups; s->proc != nullptr; ++s) {
		std::cout << s->msg;
		if (s->proc()) {
			std::cout << "OK\n";
		}
		else {
			std::cout << "FAILED (errno=" << al_get_errno() << ")\n";
			al_uninstall_system();
			exit(1);
		}
	}
}

// Template specialization: have ALLEGRO_XXX ptr objectes "deleted" with al_destroy_xxx
#define ALLEGRO_PTR_DELETER(struct_t, deleter_proc) \
	namespace std { \
		template<> \
		class default_delete<struct_t> \
		{ \
		public: \
			void operator()(struct_t *ptr) { \
				/*std::cerr << "Deleting " #struct_t " @ " << (void *)ptr << std::endl;*/ \
				deleter_proc(ptr); \
			} \
		}; \
	}

ALLEGRO_PTR_DELETER(ALLEGRO_DISPLAY, al_destroy_display)
ALLEGRO_PTR_DELETER(ALLEGRO_BITMAP, al_destroy_bitmap)
ALLEGRO_PTR_DELETER(ALLEGRO_SAMPLE, al_destroy_sample)
ALLEGRO_PTR_DELETER(ALLEGRO_FILE, al_fclose)
ALLEGRO_PTR_DELETER(ALLEGRO_FS_ENTRY, al_destroy_fs_entry)
ALLEGRO_PTR_DELETER(ALLEGRO_EVENT_QUEUE, al_destroy_event_queue)
ALLEGRO_PTR_DELETER(ALLEGRO_TIMER, al_destroy_timer)
ALLEGRO_PTR_DELETER(ALLEGRO_FONT, al_destroy_font)

// Use unique_ptrs to manage the lifetime of various objects
using DisplayPtr = std::unique_ptr<ALLEGRO_DISPLAY>;
using BitmapPtr = std::unique_ptr<ALLEGRO_BITMAP>;
using SamplePtr = std::unique_ptr<ALLEGRO_SAMPLE>;
using FilePtr = std::unique_ptr<ALLEGRO_FILE>;
using FsEntryPtr = std::unique_ptr<ALLEGRO_FS_ENTRY>;
using EventQueuePtr = std::unique_ptr<ALLEGRO_EVENT_QUEUE>;
using TimerPtr = std::unique_ptr<ALLEGRO_TIMER>;
using FontPtr = std::unique_ptr<ALLEGRO_FONT>;

// VGA Mode 13h constants (since all original graphics were tied to that mode)
static constexpr size_t VGA13_WIDTH = 320, VGA13_HEIGHT = 200, VGA13_COLORS = 256;

// Some basic type alias for convenience
using Buffer = std::vector<uint8_t>;
using Palette = std::array<ALLEGRO_COLOR, VGA13_COLORS>;

// Utility function to open and read the entire [binary] contents
// of a given file into a vector<char> (resizing as necessary)
bool slurp_file(const char *filename, Buffer& dest) {
	FsEntryPtr ent{ al_create_fs_entry(filename) };
	if (!ent) {
		return false;
	}
	
	FilePtr fp{ al_fopen(filename, "rb") };
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
bool bload_file(const char *filename, Buffer& dest) {
	FilePtr fp{ al_fopen(filename, "rb") };
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
BitmapPtr bload_convert(const Buffer& data, const Palette& pal) {
	BitmapPtr bmp{ al_create_bitmap(VGA13_WIDTH, VGA13_HEIGHT) };

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

class TempTargetBitmap {
	ALLEGRO_BITMAP		*original_bitmap_;
	ALLEGRO_TRANSFORM	original_transform_,
						original_projection_;
public:
	explicit TempTargetBitmap(ALLEGRO_BITMAP *new_target) :
		original_bitmap_(al_get_target_bitmap()),
		original_transform_(*al_get_current_transform()),
		original_projection_(*al_get_current_projection_transform())
	{
		al_set_target_bitmap(new_target);
	}

	// No copy/move/assign
	TempTargetBitmap(const TempTargetBitmap& other) = delete;
	TempTargetBitmap(TempTargetBitmap&& other) = delete;
	TempTargetBitmap& operator=(const TempTargetBitmap& other) = delete;

	// Auto restore target bitmap (with transforms) on cleanup
	~TempTargetBitmap() {
		al_set_target_bitmap(original_bitmap_);
		al_use_transform(&original_transform_);
		al_use_projection_transform(&original_projection_);
	}
};

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
static constexpr size_t ENEMY_PAL_COLORS{ 80 }, ENEMY_PAL_START{ 64 },
	ENEMY_PAL_END{ ENEMY_PAL_START + ENEMY_PAL_COLORS };

// Locations/metadata of font data withing RESOURCE.BIN
static constexpr size_t FONT_DATA_OFFSET{ 1884 }, FONT_GLYPH_SIZE{ 8 },
FONT_NUM_GLYPHS{ 224 }, FONT_ASCII_START{ 32 },
FONT_ASCII_END{ FONT_ASCII_START + FONT_NUM_GLYPHS },
FONT_GLYPH_WIDTH{ FONT_GLYPH_SIZE },		// Width of actual glpyh image
FONT_GLYPH_HEIGHT{ FONT_GLYPH_SIZE },		// Height of actual glyph image
FONT_CELL_WIDTH{ FONT_GLYPH_SIZE + 2 },		// Width of font grid glyph cell in pixels
FONT_CELL_HEIGHT{ FONT_GLYPH_SIZE + 2 },	// Ditto for cell height
FONT_GRID_COLS{ 16 },						// Columns in font grid bitmap (arbitrary)
FONT_GRID_ROWS{ FONT_NUM_GLYPHS / FONT_GRID_COLS };

// Abstraction of all resources packed into RESOURCE.BIN
class ResourceBin {
	// Raw backing store (entire file read into memory)
	Buffer data_;

	// Audio samples
	std::vector<SamplePtr> wavs_;

	// Gameplay palettes (256 colors each, although palettes 1 and 2 differ only in the 80 colors in the range 64..143)
	std::array<Palette, 3> palettes_;

	// Menu palette
	Palette menu_pal_;

	// Game font
	BitmapPtr font_bmp_;
	FontPtr font_;
public:
	enum PALETTE {
		DEFAULT = 0,
		RED_ENEMIES = 1,
		BLUE_ENEMIES = 2,

		NUM_PALETTES = 3
	};

	explicit ResourceBin(const char *pathToResourceBin = "RESOURCE.BIN") {
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
		for (size_t p = RED_ENEMIES; p < NUM_PALETTES; ++p) {
			std::copy(palettes_[DEFAULT].begin(), palettes_[DEFAULT].begin() + 64, palettes_[p].begin());			// 0..63 copied
			for (size_t i = ENEMY_PAL_START; i < ENEMY_PAL_END; ++i) {
				const uint8_t *cp = &data_[offset];
				uint8_t r = cp[0] * 4;
				uint8_t g = cp[1] * 4;
				uint8_t b = cp[2] * 4;
				offset += 3;
				palettes_[p][i] = al_map_rgba(r, g, b, 255);
			}
			std::copy(palettes_[DEFAULT].begin() + ENEMY_PAL_END,
				palettes_[DEFAULT].end(),
				palettes_[p].begin() + ENEMY_PAL_END);		// 144..255 copied
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

		// Create font (draw glyphs in rows of 16 to produce a someone squarish bitmap
		int fbw = FONT_GRID_COLS * FONT_CELL_WIDTH;
		int fbh = FONT_GRID_ROWS * FONT_CELL_HEIGHT;
		font_bmp_.reset(al_create_bitmap(fbw, fbh));
		if (!font_bmp_) {
			throw std::exception("Unable to create font grid bitmap");
		}

		TempTargetBitmap ttb{ font_bmp_.get() };
		al_clear_to_color(al_map_rgb(255, 0, 255));
		
		if (!al_lock_bitmap(font_bmp_.get(), ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_WRITEONLY)) {
			throw std::exception("Unable to lock font grid bitmap");
		}
		size_t glyph = 0;
		for (size_t r = 0; r < FONT_GRID_ROWS; ++r) {
			for (size_t c = 0; c < FONT_GRID_COLS; ++c) {
				uint8_t *bitmask = &data_[FONT_DATA_OFFSET + (glyph++ * FONT_GLYPH_SIZE)];
				int x1 = c * FONT_CELL_WIDTH + 1;
				int y1 = r * FONT_CELL_HEIGHT + 1;
				
				for (int y = 0; y < FONT_GLYPH_HEIGHT; ++y) {
					for (int x = 0; x < FONT_GLYPH_WIDTH; ++x) {
						bool bit = (bitmask[y] >> x) & 1;
						al_put_pixel(x1 + (FONT_GLYPH_WIDTH - x), y1 + y, bit ? al_map_rgba(255, 255, 255, 255) : al_map_rgba(0, 0, 0, 0));
					}
				}
			}
		}
		al_unlock_bitmap(font_bmp_.get());

		int ranges[] = { FONT_ASCII_START, FONT_ASCII_END - 1 };
		font_.reset(al_grab_font_from_bitmap(font_bmp_.get(), 1, ranges));
		if (!font_) {
			throw std::exception("Unable to grab font from bitmap");
		}
	}

	const Palette& menu_palette() const { return menu_pal_; }

	const Palette& palette(size_t index) const {
		return palettes_.at(index);
	}

	ALLEGRO_SAMPLE *sample(size_t index) const {
		return wavs_.at(index).get();
	}

	size_t num_samples() const {
		return NUM_SOUNDS;
	}

	ALLEGRO_BITMAP *font_bmp() const {
		return font_bmp_.get();
	}

	ALLEGRO_FONT *font() const {
		return font_.get();
	}
};

static constexpr size_t SPRITES_COLS = 20;
static constexpr size_t SPRITES_ROWS = 12;
static constexpr size_t SPRITE_WIDTH = 16;
static constexpr size_t SPRITE_HEIGHT = 16;
static constexpr size_t NUM_SPRITES = SPRITES_COLS * SPRITES_ROWS;

class SpritesBin {
	// Bitmaps containing all the data from SPRITES.BIN in the 3 available palettes
	std::array<BitmapPtr, 3> sprite_maps_;

	// Sub-bitmaps for each sprite (for each palette)
	std::array<std::array<BitmapPtr, NUM_SPRITES>, 3> sprites_;
public:
	// Must have loaded palette data from RESOURCE.BIN first!
	SpritesBin(const ResourceBin& rsrc, const char *pathToSpritesBin = "SPRITES.BIN") {
		Buffer raw;
		if (!bload_file(pathToSpritesBin, raw)) {
			throw std::exception("Unable to read data from SPRITES.BIN");
		}

		for (size_t i = 0; i < 3; ++i) {
			sprite_maps_[i] = bload_convert(raw, rsrc.palette(i));

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

	ALLEGRO_BITMAP *sprite_map(size_t palette = 0) {
		return sprite_maps_.at(palette).get();
	}

	ALLEGRO_BITMAP *sprite(size_t shape, size_t palette = 0) {
		return sprites_.at(palette).at(shape).get();
	}
};

static constexpr size_t SPRITEOBJ_MAX_FRAMES = 8;

class SpriteObj {
	float x_, y_;
	float dx_, dy_;
	size_t curr_frame_;
	size_t max_frame_;
	size_t frame_ttl_;
	size_t frame_init_ttl_;
	std::array<ALLEGRO_BITMAP *, SPRITEOBJ_MAX_FRAMES> frames_;
public:
	SpriteObj(ALLEGRO_BITMAP *init_frame, float init_x, float init_y) :
		x_(init_x), y_(init_y), dx_(0), dy_(0),
		curr_frame_(0), max_frame_(1),
		frame_ttl_(0), frame_init_ttl_(0)
	{
		frames_[0] = init_frame;
	}

	void add_frame(ALLEGRO_BITMAP *next_frame) {
		if (max_frame_ < frames_.size()) {
			frames_[max_frame_++] = next_frame;
		}
	}

	void animate(size_t ttl) {
		frame_init_ttl_ = ttl;
		frame_ttl_ = ttl;
	}

	void update() {
		x_ += dx_;
		y_ += dy_;
		if (frame_init_ttl_) {
			if (--frame_ttl_ == 0) {
				if (++curr_frame_ >= max_frame_) { curr_frame_ = 0; }
				frame_ttl_ = frame_init_ttl_;
			}
		}
	}

	void render() {
		al_draw_bitmap(frames_[curr_frame_], x_, y_, 0);
	}

};

static constexpr int DWIDTH = 640, DHEIGHT = 480;

void allegro_die(const char *msg) {
	std::cout << msg << " (errno=" << al_get_errno() << ")\n";
	exit(1);
}

int main(int argc, char **argv) {
	startup();

	EventQueuePtr events{ al_create_event_queue() };
	if (!events) { allegro_die("Unable to create event queue"); }
	al_register_event_source(events.get(), al_get_keyboard_event_source());
	al_register_event_source(events.get(), al_get_mouse_event_source());

	TimerPtr timer{ al_create_timer(1.0 / 60) };
	if (!timer) { allegro_die("Unable to create timer"); }
	al_register_event_source(events.get(), al_get_timer_event_source(timer.get()));
	
	DisplayPtr dptr{ al_create_display(DWIDTH, DHEIGHT) };
	if (!dptr) { allegro_die("Unable to create display"); }
	al_register_event_source(events.get(), al_get_display_event_source(dptr.get()));

	ResourceBin rsrc{ "RESOURCE.BIN" };
	SpritesBin sprites{ rsrc, "SPRITES.BIN" };

	if (!al_reserve_samples(rsrc.num_samples())) { allegro_die("Failed to reserve samples"); }
	
	Buffer raw;
	if (!bload_file("TITLE.BIN", raw)) { exit(1); }
	BitmapPtr bgrd{ bload_convert(raw, rsrc.menu_palette()) };
	if (!bgrd) { allegro_die("Unable to BLOAD TITLE.BIN"); }

	SpriteObj crab{ sprites.sprite(1, 0), DWIDTH / 2.0f, DHEIGHT / 2.0f };
	crab.add_frame(sprites.sprite(0));
	crab.add_frame(sprites.sprite(1));
	crab.add_frame(sprites.sprite(2));
	crab.animate(10);

	al_start_timer(timer.get());
	bool done = false;
	bool render = true;
	while (!done) {
		// Drain event queue
		ALLEGRO_EVENT evt;
		while (al_get_next_event(events.get(), &evt)) {
			switch (evt.type) {
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				done = true;
				break;
			case ALLEGRO_EVENT_TIMER:
				if (evt.timer.source == timer.get()) {
					crab.update();
					render = true;
				}
				break;
			}
		}

		if (render) {
			al_draw_scaled_bitmap(bgrd.get(), 0, 0, 320, 200, 0, 0, DWIDTH, DHEIGHT, 0);
			al_draw_scaled_bitmap(rsrc.font_bmp(), 0, 0, 160, 120, 0, 0, DWIDTH, DHEIGHT, 0);
			//al_draw_textf(rsrc.font(), al_map_rgb(255, 255, 255), DWIDTH / 2, 0, ALLEGRO_ALIGN_CENTER, "HELLO, WORLD!");
			crab.render();
			al_flip_display();
			render = false;
		}
	}

	/*while (std::cin >> sound_index) {
		if ((sound_index < 0) || (sound_index >= (int)rsrc.num_samples())) { break; }
		al_play_sample(rsrc.sample(sound_index), 1.0f, ALLEGRO_AUDIO_PAN_NONE, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
		std::cout << "Enter a sample index [0, " << rsrc.num_samples() << "): ";

		pal = (pal < 2) ? (pal + 1) : 0;
		if (++shape >= NUM_SPRITES) { shape = 0; }
		al_clear_to_color(al_map_rgb(128, 128, 128));
		al_draw_scaled_bitmap(bgrd.get(), 0, 0, 320, 200, 0, 0, 640, 400, 0);
		al_draw_bitmap(sprites.sprite(shape, pal), 50, 50, 0);
		al_flip_display();
	}*/

	return 0;
}
