// Lib C stuff
#include <cstdlib>
#define _USE_MATH_DEFINES
#include <cmath>

// Lib C++ stuff
#include <iostream>
#include <iterator>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>
#include <algorithm>

// Raw Allegro 5 stuff
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>

// Convenience/safety wrappers for Allegro 5
#include "awful.h"
using namespace awful;

// Game-specific headers:
#include "common.h"		// Common typedefs
#include "assets.h"		// Resource loading types
#include "actors.h"		// Animation metadata types/tables
#include "inputs.h"		// Input mechanism abstraction

// SETUP STUFF
//---------------

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

void allegro_die(const char *msg) {
	std::cout << msg << " (errno=" << al_get_errno() << ")\n";
	exit(1);
}

// DISPLAY STUFF
//---------------

static constexpr int DWIDTH = 640, DHEIGHT = 400;

class RenderBuffer {
	BitmapPtr fb_;
public:
	RenderBuffer() : fb_(al_create_bitmap(VGA13_WIDTH, VGA13_HEIGHT)) {
		if (!fb_) { throw std::exception("Unable to create RenderBuffer bitmap"); }
		al_set_target_bitmap(fb_.get());
	}

	void flip(ALLEGRO_DISPLAY *display) {
		al_set_target_backbuffer(display);
		al_draw_scaled_bitmap(fb_.get(), 0, 0, VGA13_WIDTH, VGA13_HEIGHT, 0, 0,
			al_get_display_width(display), al_get_display_height(display), 0);
		al_flip_display();
		al_set_target_bitmap(fb_.get());
	}
};


// Prototype Actor stuff
//-------------------------

struct Position
{
	int x, y;
	int dx, dy;
	int speed;

	Position(int x_, int y_, int speed_) : x(x_), y(y_), dx(0), dy(0), speed(speed_) { }

	void set_delta_from_inputs(const Inputs& input) {
		dx = dy = 0;
		if (input.left()) { dx -= speed; }
		if (input.right()) { dx += speed; }
		if (input.up()) { dy -= speed; }
		if (input.down()) { dy += speed; }
	}

	void update() {
		x += dx;
		y += dy;
	}
};

class Animation {
	const int * seq_;
	unsigned frame_, rate_, ttl_;
public:
	Animation(const int *seq, unsigned rate) : seq_(seq), frame_(0), rate_(rate), ttl_(rate_) {}

	int shape() const {
		return seq_[frame_];
	}

	void advance() {
		// Handle delay between frames (if rate_ != 0)
		if (rate_ && (--ttl_ > 0)) return;

		ttl_ = rate_;
		int next_shape = seq_[++frame_];
		if (next_shape < 0) {
			frame_ += next_shape;
		}
	}

	int shape_advance() {
		int ret = shape();
		advance();
		return ret;
	}

};

class Actor {
public:
	Actor(const actor_model_t& model, unsigned int rate) :
		model_{ &model }, dir_{ DIR_DOWN }, action_{ ACTION_IDLE },
		rate_{ rate }, anim_{ (*model_)[dir_][action_], rate_ } { }

	void set_model(const actor_model_t& model, unsigned int rate = 0) {
		model_ = &model;
		rate_ = rate;
		reset();
	}

	void set_dir(ACTOR_DIRECTION dir) {
		dir_ = dir;
		reset();
	}

	void set_action(ACTOR_ACTION action) {
		action_ = action;
		reset();
	}

	void set_both(ACTOR_DIRECTION dir, ACTOR_ACTION action) {
		dir_ = dir;
		action_ = action;
		reset();
	}

	void set_from_inputs(const Inputs& input) {
		ACTOR_DIRECTION old_dir = dir_;
		ACTOR_ACTION old_action = action_;

		// Assume IDLE action...
		action_ = ACTION_IDLE;

		// Check direction
		if (input.down()) {
			dir_ = DIR_DOWN;
			action_ = ACTION_MOVE;
		} else if (input.left()) {
			dir_ = DIR_LEFT;
			action_ = ACTION_MOVE;
		}
		else if (input.up()) {
			dir_ = DIR_UP;
			action_ = ACTION_MOVE;
		}
		else if (input.right()) {
			dir_ = DIR_RIGHT;
			action_ = ACTION_MOVE;
		}

		// Finally, check "fire" button
		if (input.fire()) {
			action_ = ACTION_FIRE;
		}

		// Reset (including the framecounter) ONLY if something changed
		if ((dir_ != old_dir) || (action_ != old_action)) {
			reset();
		}
	}

