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
#include <functional>

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
	unsigned tick_, rate_;
public:
	Animation(const int *seq, unsigned rate = 0) : seq_{ seq }, tick_{ 0 }, rate_{ rate } {}

	int shape() const {
		return compute_frame(seq_, rate_ ? (tick_ / rate_) : tick_);
	}

	void advance() {
		++tick_;
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

using entity_id_t = unsigned int;

// Component: A basic sprite (position on screen, ALLEGRO_BITMAP* to render)
struct CSprite {
	entity_id_t eid;
	float x, y;
	ALLEGRO_BITMAP *bitmap;
	int flags;		// Arbitrary flags used by rendering system to alter sprite's appearance
};

// Component: Palette-sensitive bitmap hack
struct CPalShape {
	entity_id_t eid;
	size_t shape;
};

// Component: Time/lifecycle
struct CTimer {
	entity_id_t eid;
	unsigned int ticks;		// Monotonically increasing "clock" (useful for animation, etc.)
	unsigned int ttl;		// Decreasing (if > 0) "time-to-live" timer; useful for state changes
};

// Component: Screen movement (change in CSprite::x/y and [optionally] CSprite::bitmap based on CTime::ticks)
struct CMotion {
	entity_id_t eid;
	float dx, dy;			// Used to modify CSprite::x and CSprite::y
	sequence_t frames;		// Optional; if not nullptr, used to compute help new CSprite::bitmap
	unsigned int rate;		// Tick divider used to slow down frame transitions
};

enum class GridDirection {
	Down = DIR_DOWN,
	Left = DIR_LEFT,
	Up = DIR_UP,
	Right = DIR_RIGHT
};

std::pair<float, float> direction_delta(GridDirection dir, float scale = 1.0f) {
	switch (dir) {
	case GridDirection::Down:
		return{ 0.f, scale };
	case GridDirection::Left:
		return{ -scale, 0.f };
	case GridDirection::Up:
		return{ 0.f, -scale };
	case GridDirection::Right:
		return{ scale, 0.f };
	default:
		return{ 0.f, 0.f };
	}
}

// Component: Grid-based motion control
struct CGridMoCtrl {
	entity_id_t eid;
	bool busy;			// If true, this entity is moving; if not, it is "at rest" and ready for control input
	bool restless;		// If true, this entity wants to move next chance it gets
	GridDirection dir;	// Gives the direction an entity "wants" to move at its next "at rest"
};

// Component: "Hacks" component for general experimentation
struct CHacks {
	entity_id_t eid;
	
	bool wrap_to_screen;	// If true, wrap this [sprite-equipped] entity to the VGA screen

	Inputs *controller;		// If !nullptr, use this to deduce the intent of our GridMoCtrl (if any)

	actor_model_t* actor;	// If !nullptr, use this to decide which sequence to use for animation
};

// Compare any 2 components (of the same type) to sort on EID
template<typename ComponentType>
bool compare_components(const ComponentType& lhs, const ComponentType& rhs) {
	return lhs.eid < rhs.eid;
}

// Insert a component (by move-assignment) into a vector of that type of component,
// keeping the vector sorted by entity ID (for fast binary search later)
template<typename ComponentType, typename ContainerType = std::vector<ComponentType>>
ComponentType& insert_component(ContainerType& container, ComponentType&& component) {
	auto target = std::upper_bound(container.begin(), container.end(), component, compare_components<ComponentType>);
	auto place = container.insert(target, component);
	return *place;
}

// Look up a component of a given type from a container of the same by entity ID (using binary search)
// If no matching component is found, returns nullptr
template<typename ComponentType, typename ContainerType = std::vector<ComponentType>>
ComponentType* lookup_component(ContainerType& container, entity_id_t eid) {
	auto place = std::lower_bound(container.begin(), container.end(), component compare_components<ComponentType>);
	if ((place == container.end()) || (place->eid != eid)) {
		return nullptr;
	}
	else {
		return &*place;
	}
}

// Typedef/consts for bitmask telling what components are available for a given entity
using component_mask_t = unsigned int;
const component_mask_t HasSprite = 1, HasPalShape = 2, HasTimer = 4, HasMotion = 8, HasGridMoCtrl = 16, HasHacks = 32;

// Fwd decl of master system struct
struct ECS;

struct Entity {
	entity_id_t			id;		// Arbitrary/unique/opaque entity identifier
	component_mask_t	cmask;	// Bitmask of what components this has
	ECS&				sys;	// Reference back to parent system

	Entity(ECS& parent, entity_id_t eid, component_mask_t mask) : id{ eid }, cmask{ mask }, sys{ parent } {}

	bool has_all(component_mask_t mask) {
		return (cmask & mask) == mask;
	}

	bool has_any(component_mask_t mask) {
		return (cmask & mask);
	}

	Entity& add_sprite(float x = 0.f, float y = 0.f, ALLEGRO_BITMAP* bitmap = nullptr, int flags = 0);
	Entity& add_pal_shape(size_t shape = 0);
	Entity& add_timer(unsigned int ttl = 0);
	Entity& add_motion(sequence_t animation = nullptr, unsigned int rate = 1, float dx = 0.0f, float dy = 0.0f);
	Entity& add_grid_mo_ctrl(GridDirection heading = GridDirection::Down, bool is_moving = false);
	Entity& add_hack(bool wrap_to_screen = true, Inputs *controller = nullptr, actor_model_t *model = nullptr);
};

struct ECS {
	// "Global" resources and state used by this system
	const SpritesBin& sprites_bin;
	ResourceBin::PALETTE pal;

	// The official game objects
	std::vector<Entity> entities;

	// This system's current max ID
	entity_id_t eid_seed;

	// The components, stored contiguously
	std::vector<CSprite> sprites;
	std::vector<CPalShape> pal_shapes;
	std::vector<CTimer> timers;
	std::vector<CMotion> movers;
	std::vector<CGridMoCtrl> gmcs;
	std::vector<CHacks> hacks;

	ECS(const SpritesBin& sprites) : sprites_bin{ sprites }, pal{ ResourceBin::PAL_DEFAULT }, eid_seed { 0 } {}

	// The entity list will always be sorted--we always add new entities at the back,
	// and each new entity has an ID greater than all the previous ones (up until rollover--LOL)
	Entity& make_entity() {
		entities.emplace_back(*this, ++eid_seed, 0u);
		return entities.back();
	}

	// Update all timers/movements/etc. (anything "tick" based)
	void update(unsigned int interval = 1);

	// Draw all stuff (no "updates" performed)
	void render();
};

Entity& Entity::add_sprite(float x, float y, ALLEGRO_BITMAP* bitmap, int flags) {
	insert_component<CSprite>(sys.sprites, { id, x, y, bitmap, flags });
	cmask = cmask | HasSprite;
	return *this;
}

Entity& Entity::add_pal_shape(size_t shape) {
	insert_component<CPalShape>(sys.pal_shapes, { id, shape });
	cmask = cmask | HasPalShape;
	return *this;
}

Entity& Entity::add_timer(unsigned int ttl) {
	insert_component<CTimer>(sys.timers, { id, 0u, ttl });
	cmask = cmask | HasTimer;
	return *this;
}

Entity& Entity::add_motion(sequence_t animation, unsigned int rate, float dx, float dy) {
	insert_component<CMotion>(sys.movers, { id, dx, dy, animation, std::max(rate, 1u) });
	cmask = cmask | HasMotion;
	return *this;
}

Entity& Entity::add_grid_mo_ctrl(GridDirection heading, bool is_moving) {
	insert_component<CGridMoCtrl>(sys.gmcs, { id, is_moving, is_moving, heading });
	cmask = cmask | HasGridMoCtrl;
	return *this;
}

Entity& Entity::add_hack(bool wrap_to_screen, Inputs *controller, actor_model_t *model) {
	insert_component<CHacks>(sys.hacks, { id, wrap_to_screen, controller, model });
	cmask = cmask | HasHacks;
	return *this;
}

void ECS::update(unsigned int interval) {
	auto isprite = sprites.begin();
	auto ishape = pal_shapes.begin();
	auto itimer = timers.begin();
	auto imover = movers.begin();
	auto igmc = gmcs.begin();
	auto ihack = hacks.begin();

	for (auto& e : entities) {
		// Find this entity's components (if this entity doesn't HAVE a particular kind of component, that's OK)
		while ((isprite != sprites.end()) && (isprite->eid < e.id)) ++isprite;
		while ((ishape != pal_shapes.end()) && (ishape->eid < e.id)) ++ishape;
		while ((itimer != timers.end()) && (itimer->eid < e.id)) ++itimer;
		while ((imover != movers.end()) && (imover->eid < e.id)) ++imover;
		while ((igmc != gmcs.end()) && (igmc->eid < e.id)) ++igmc;
		while ((ihack != hacks.end()) && (ihack->eid < e.id)) ++ihack;

		if (e.has_any(HasTimer)) {
			// DEBUG ASSERT
			assert((itimer != timers.end()) && (itimer->eid == e.id));

			// TIMER UPDATE SYSTEM
			itimer->ticks += interval;
			itimer->ttl = (interval >= itimer->ttl) ? 0 : itimer->ttl - interval;
		}

		if (e.has_all(HasSprite | HasPalShape)) {
			// DEBUG ASSERT
			assert((isprite != sprites.end()) && (ishape != pal_shapes.end()) && (isprite->eid == e.id) && (ishape->eid == e.id));

			// PAL SHAPE UPDATE SYSTEM
			isprite->bitmap = sprites_bin.sprite(ishape->shape, pal);
		}

		// GRID BASED MOTION CONTROL
		if (e.has_all(HasSprite | HasMotion | HasGridMoCtrl)) {
			assert((isprite != sprites.end()) && (imover != movers.end()) && (igmc != gmcs.end()));
			assert((isprite->eid == e.id) && (imover->eid == e.id) && (igmc->eid == e.id));

			// Check for an actor system (currently shoe-horned into CHacks
			if (e.has_any(HasHacks)) {
				assert((ihack != hacks.end()) && (ihack->eid == e.id));
			
				if (ihack->actor) {
					imover->frames = (*ihack->actor)[(ACTOR_DIRECTION)igmc->dir][(igmc->busy) ? ACTION_MOVE : ACTION_IDLE];
				}
			}

			if (igmc->busy) {
				// Should we "stop" moving (i.e., switch to a rest state where we can accept a new direction
				if ((int(isprite->x) & 0xf) == 0) { imover->dx = 0; }
				if ((int(isprite->y) & 0xf) == 0) { imover->dy = 0; }
				if ((imover->dx == 0) && (imover->dy == 0)) {
					igmc->busy = false;
				}
			}

			if (!igmc->busy && igmc->restless) {
				// Should we "start" moving?
				std::tie(imover->dx, imover->dy) = direction_delta(igmc->dir, 1.f);
				igmc->busy = true;

				// If this item had a timer, reset its tick for crisper looking animation
				if (e.has_any(HasTimer)) {
					assert((itimer != timers.end()) && (itimer->eid == e.id));
					itimer->ticks = 0u;
				}
			}
		}

		if (e.has_all(HasSprite | HasMotion)) {
			// DEBUG ASSERT
			assert((isprite != sprites.end()) && (imover != movers.end()));
			assert((isprite->eid == e.id) && (imover->eid == e.id));

			// MOVING SPRITE UPDATE SYSTEM
			isprite->x += imover->dx * interval;
			isprite->y += imover->dy * interval;
			if (imover->frames && e.has_any(HasTimer)) {
				assert((itimer != timers.end()) && (itimer->eid == e.id));

				isprite->bitmap = sprites_bin.sprite(compute_frame(imover->frames, itimer->ticks / imover->rate), pal);
			}
		}

		// GENERAL PURPOSE HACKS
		if (e.has_all(HasSprite | HasHacks)) {
			assert((isprite != sprites.end()) && (ihack != hacks.end()));
			assert((isprite->eid == e.id) && (ihack->eid == e.id));

			// Wrap to screen?
			if (ihack->wrap_to_screen) {
				if (isprite->x < 0.f) {
					isprite->x += VGA13_WIDTH;
				} else if (isprite->x > VGA13_WIDTH) {
					isprite->x -= VGA13_WIDTH;
				}
				if (isprite->y < 0.f) {
					isprite->y += VGA13_HEIGHT;
				}
				else if (isprite->y > VGA13_HEIGHT) {
					isprite->y -= VGA13_HEIGHT;
				}
			}

			// Drive grid mo control with a given [abstract] game controller
			if (ihack->controller && e.has_any(HasGridMoCtrl)) {
				assert((igmc != gmcs.end()) && (igmc->eid == e.id));

				igmc->restless = true;
				if (ihack->controller->left()) {
					igmc->dir = GridDirection::Left;
				} else if(ihack->controller->right()) {
					igmc->dir = GridDirection::Right;
				} else if (ihack->controller->up()) {
					igmc->dir = GridDirection::Up;
				} else if (ihack->controller->down()) {
					igmc->dir = GridDirection::Down;
				} else {
					igmc->restless = false;
				}
			}
		}
	}
}

void ECS::render() {
	for (auto& s : sprites) {
		if (s.bitmap) {
			al_draw_bitmap(s.bitmap, s.x, s.y, 0);
		}

		// DEBUG HACKS
		if (s.flags) {
			unsigned char r = (s.flags & 4) ? 255 : 0;
			unsigned char g = (s.flags & 2) ? 255 : 0;
			unsigned char b = (s.flags & 1) ? 255 : 0;
			al_draw_rectangle(s.x + 0.5f, s.y + 0.5f, s.x + 15.5f, s.y + 15.5f, al_map_rgb(r, g, b), 1.0f);
		}
	}
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

	KeyboardInputs ctrl{ ALLEGRO_KEY_DOWN, ALLEGRO_KEY_LEFT, ALLEGRO_KEY_UP, ALLEGRO_KEY_RIGHT, ALLEGRO_KEY_SPACE };

	/*Animation figure{ ANIMATION_TABLE[ANIM_BUBBLE_NA_SHOOT], 10 };
	Actor cuby{ MODEL_TABLE[ACTOR_CUBY], 6 };
	Position spot{ VGA13_WIDTH / 2, VGA13_HEIGHT / 2, 1 };*/

	ECS ecs{ sprites };

	// Add one palette-indepedent sprite
	ecs.make_entity().add_sprite(16.0f * 10, 16.0f * 6, sprites.sprite(207), 7).add_motion().add_grid_mo_ctrl().add_hack(true, &ctrl);
	
	// One palette-dependent
	ecs.make_entity().add_sprite(16.0f * 5, 16.0f * 3, nullptr, 5).add_pal_shape(94);

	// One looping animation (moving right)
	ecs.make_entity().add_sprite(0.0f, 0.0f, nullptr, 4).add_timer().add_motion(ANIMATION_TABLE[ANIM_BUBBLE_NA_SHOOT], 10, 1.0f, 0.f).add_hack(true);

	// Try to make Cuby!
	auto cuby_id = ecs.make_entity().add_sprite().add_motion(nullptr, 4u).add_grid_mo_ctrl().add_timer().add_hack(true, &ctrl, &MODEL_TABLE[ACTOR_CUBY]).id;

	RenderBuffer frame_buff;	// All rendering goes here...
	al_start_timer(timer.get());
	bool done = false;
	bool render = true;
	//ResourceBin::PALETTE pal = ResourceBin::PAL_DEFAULT;
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
			/*case ALLEGRO_KEY_F1:
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
				break;*/
			}
			break;
		case ALLEGRO_EVENT_KEY_CHAR:
			switch (evt.keyboard.unichar) {
			case '0':
				ecs.pal = ResourceBin::PAL_DEFAULT;
				render = true;
				break;
			case '1':
				ecs.pal = ResourceBin::PAL_RED_ENEMIES;
				render = true;
				break;
			case '2':
				ecs.pal = ResourceBin::PAL_BLUE_ENEMIES;
				render = true;
				break;
			case '3':
				ecs.pal = ResourceBin::PAL_DIM_ENEMIES;
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
			//cuby.set_from_inputs(ctrl);
			//spot.set_delta_from_inputs(ctrl);

			// "Physics"
			//spot.update();
			ecs.update();

			// "Render"
			al_draw_bitmap(bgrd.get(), 0, 0, 0);
			//al_draw_bitmap(sprites.sprite(cuby.shape_advance(), pal), spot.x, spot.y, 0);
			//al_draw_bitmap(sprites.sprite(figure.shape_advance(), pal), 0, 0, 0);

			ecs.render();

			for (float y = 0.5f; y < VGA13_HEIGHT; y += 16.0f) {
				al_draw_line(0.5f, y, VGA13_WIDTH - 0.5f, y, al_map_rgba_f(0.5f, 0.5f, 0.5f, 0.25f), 1.0f);
			}

			for (float x = 0.5f; x < VGA13_WIDTH; x += 16.0f) {
				al_draw_line(x, 0.5f, x, VGA13_HEIGHT - 0.5f, al_map_rgba_f(0.5f, 0.5f, 0.5f, 0.25f), 1.0f);
			}

			frame_buff.flip(dptr.get());
			render = false;
		}
	}

	return 0;
}
