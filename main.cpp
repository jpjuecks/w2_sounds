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
	X(GHOST, RIGHT, FIRE, ONCE, 89, 90, 91) \
	/* Putties are canonical actors (except for no real idle) */ \
	X(PUTTY, DOWN, IDLE, ONCE, 93) \
	X(PUTTY, DOWN, MOVE, LOOP, 93, 94, 93, 92) \
	X(PUTTY, DOWN, FIRE, ONCE, 104) \
	X(PUTTY, LEFT, IDLE, ONCE, 96) \
	X(PUTTY, LEFT, MOVE, LOOP, 96, 97, 96, 95) \
	X(PUTTY, LEFT, FIRE, ONCE, 105) \
	X(PUTTY, UP, IDLE, ONCE, 99) \
	X(PUTTY, UP, MOVE, LOOP, 99, 100, 99, 98) \
	X(PUTTY, UP, FIRE, ONCE, 106) \
	X(PUTTY, RIGHT, IDLE, ONCE, 102) \
	X(PUTTY, RIGHT, MOVE, LOOP, 102, 103, 102, 101) \
	X(PUTTY, RIGHT, FIRE, ONCE, 107) \
	/* Like bees, mice have no real idle/fire sequences */ \
	X(MOUSE, DOWN, NA, ONCE, 109) \
	X(MOUSE, DOWN, MOVE, LOOP, 109, 110, 109, 108) \
	X(MOUSE, LEFT, NA, ONCE, 112) \
	X(MOUSE, LEFT, MOVE, LOOP, 112, 113, 112, 111) \
	X(MOUSE, UP, NA, ONCE, 115) \
	X(MOUSE, UP, MOVE, LOOP, 115, 116, 115, 114) \
	X(MOUSE, RIGHT, NA, ONCE, 118) \
	X(MOUSE, RIGHT, MOVE, LOOP, 118, 119, 118, 117) \
	/* Penguins are canonical actors (except for no real idle) */ \
	X(PENGUIN, DOWN, IDLE, ONCE, 121) \
	X(PENGUIN, DOWN, MOVE, LOOP, 121, 122, 121, 120) \
	X(PENGUIN, DOWN, FIRE, ONCE, 132) \
	X(PENGUIN, LEFT, IDLE, ONCE, 124) \
	X(PENGUIN, LEFT, MOVE, LOOP, 124, 125, 124, 123) \
	X(PENGUIN, LEFT, FIRE, ONCE, 133) \
	X(PENGUIN, UP, IDLE, ONCE, 127) \
	X(PENGUIN, UP, MOVE, LOOP, 127, 128, 127, 126) \
	X(PENGUIN, UP, FIRE, ONCE, 134) \
	X(PENGUIN, RIGHT, IDLE, ONCE, 130) \
	X(PENGUIN, RIGHT, MOVE, LOOP, 130, 131, 130, 129) \
	X(PENGUIN, RIGHT, FIRE, ONCE, 135)

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
	{	// ACTOR_PUTTY
		{ PUTTY_DOWN_IDLE.data(), PUTTY_DOWN_MOVE.data(), PUTTY_DOWN_FIRE.data() },
		{ PUTTY_LEFT_IDLE.data(), PUTTY_LEFT_MOVE.data(), PUTTY_LEFT_FIRE.data() },
		{ PUTTY_UP_IDLE.data(), PUTTY_UP_MOVE.data(), PUTTY_UP_FIRE.data() },
		{ PUTTY_RIGHT_IDLE.data(), PUTTY_RIGHT_MOVE.data(), PUTTY_RIGHT_FIRE.data() },
	},
	{	// ACTOR_MOUSE (movement only)
		{ MOUSE_DOWN_NA.data(), MOUSE_DOWN_MOVE.data(), MOUSE_DOWN_NA.data() },
		{ MOUSE_LEFT_NA.data(), MOUSE_LEFT_MOVE.data(), MOUSE_LEFT_NA.data() },
		{ MOUSE_UP_NA.data(), MOUSE_UP_MOVE.data(), MOUSE_UP_NA.data() },
		{ MOUSE_RIGHT_NA.data(), MOUSE_RIGHT_MOVE.data(), MOUSE_RIGHT_NA.data() },
	},
	{	// ACTOR_PENGUIN
		{ PENGUIN_DOWN_IDLE.data(), PENGUIN_DOWN_MOVE.data(), PENGUIN_DOWN_FIRE.data() },
		{ PENGUIN_LEFT_IDLE.data(), PENGUIN_LEFT_MOVE.data(), PENGUIN_LEFT_FIRE.data() },
		{ PENGUIN_UP_IDLE.data(), PENGUIN_UP_MOVE.data(), PENGUIN_UP_FIRE.data() },
		{ PENGUIN_RIGHT_IDLE.data(), PENGUIN_RIGHT_MOVE.data(), PENGUIN_RIGHT_FIRE.data() },
	},
};

