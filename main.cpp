#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>
#include <algorithm>

#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>

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
	{ al_install_audio, "Initializing audio subsystem..." },
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

static constexpr size_t DEFAULT_PAL_OFFSET{ 636 };

// Template specialization: have ALLEGRO_XXX ptr objectes "deleted" with al_destroy_xxx
#define ALLEGRO_PTR_DELETER(struct_t, deleter_proc) \
	namespace std { \
		template<> \
		class default_delete<struct_t> \
		{ \
		public: \
			void operator()(struct_t *ptr) { \
				deleter_proc(ptr); \
			} \
		}; \
	}

ALLEGRO_PTR_DELETER(ALLEGRO_DISPLAY, al_destroy_display)
ALLEGRO_PTR_DELETER(ALLEGRO_BITMAP, al_destroy_bitmap)
ALLEGRO_PTR_DELETER(ALLEGRO_SAMPLE, al_destroy_sample)

// Use a vector of char for all file I/O (because stupid iostreams API)
using Buffer = std::vector<char>;

// Use unique_ptr to manage the lifetime of ALLEGRO_SAMPLE objects
using DisplayPtr = std::unique_ptr<ALLEGRO_DISPLAY>;
using BitmapPtr = std::unique_ptr<ALLEGRO_BITMAP>;
using SamplePtr = std::unique_ptr<ALLEGRO_SAMPLE>;

// Utility function to open and read the entire [binary] contents
// of a given file into a vector<char> (resizing as necessary)
bool slurp_file(const char *filename, Buffer& dest) {
	std::ifstream file{ filename, std::ios::binary };
	if (!file.seekg(0, std::ios::end)) { return false; }
	auto file_size = file.tellg();
	dest.resize(file_size);
	if (!file.seekg(0, std::ios::beg)) { return false; }
	file.read(&dest[0], dest.size());
	return file.good();	// Have not read PAST EOF, so should still be good...
}

// Abstraction of all resources packed into RESOURCE.BIN
class ResourceBin {
	// Raw backing store (entire file read into memory)
	Buffer data_;

	// Audio samples
	std::vector<SamplePtr> wavs_;

	// Palettes (256 colors each, although palettes 1 and 2 differ only in the 80 colors in the range 64..143)
	std::array<std::array<ALLEGRO_COLOR, 256>, 3> palettes_;
public:
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
		for (size_t i = 0; i < 256; ++i) {
			const char *cp = &data_[offset];
			unsigned char r = static_cast<unsigned char>(cp[0]) * 4;	// Original VGA palette entries are 0-63; we need 0-255
			unsigned char g = static_cast<unsigned char>(cp[1]) * 4;
			unsigned char b = static_cast<unsigned char>(cp[2]) * 4;
			offset += 3;
			palettes_[0][i] = al_map_rgba(r, g, b, (i == 0) ? 0 : 255);	// Color 0 is transparent!
		}

		// Create enemy palettes
		for (size_t p = 1; p < 3; ++p) {
			std::copy(palettes_[0].begin(), palettes_[0].begin() + 64, palettes_[p].begin());			// 0..63 copied
			for (size_t i = 64; i < 144; ++i) {
				const char *cp = &data_[offset];
				unsigned char r = static_cast<unsigned char>(cp[0]) * 4;
				unsigned char g = static_cast<unsigned char>(cp[1]) * 4;
				unsigned char b = static_cast<unsigned char>(cp[2]) * 4;
				offset += 3;
				palettes_[p][i] = al_map_rgba(r, g, b, 255);
			}
			std::copy(palettes_[0].begin() + 144, palettes_[0].end(), palettes_[p].begin() + 144);		// 144..255 copied
		}
	}

	const ALLEGRO_COLOR& color(size_t index, size_t palette=0) const {
		return palettes_.at(palette).at(index);
	}

	ALLEGRO_SAMPLE *sample(size_t index) const {
		return wavs_.at(index).get();
	}

	size_t num_samples() const {
		return NUM_SOUNDS;
	}
};

class SpritesBin {
	// Bitmaps containing all the data from SPRITES.BIN in the 3 available palettes
	std::array<BitmapPtr, 3> sprite_maps_;
public:
	// Must have loaded palette data from RESOURCE.BIN first!
	SpritesBin(const ResourceBin& rsrc, const char *pathToSpritesBin = "SPRITES.BIN") {
		Buffer raw;
		if (!slurp_file(pathToSpritesBin, raw)) {
			throw std::exception("Unable to read data from SPRITES.BIN");
		}

		for (size_t i = 0; i < 3; ++i) {
			sprite_maps_[i].reset(al_create_bitmap(320, 200));
			if (!sprite_maps_[i].get()) {
				throw std::exception("Unable to create bitmap");
			}
			if (!al_lock_bitmap(sprite_maps_[i].get(), ALLEGRO_PIXEL_FORMAT_ANY, ALLEGRO_LOCK_WRITEONLY)) {
				throw std::exception("Unable to lock bitmap");
			}
			al_set_target_bitmap(sprite_maps_[i].get());
			al_clear_to_color(rsrc.color(0, i));
			for (int y = 0; y < 200; ++y) {
				for (int x = 0; x < 320; ++x) {
					size_t offset = (y * 320) + x + 7;	// 7 is BLOAD magic header size;
					if (offset >= raw.size()) { break; }
					unsigned char color = static_cast<unsigned char>(raw[offset]);
					al_put_pixel(x, y, rsrc.color(color, i));
				}
			}
			al_unlock_bitmap(sprite_maps_[i].get());
		}
	}

	ALLEGRO_BITMAP *sprite_map(size_t palette = 0) {
		return sprite_maps_.at(palette).get();
	}
};

int main(int argc, char **argv) {
	startup();

	DisplayPtr dptr{ al_create_display(640, 400) };

	auto t1 = al_get_time();
	ResourceBin rsrc{ "RESOURCE.BIN" };
	auto t2 = al_get_time();
	SpritesBin sprites{ rsrc, "SPRITES.BIN" };
	auto t3 = al_get_time();
	std::cout << "t2 - t1: " << t2 - t1 << std::endl;
	std::cout << "t3 - t2: " << t3 - t2 << std::endl;
	std::cout << "t3 - t1: " << t3 - t1 << std::endl;

	
	if (!al_reserve_samples(rsrc.num_samples())) {
		std::cout << "Failed to reserve " << rsrc.num_samples() << " samples (errno=" << al_get_errno() << ")\n";
		exit(1);
	}

	al_set_target_backbuffer(dptr.get());

	std::cout << "Enter a sample index [0, " << rsrc.num_samples() << "); invalid to quit: ";
	int sound_index = 0;
	size_t pal = 0;
	al_clear_to_color(al_map_rgb(128, 128, 128));
	al_draw_scaled_bitmap(sprites.sprite_map(pal), 0, 0, 16, 16, 50, 50, 100, 100, 0);
	al_flip_display();
	while (std::cin >> sound_index) {
		if ((sound_index < 0) || (sound_index >= (int)rsrc.num_samples())) { break; }
		al_play_sample(rsrc.sample(sound_index), 1.0f, ALLEGRO_AUDIO_PAN_NONE, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
		std::cout << "Enter a sample index [0, " << rsrc.num_samples() << "): ";

		pal = (pal < 2) ? (pal + 1) : 0;
		al_clear_to_color(al_map_rgb(128, 128, 128));
		al_draw_scaled_bitmap(sprites.sprite_map(pal), 0, 0, 320, 200, 0, 0, 640, 400, 0);
		al_flip_display();
	}

	return 0;
}