	// Forward all animation-specifics to our Animation object
	int shape() const { return anim_.shape(); }
	void advance() { return anim_.advance(); }
	int shape_advance() { return anim_.shape_advance(); }

private:
	void reset() {
		anim_ = Animation{ (*model_)[dir_][action_], rate_ };
	}

	// Actor model data
	const actor_model_t *model_;

	// Actor state
	ACTOR_DIRECTION dir_;
	ACTOR_ACTION action_;

	// Current actor animation sequence
	Animation anim_;
	unsigned int rate_;
};


// Entity/Component/System experiments
//------------------------------------------

// Component: Position on screen (pixel coordinates)
struct CPos {
	int x, y;
};

// Component: Movement on screen (pixel deltas)
struct CMotion {
	int dx, dy;
};

// Component: Raw bitmap to draw on screen
struct CRawBitmap {
	const ALLEGRO_BITMAP *bitmap;
};

// Component: Palette-aware SPRITES.BIN shape
struct CStdShape {
	size_t shape;
};

// Component: an animation sequence
struct CSequence {
	const int * const seq;
	int tick;
};

// Component: Actor/model with orientation
struct CActor {
	const actor_model_t *model;
	ACTOR_DIRECTION dir;
	ACTOR_ACTION actor_
};


struct Entity {
	unsigned int id;	// Arbitrary/unique/opaque entity identifier

	// Bitfields indicating what components are here
	struct {
		bool pos : 1;
		bool motion : 1;
		bool bitmap : 1;
		bool shape : 1;
		bool actor : 1;
	} has;

};



