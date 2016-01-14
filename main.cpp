#include <cstdlib>

#define _USE_MATH_DEFINES
#include <cmath>

#include <iostream>
#include <iterator>
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

// Use the safety wrappers in AWFUL
#include "awful.h"
using namespace awful;

// Use common types
#include "common.h"

// Asset management code
#include "assets.h"

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

	void set_dx(float dx) { dx_ = dx; }
	void set_dy(float dy) { dy_ = dy; }

	float x() const { return x_; }
	float y() const { return y_; }
	void set_x(float x) { x_ = x; }
	void set_y(float y) { y_ = y; }

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

static constexpr int DWIDTH = 640, DHEIGHT = 400;

void allegro_die(const char *msg) {
	std::cout << msg << " (errno=" << al_get_errno() << ")\n";
	exit(1);
}

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

static constexpr size_t MAX_SEQUENCE_FRAMES{ 8 }, MAX_FRAME_SHAPE{ 239 };

// Define enum for actor ID
enum ACTOR_MODEL {
	ACTOR_CUBY,
	ACTOR_COBY,
	ACTOR_BEE,
	ACTOR_WORM,
	ACTOR_SHARK,
	ACTOR_GHOST,
	ACTOR_PUTTY,
	ACTOR_MOUSE,
	ACTOR_PENGUIN,
	ACTOR_MAX
};

// Define enum for direction
enum ACTOR_DIRECTION {
	DIR_DOWN,
	DIR_LEFT,
	DIR_UP,
	DIR_RIGHT,
	DIR_MAX
};

// Define enum for actions
enum ACTOR_ACTION {
	ACTION_IDLE,
	ACTION_MOVE,
	ACTION_FIRE,
	ACTION_MAX
};

// constexpr function templated on a variadic list of integer values
// returns a constexpr std::array containing all the values plus a trailer
// [negative] value that controls what happens when the end of the sequence
// is reached:
//	* if looped == true (default), the trailer value will restart the sequence
//	* if looped == false, the trailer value will "park" the animation on its final frame
template<int... frame_numbers>
constexpr std::array<int, sizeof...(frame_numbers)+1> make_seq(bool looped = true) {
	return{ frame_numbers..., looped ? -static_cast<int>(sizeof...(frame_numbers)) : -1 };
}

