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

static constexpr size_t MAX_SEQUENCE_FRAMES{ 8 };

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

// Define the master data table of all animation frame sequence numbers for various actors/directions/actions
// as a macro that uses the currently-undefined function macro X().
#undef X
#define ANIMATION_SEQUENCE_TABLE \
	X(CUBY, DOWN, IDLE, 1) \
	X(CUBY, DOWN, MOVE, 1, 2, 1, 0) \
	X(CUBY, DOWN, FIRE, 3) \
	X(CUBY, LEFT, IDLE, 5) \
	X(CUBY, LEFT, MOVE, 5, 6, 5, 4) \
	X(CUBY, LEFT, FIRE, 7) \
	X(CUBY, UP, IDLE, 9) \
	X(CUBY, UP, MOVE, 9, 10, 9, 8) \
	X(CUBY, UP, FIRE, 11) \
	X(CUBY, RIGHT, IDLE, 13) \
	X(CUBY, RIGHT, MOVE, 13, 14, 13, 12) \
	X(CUBY, RIGHT, FIRE, 15) \
	X(COBY, DOWN, IDLE, 21) \
	X(COBY, DOWN, MOVE, 21, 22, 21, 20) \
	X(COBY, DOWN, FIRE, 23) \
	X(COBY, LEFT, IDLE, 25) \
	X(COBY, LEFT, MOVE, 25, 26, 25, 24) \
	X(COBY, LEFT, FIRE, 27) \
	X(COBY, UP, IDLE, 29) \
	X(COBY, UP, MOVE, 29, 30, 29, 28) \
	X(COBY, UP, FIRE, 31) \
	X(COBY, RIGHT, IDLE, 33) \
	X(COBY, RIGHT, MOVE, 33, 34, 33, 32) \
	X(COBY, RIGHT, FIRE, 35)

// Define an array of animation sequence frame numbers (terminated by -1)
using frame_seq_t = const int[MAX_SEQUENCE_FRAMES + 1];
#define X(Actor, Direction, Action, ...) { __VA_ARGS__, -1 },
static frame_seq_t actor_frame_sequences[] = {
	ANIMATION_SEQUENCE_TABLE
};
#undef X

// Define a parallel enum of animation sequence IDs
#define X(Actor, Direction, Action, ...) Actor##_##Direction##_##Action, 
enum ACTOR_SEQUENCE {
	ANIMATION_SEQUENCE_TABLE
};
#undef X

// Master actor model mapping table (mapping modelID -> direction -> action -> ptr to sequence
using actor_model_t = frame_seq_t * const[DIR_MAX][ACTION_MAX];
static actor_model_t actor_models[] = {
	{
		{ &actor_frame_sequences[CUBY_DOWN_IDLE], &actor_frame_sequences[CUBY_DOWN_MOVE], &actor_frame_sequences[CUBY_DOWN_FIRE] },
		{ &actor_frame_sequences[CUBY_LEFT_IDLE], &actor_frame_sequences[CUBY_LEFT_MOVE], &actor_frame_sequences[CUBY_LEFT_FIRE] },
		{ &actor_frame_sequences[CUBY_UP_IDLE], &actor_frame_sequences[CUBY_UP_MOVE], &actor_frame_sequences[CUBY_UP_FIRE] },
		{ &actor_frame_sequences[CUBY_RIGHT_IDLE], &actor_frame_sequences[CUBY_RIGHT_MOVE], &actor_frame_sequences[CUBY_RIGHT_FIRE] },
	},
	{
		{ &actor_frame_sequences[COBY_DOWN_IDLE], &actor_frame_sequences[COBY_DOWN_MOVE], &actor_frame_sequences[COBY_DOWN_FIRE] },
		{ &actor_frame_sequences[COBY_LEFT_IDLE], &actor_frame_sequences[COBY_LEFT_MOVE], &actor_frame_sequences[COBY_LEFT_FIRE] },
		{ &actor_frame_sequences[COBY_UP_IDLE], &actor_frame_sequences[COBY_UP_MOVE], &actor_frame_sequences[COBY_UP_FIRE] },
		{ &actor_frame_sequences[COBY_RIGHT_IDLE], &actor_frame_sequences[COBY_RIGHT_MOVE], &actor_frame_sequences[COBY_RIGHT_FIRE] },
	},
};

class Actor {
public:
	Actor(const actor_model_t& model) :
		model_(model), dir_(DIR_DOWN), action_(ACTION_IDLE),
		seq_(nullptr), ttl_(0), rate_(0), frame_(0)
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
			return (*seq_)[frame_];
		}
		else {
			ttl_ = rate_;
			int next = (*seq_)[++frame_];
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
		seq_ = model_[dir_][action_];
		frame_ = (rate_ ? 0 : -1);
	}

	const actor_model_t &model_;

	ACTOR_DIRECTION dir_;
	ACTOR_ACTION action_;

	frame_seq_t *seq_;
	int rate_;
	int ttl_;
	int frame_;
};

int main(int argc, char **argv) {
	std::cout << "sizeof(actor_models) -> " << sizeof(actor_models) << std::endl;
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

	Actor cuby{ actor_models[ACTOR_COBY] };
	cuby.set_rate(10);

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
			al_draw_bitmap(sprites.sprite(cuby.next_frame()), 160, 100, 0);
			frame_buff.flip(dptr.get());
			render = false;
		}
	}

	return 0;
}