int main(int argc, char **argv) {
	startup();

	EventQueuePtr events{ al_create_event_queue() };
	if (!events) { allegro_die("Unable to create event queue"); }
	al_register_event_source(events.get(), al_get_keyboard_event_source());
	al_register_event_source(events.get(), al_get_mouse_event_source());

	TimerPtr timer{ al_create_timer(1.0 / 60) };
	if (!timer) { allegro_die("Unable to create timer"); }
	al_register_event_source(events.get(), al_get_timer_event_source(timer.get()));
	
	//al_set_new_display_flags(ALLEGRO_FULLSCREEN);
	DisplayPtr dptr{ al_create_display(DWIDTH, DHEIGHT) };
	if (!dptr) { allegro_die("Unable to create display"); }
	al_register_event_source(events.get(), al_get_display_event_source(dptr.get()));

	// Load assets
	ResourceBin rsrc{ "RESOURCE.BIN" };
	SpritesBin sprites{ rsrc, "SPRITES.BIN" };

	if (!al_reserve_samples(rsrc.num_sounds())) { allegro_die("Failed to reserve samples"); }
	
	BitmapPtr bgrd{ bload_image("TITLE.BIN", rsrc.menu_palette()) };
	if (!bgrd) { allegro_die("Unable to BLOAD TITLE.BIN"); }

	Animation figure{ ANIMATION_TABLE[ANIM_BUBBLE_NA_SHOOT], 10 };
	Actor cuby{ MODEL_TABLE[ACTOR_CUBY], 6 };
	Position spot{ VGA13_WIDTH / 2, VGA13_HEIGHT / 2, 1 };
	KeyboardInputs ctrl{ ALLEGRO_KEY_DOWN, ALLEGRO_KEY_LEFT, ALLEGRO_KEY_UP, ALLEGRO_KEY_RIGHT, ALLEGRO_KEY_SPACE };

	RenderBuffer frame_buff;	// All rendering goes here...
	al_start_timer(timer.get());
	bool done = false;
	bool render = true;
	ResourceBin::PALETTE pal = ResourceBin::PAL_DEFAULT;
	while (!done) {
		ALLEGRO_EVENT evt;
		al_wait_for_event(events.get(), &evt);

		// Update controller status based on event
		ctrl.update(evt);
		
		switch (evt.type) {
		case ALLEGRO_EVENT_DISPLAY_CLOSE:
			done = true;
			break;
		case ALLEGRO_EVENT_KEY_DOWN:
			switch (evt.keyboard.keycode) {
			case ALLEGRO_KEY_ESCAPE:
				done = true;
				break;
			case ALLEGRO_KEY_F1:
				cuby.set_model(MODEL_TABLE[ACTOR_CUBY], 6);
				break;
			case ALLEGRO_KEY_F2:
				cuby.set_model(MODEL_TABLE[ACTOR_COBY], 6);
				break;
			case ALLEGRO_KEY_F3:
				cuby.set_model(MODEL_TABLE[ACTOR_BEE], 3);
				break;
			case ALLEGRO_KEY_F4:
				cuby.set_model(MODEL_TABLE[ACTOR_WORM], 3);
				break;
			case ALLEGRO_KEY_F5:
				cuby.set_model(MODEL_TABLE[ACTOR_SHARK], 6);
				break;
			case ALLEGRO_KEY_F6:
				cuby.set_model(MODEL_TABLE[ACTOR_GHOST], 3);
				break;
			case ALLEGRO_KEY_F7:
				cuby.set_model(MODEL_TABLE[ACTOR_PUTTY], 6);
				break;
			case ALLEGRO_KEY_F8:
				cuby.set_model(MODEL_TABLE[ACTOR_MOUSE], 6);
				break;
			case ALLEGRO_KEY_F9:
				cuby.set_model(MODEL_TABLE[ACTOR_PENGUIN], 6);
				break;
			case ALLEGRO_KEY_PGUP:
				break;
			case ALLEGRO_KEY_PGDN:
				break;
			}
			break;
		case ALLEGRO_EVENT_KEY_CHAR:
			switch (evt.keyboard.unichar) {
			case '0':
				pal = ResourceBin::PAL_DEFAULT;
				render = true;
				break;
			case '1':
				pal = ResourceBin::PAL_RED_ENEMIES;
				render = true;
				break;
			case '2':
				pal = ResourceBin::PAL_BLUE_ENEMIES;
				render = true;
				break;
			case '3':
				pal = ResourceBin::PAL_DIM_ENEMIES;
				render = true;
				break;
			case 'a':	// 0
			case 'b':	// 1
			case 'c':	// 2
			case 'd':	// 3
			case 'e':	// 4
			case 'f':	// 5
			case 'g':	// 6
			case 'h':	// 7
			case 'i':	// 8
			case 'j':	// 9
			case 'k':	// 10
			case 'l':	// 11
			case 'm':	// 12
			case 'n':	// 13
			case 'o':	// 14
			case 'p':	// 15
			case 'q':	// 16
			case 'r':	// 17
			case 's':	// 18
				al_play_sample(rsrc.sound_sample(evt.keyboard.unichar - 'a'),
					1.0f, ALLEGRO_AUDIO_PAN_NONE, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
				break;
			}
			break;
		case ALLEGRO_EVENT_TIMER:
			if (evt.timer.source == timer.get()) {
				render = true;
			}
			break;
		}

		if (render && al_is_event_queue_empty(events.get())) {
			// "Control"
			cuby.set_from_inputs(ctrl);
			spot.set_delta_from_inputs(ctrl);

			// "Physics"
			spot.update();

			// "Render"
			al_draw_bitmap(bgrd.get(), 0, 0, 0);
			al_draw_bitmap(sprites.sprite(cuby.shape_advance(), pal), spot.x, spot.y, 0);
			al_draw_bitmap(sprites.sprite(figure.shape_advance(), pal), 0, 0, 0);

			for (float y = 0.5f; y < VGA13_HEIGHT; y += 16.0f) {
				al_draw_line(0.5f, y, VGA13_WIDTH - 0.5f, y, al_map_rgb(0, 0, 255), 1.0f);
			}

			for (float x = 0.5f; x < VGA13_WIDTH; x += 16.0f) {
				al_draw_line(x, 0.5f, x, VGA13_HEIGHT - 0.5f, al_map_rgb(0, 0, 255), 1.0f);
			}

			frame_buff.flip(dptr.get());
			render = false;
		}
	}

	return 0;
}