// Define the master data table of all animation frame sequence numbers for various actors/directions/actions
// as a macro that uses the currently-undefined function macro X().
#undef X
#define LOOP true
#define ONCE false
#define MODEL_ACTION_SEQUENCE_TABLE \
	/* Cuby and Coby are cononical actors */ \
	X(CUBY, DOWN, IDLE, ONCE, 1) \
	X(CUBY, DOWN, MOVE, LOOP, 1, 2, 1, 0) \
	X(CUBY, DOWN, FIRE, ONCE, 3) \
	X(CUBY, LEFT, IDLE, ONCE, 5) \
	X(CUBY, LEFT, MOVE, LOOP, 5, 6, 5, 4) \
	X(CUBY, LEFT, FIRE, ONCE, 7) \
	X(CUBY, UP, IDLE, ONCE, 9) \
	X(CUBY, UP, MOVE, LOOP, 9, 10, 9, 8) \
	X(CUBY, UP, FIRE, ONCE, 11) \
	X(CUBY, RIGHT, IDLE, ONCE, 13) \
	X(CUBY, RIGHT, MOVE, LOOP, 13, 14, 13, 12) \
	X(CUBY, RIGHT, FIRE, ONCE, 15) \
	X(CUBY, NA, YAHOO, LOOP, 16, 17, 18) \
	X(COBY, DOWN, IDLE, ONCE, 21) \
	X(COBY, DOWN, MOVE, LOOP, 21, 22, 21, 20) \
	X(COBY, DOWN, FIRE, ONCE, 23) \
	X(COBY, LEFT, IDLE, ONCE, 25) \
	X(COBY, LEFT, MOVE, LOOP, 25, 26, 25, 24) \
	X(COBY, LEFT, FIRE, ONCE, 27) \
	X(COBY, UP, IDLE, ONCE, 29) \
	X(COBY, UP, MOVE, LOOP, 29, 30, 29, 28) \
	X(COBY, UP, FIRE, ONCE, 31) \
	X(COBY, RIGHT, IDLE, ONCE, 33) \
	X(COBY, RIGHT, MOVE, LOOP, 33, 34, 33, 32) \
	X(COBY, RIGHT, FIRE, ONCE, 35) \
	X(COBY, NA, YAHOO, ONCE, 36, 37, 38) \
	/* Bees have no real idle/fire sequences */ \
	X(BEE, DOWN, NA, ONCE, 41) \
	X(BEE, DOWN, MOVE, LOOP, 41, 42, 41, 40) \
	X(BEE, LEFT, NA, ONCE, 44) \
	X(BEE, LEFT, MOVE, LOOP, 44, 45, 44, 43) \
	X(BEE, UP, NA, ONCE, 47) \
	X(BEE, UP, MOVE, LOOP, 47, 48, 47, 46) \
	X(BEE, RIGHT, NA, ONCE, 50) \
	X(BEE, RIGHT, MOVE, LOOP, 50, 51, 50, 49) \
	/* Worms have no idle/fire sequences; just LONG movement sequences! */ \
	X(WORM, NA, NA, ONCE, 60) \
	X(WORM, DOWN, MOVE, LOOP, 52, 53, 54, 55, 56, 57, 58, 59) \
	X(WORM, LEFT, MOVE, LOOP, 60, 67, 66, 65, 64, 63, 62, 61) \
	X(WORM, UP, MOVE, LOOP, 59, 58, 57, 56, 55, 54, 53, 52) \
	X(WORM, RIGHT, MOVE, LOOP, 60, 61, 62, 63, 64, 65, 66, 67) \
	/* Sharks have no idle/fire sequences (they also move diagonally;
		"DOWN" = SW, "LEFT" = NW, "UP" = NE, "RIGHT" = SE */ \
	X(SHARK, DOWN, NA, ONCE, 69) \
	X(SHARK, DOWN, MOVE, LOOP, 69, 70, 69, 68) \
	X(SHARK, LEFT, NA, ONCE, 72) \
	X(SHARK, LEFT, MOVE, LOOP, 72, 73, 72, 71) \
	X(SHARK, UP, NA, ONCE, 75) \
	X(SHARK, UP, MOVE, LOOP, 75, 76, 75, 74) \
	X(SHARK, RIGHT, NA, LOOP, 78) \
	X(SHARK, RIGHT, MOVE, LOOP, 78, 79, 78, 77) \
	/* Ghosts are canonical actors (except for having no dedicated IDLE stance */ \
	X(GHOST, DOWN, MOVE, ONCE, 80) \
	X(GHOST, DOWN, FIRE, ONCE, 80, 81, 82) \
	X(GHOST, LEFT, MOVE, ONCE, 83) \
	X(GHOST, LEFT, FIRE, ONCE, 83, 84, 85) \
	X(GHOST, UP, MOVE, ONCE, 86) \
	X(GHOST, UP, FIRE, ONCE, 86, 87, 88) \
	X(GHOST, RIGHT, MOVE, ONCE, 89) \
	X(GHOST, RIGHT, FIRE, ONCE, 89, 90, 91)


	// Define an array of animation sequence frame numbers (terminated by -1)
#define X(Actor, Direction, Action, Mode, ...) \
	static auto Actor##_##Direction##_##Action = make_seq<__VA_ARGS__>(Mode);
MODEL_ACTION_SEQUENCE_TABLE
#undef X

#undef LOOP
#undef ONCE