class Inputs {
protected:
	// Protected concrete storage (can be manipulated by subclasses
	bool down_, left_, up_, right_, fire_;

public:
	Inputs() : down_(false), left_(false), up_(false), right_(false), fire_(false) { }

	// Concrete getters for input state
	bool down() const { return down_; }
	bool left() const { return left_; }
	bool up() const { return up_; }
	bool right() const { return right_; }
	bool fire() const { return fire_; }

	// Virtual ALLEGRO_EVENT handler
	virtual void update(const ALLEGRO_EVENT& ev) = 0;
};

class Actor {
public:
	Actor(const actor_model_t& model, int rate = 0) :
		model_(&model), dir_(DIR_DOWN), action_(ACTION_IDLE),
		seq_(nullptr), ttl_(0), rate_(rate), frame_(0)
	{
		reset();
	}

	void set_model(const actor_model_t& model, int rate = 0) {
		model_ = &model;
		rate_ = rate;
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


private:
	void reset() {
		seq_ = (*model_)[dir_][action_];
		frame_ = 0;
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

// Translates Allegro keyboard events into Inputs state
class KeyboardInputs : public Inputs {
	// Allegro key-code mappings for the movements requested
	int key_down_, key_left_, key_up_, key_right_, key_fire_;
public:
	KeyboardInputs(int key_down, int key_left, int key_up, int key_right, int key_fire) :
		key_down_(key_down), key_left_(key_left), key_up_(key_up), key_right_(key_right), key_fire_(key_fire)
	{}

	void update(const ALLEGRO_EVENT& ev) override {
		if ((ev.type != ALLEGRO_EVENT_KEY_DOWN) && (ev.type != ALLEGRO_EVENT_KEY_UP)) return;

		bool state = (ev.type == ALLEGRO_EVENT_KEY_DOWN) ? true : false;
		if (ev.keyboard.keycode == key_down_) {
			down_ = state;
		}
		else if (ev.keyboard.keycode == key_left_) {
			left_ = state;
		}
		else if (ev.keyboard.keycode == key_up_) {
			up_ = state;
		}
		else if (ev.keyboard.keycode == key_right_) {
			right_ = state;
		}
		else if (ev.keyboard.keycode == key_fire_) {
			fire_ = state;
		}
	}
};

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

	Actor cuby{ actor_models[ACTOR_CUBY], 6 };
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
				cuby.set_model(actor_models[ACTOR_CUBY], 6);
				break;
			case ALLEGRO_KEY_F2:
				cuby.set_model(actor_models[ACTOR_COBY], 6);
				break;
			case ALLEGRO_KEY_F3:
				cuby.set_model(actor_models[ACTOR_BEE], 3);
				break;
			case ALLEGRO_KEY_F4:
				cuby.set_model(actor_models[ACTOR_WORM], 3);
				break;
			case ALLEGRO_KEY_F5:
				cuby.set_model(actor_models[ACTOR_SHARK], 6);
				break;
			case ALLEGRO_KEY_F6:
				cuby.set_model(actor_models[ACTOR_GHOST], 3);
				break;
			case ALLEGRO_KEY_F7:
				cuby.set_model(actor_models[ACTOR_PUTTY], 6);
				break;
			case ALLEGRO_KEY_F8:
				cuby.set_model(actor_models[ACTOR_MOUSE], 6);
				break;
			case ALLEGRO_KEY_F9:
				cuby.set_model(actor_models[ACTOR_PENGUIN], 6);
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
			frame_buff.flip(dptr.get());
			render = false;
		}
	}

	return 0;
}