// Master actor model mapping table (mapping modelID -> direction -> action -> ptr to sequence
using actor_model_t = const int * const[DIR_MAX][ACTION_MAX];
static actor_model_t actor_models[] = {
	{	// ACTOR_CUBY
		{ CUBY_DOWN_IDLE.data(), CUBY_DOWN_MOVE.data(), CUBY_DOWN_FIRE.data() },
		{ CUBY_LEFT_IDLE.data(), CUBY_LEFT_MOVE.data(), CUBY_LEFT_FIRE.data() },
		{ CUBY_UP_IDLE.data(), CUBY_UP_MOVE.data(), CUBY_UP_FIRE.data() },
		{ CUBY_RIGHT_IDLE.data(), CUBY_RIGHT_MOVE.data(), CUBY_RIGHT_FIRE.data() },
	},
	{	// ACTOR_COBY
		{ COBY_DOWN_IDLE.data(), COBY_DOWN_MOVE.data(), COBY_DOWN_FIRE.data() },
		{ COBY_LEFT_IDLE.data(), COBY_LEFT_MOVE.data(), COBY_LEFT_FIRE.data() },
		{ COBY_UP_IDLE.data(), COBY_UP_MOVE.data(), COBY_UP_FIRE.data() },
		{ COBY_RIGHT_IDLE.data(), COBY_RIGHT_MOVE.data(), COBY_RIGHT_FIRE.data() },
	},
	{	// ACTOR_BEE (movement only)
		{ BEE_DOWN_NA.data(), BEE_DOWN_MOVE.data(), BEE_DOWN_NA.data() },
		{ BEE_LEFT_NA.data(), BEE_LEFT_MOVE.data(), BEE_LEFT_NA.data() },
		{ BEE_UP_NA.data(), BEE_UP_MOVE.data(), BEE_UP_NA.data() },
		{ BEE_RIGHT_NA.data(), BEE_RIGHT_MOVE.data(), BEE_RIGHT_NA.data() },
	},
	{	// ACTOR_WORM (movement only)
		{ WORM_NA_NA.data(), WORM_DOWN_MOVE.data(), WORM_NA_NA.data() },
		{ WORM_NA_NA.data(), WORM_LEFT_MOVE.data(), WORM_NA_NA.data() },
		{ WORM_NA_NA.data(), WORM_UP_MOVE.data(), WORM_NA_NA.data() },
		{ WORM_NA_NA.data(), WORM_RIGHT_MOVE.data(), WORM_NA_NA.data() },
	},
	{	// ACTOR_WORM (movement only [and that's diagnoal movement.data(), unfortunately])
		{ SHARK_DOWN_NA.data(), SHARK_DOWN_MOVE.data(), SHARK_DOWN_NA.data() },
		{ SHARK_LEFT_NA.data(), SHARK_LEFT_MOVE.data(), SHARK_LEFT_NA.data() },
		{ SHARK_UP_NA.data(), SHARK_UP_MOVE.data(), SHARK_UP_NA.data() },
		{ SHARK_RIGHT_NA.data(), SHARK_RIGHT_MOVE.data(), SHARK_RIGHT_NA.data() },
	},
	{	// ACTOR_GHOST
		{ GHOST_DOWN_MOVE.data(), GHOST_DOWN_MOVE.data(), GHOST_DOWN_FIRE.data() },
		{ GHOST_LEFT_MOVE.data(), GHOST_LEFT_MOVE.data(), GHOST_LEFT_FIRE.data() },
		{ GHOST_UP_MOVE.data(), GHOST_UP_MOVE.data(), GHOST_UP_FIRE.data() },
		{ GHOST_RIGHT_MOVE.data(), GHOST_RIGHT_MOVE.data(), GHOST_RIGHT_FIRE.data() },
	},
};

class Actor {
public:
	Actor(const actor_model_t& model, int rate = 0) :
		model_(&model), dir_(DIR_DOWN), action_(ACTION_IDLE),
		seq_(nullptr), ttl_(0), rate_(rate), frame_(0)
	{
		reset();
	}

	void set_rate(int rate) {
		rate_ = rate;
		ttl_ = rate;
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

	int next_frame() {
		// Framerate delay check
		if (rate_ && (--ttl_ > 0)) {
			// Still counting down; give them the last frame...
			return seq_[frame_];
		}
		else {
			ttl_ = rate_;
			int next = seq_[++frame_];
			if (next < 0) {
				// Hit the end; reset frame_ to -1 and try again
				frame_ = (rate_ ? 0 : -1);
				return next_frame();
			}
			return next;
		}
	}

private:
	void reset() {
		seq_ = (*model_)[dir_][action_];
		frame_ = (rate_ ? 0 : -1);
		ttl_ = rate_;
	}

	const actor_model_t *model_;

	ACTOR_DIRECTION dir_;
	ACTOR_ACTION action_;

	const int *seq_;
	int rate_;
	int ttl_;
	int frame_;
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

	/*SpriteObj crab{ sprites.sprite(93), VGA13_WIDTH / 2.0f, VGA13_HEIGHT / 2.0f };
	crab.add_frame(sprites.sprite(92));
	crab.add_frame(sprites.sprite(93));
	crab.add_frame(sprites.sprite(94));
	crab.animate(10);*/

	Actor cuby{ actor_models[ACTOR_CUBY], 6 };

	RenderBuffer frame_buff;	// All rendering goes here...
	al_start_timer(timer.get());
	bool done = false;
	bool render = true;
	ResourceBin::PALETTE pal = ResourceBin::PAL_DEFAULT;
	while (!done) {
		ALLEGRO_EVENT evt;
		al_wait_for_event(events.get(), &evt);
		
		switch (evt.type) {
		case ALLEGRO_EVENT_DISPLAY_CLOSE:
			done = true;
			break;
		case ALLEGRO_EVENT_KEY_DOWN:
			switch (evt.keyboard.keycode) {
			case ALLEGRO_KEY_LEFT:
				//crab.set_dx(-1.0f);
				cuby.set_both(DIR_LEFT, ACTION_MOVE);
				break;
			case ALLEGRO_KEY_RIGHT:
				//crab.set_dx(1.0f);
				cuby.set_both(DIR_RIGHT, ACTION_MOVE);
				break;
			case ALLEGRO_KEY_UP:
				//crab.set_dy(-1.0f);
				cuby.set_both(DIR_UP, ACTION_MOVE);
				break;
			case ALLEGRO_KEY_DOWN:
				//crab.set_dy(1.0f);
				cuby.set_both(DIR_DOWN, ACTION_MOVE);
				break;
			case ALLEGRO_KEY_ESCAPE:
				done = true;
				break;
			case ALLEGRO_KEY_F1:
				cuby = Actor{ actor_models[ACTOR_CUBY], 6 };
				break;
			case ALLEGRO_KEY_F2:
				cuby = Actor{ actor_models[ACTOR_COBY], 6 };
				break;
			case ALLEGRO_KEY_F3:
				cuby = Actor{ actor_models[ACTOR_BEE], 3 };
				break;
			case ALLEGRO_KEY_F4:
				cuby = Actor{ actor_models[ACTOR_WORM], 3 };
				break;
			case ALLEGRO_KEY_F5:
				cuby = Actor{ actor_models[ACTOR_SHARK], 6 };
				break;
			case ALLEGRO_KEY_F6:
				cuby = Actor{ actor_models[ACTOR_GHOST], 3 };
				break;
			}
			break;
		case ALLEGRO_EVENT_KEY_UP:
			switch (evt.keyboard.keycode) {
			case ALLEGRO_KEY_LEFT:
			case ALLEGRO_KEY_RIGHT:
				//crab.set_dx(0);
				//break;
			case ALLEGRO_KEY_UP:
			case ALLEGRO_KEY_DOWN:
				//crab.set_dy(0);
				cuby.set_action(ACTION_IDLE);
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
				//crab.update();
				render = true;
			}
			break;
		}

		if (render && al_is_event_queue_empty(events.get())) {
			al_draw_bitmap(bgrd.get(), 0, 0, 0);
			//al_draw_bitmap(sprites.sprite_map(pal), 0, 0, 0);
			//crab.render();
			al_draw_bitmap(sprites.sprite(cuby.next_frame(), pal), 160, 100, 0);
			frame_buff.flip(dptr.get());
			render = false;
		}
	}

	return 0;
}
