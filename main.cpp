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
#include <tuple>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <utility>

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

// Typedef and invalid value for an opaque entity ID
using entity_id_t = unsigned int;
constexpr entity_id_t INVALID_EID = 0u;

// Typedef for global game clock
using tick_t = unsigned int;

// Typedef for bitmask telling what components are available for a given entity
using component_mask_t = unsigned int;



// Component base: contains the parent entity ID and operator< to sort the component based on that ID
struct Component {
	entity_id_t eid;

	explicit Component(entity_id_t eid_) : eid{ eid_ } {}

	bool operator<(const Component& that) const {
		return eid < that.eid;
	}
};

// Component: A basic sprite (screen position and rendering info)
struct CSprite : public Component {
	static constexpr component_mask_t Mask = 1;

	ALLEGRO_BITMAP *bitmap;
	float x, y;
	int flags;		// Arbitrary flags used by rendering system to alter sprite's appearance

	CSprite(entity_id_t eid_, ALLEGRO_BITMAP *bitmap_ = nullptr, float x_ = 0.0f, float y_ = 0.0f, int flags_ = 0) :
		Component{ eid_ }, bitmap{ bitmap_}, x{ x_ }, y{ y_ }, flags{ flags_ } {}
};

// Component: Animation (metadata for a animating sprite)
struct CAnimation : public Component {
	static constexpr component_mask_t Mask = 2;

	// Time base for frame selection (if any) and wobble (if any)
	tick_t tbase;

	// Animation frames (sequence of SpritesBin shapes and clock divider for frame rate)
	sequence_t seq;
	int rate;

	// Palette effect (useful for enemies only)
	ResourceBin::PALETTE pal;
	
	// "Wobble" on the y axis (to be implemented)
	float wamp;	// max positive amplitude
	int wper;	// period of an up/down cycle in frame ticks

	CAnimation(entity_id_t eid_, sequence_t seq_ = nullptr, int rate_ = 1, ResourceBin::PALETTE pal_ = ResourceBin::PAL_DEFAULT, float wamp_ = 0.0f, int wper_ = 0) :
		Component{ eid_ }, tbase{ 0u }, seq{ seq_ }, rate{ rate_ }, pal{ pal_ }, wamp{ wamp_ }, wper{ wper_ } {}

};

// Component: Actor (multi-direction/action animated model)
struct CActor : public Component {
	static constexpr component_mask_t Mask = 4;

	actor_model_t *model;
	ACTOR_DIRECTION dir;
	ACTOR_ACTION action;

	CActor(entity_id_t eid_, actor_model_t *model_ = nullptr, ACTOR_DIRECTION dir_ = DIR_DOWN, ACTOR_ACTION action_ = ACTION_IDLE) :
		Component{ eid }, model { model_ }, dir{ dir_ }, action{ action_ } {}
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

// Component: Grid mover (dynamic entity whose movement is constrained by the 16x16 grid)
struct CGridMover : public Component {
	static constexpr component_mask_t Mask = 8;

	bool moving;				// TRUE if the entity is moving
	float dx, dy;				// Actual screen motion deltas
	ACTOR_DIRECTION cur_dir;	// Actual facing direction of current movement (useful for Actors)

	// Control intent indicators
	bool			should_move;
	GridDirection	move_dir;
	float			move_scale;

	CGridMover(entity_id_t eid_, bool moving_ = false, bool should_move_ = false, GridDirection move_dir_ = GridDirection::Down, float move_scale_ = 1.0f) :
		Component{ eid_ }, moving{ moving_}, dx{ 0.0f }, dy{ 0.0f }, cur_dir{ (ACTOR_DIRECTION)move_dir_ },
		should_move{ should_move_ }, move_dir{ move_dir_ }, move_scale{ move_scale_ } {}
};

// Component: "Hacks" component for general experimentation
struct CHacks : public Component {
	static constexpr component_mask_t Mask = 16;

	bool wrap_to_screen;	// If true, wrap this [sprite-equipped] entity to the VGA screen
	Inputs *controller;		// If !nullptr, use this to deduce the intent of our GridMoCtrl (if any)

	CHacks(entity_id_t eid_, bool wrap_to_screen_ = false, Inputs *controller_ = nullptr) :
		Component{ eid_ }, wrap_to_screen{ wrap_to_screen_ }, controller{ controller_ } {}
};

// Insert a component (by move-assignment) into a vector of that type of component,
// keeping the vector sorted by entity ID (for fast binary search later)
template<typename ComponentType, typename ContainerType = std::vector<ComponentType>>
ComponentType& insert_component(ContainerType& container, ComponentType&& component) {
	auto target = std::upper_bound(container.begin(), container.end(), component);
	auto place = container.insert(target, component);
	return *place;
}

// Look up a component of a given type from a container of the same by entity ID (using binary search)
// If no matching component is found, returns nullptr
template<typename ComponentType, typename ContainerType = std::vector<ComponentType>>
ComponentType* lookup_component(ContainerType& container, entity_id_t eid) {
	auto place = std::lower_bound(container.begin(), container.end(), component);
	if ((place == container.end()) || (place->eid != eid)) {
		return nullptr;
	}
	else {
		return &*place;
	}
}

// Compile-time-recursive foreach-tuple implementation inspired by (http://stackoverflow.com/questions/1198260/iterate-over-tuple/6894436#6894436)
template<size_t Index, typename Func, typename... Pack>
inline typename std::enable_if<Index == sizeof...(Pack)>::type tuple_foreach(std::tuple<Pack...> tup, Func fun) {} // Terminal case (no-op)

template<size_t Index, typename Func, typename... Pack>
inline typename std::enable_if<Index < sizeof...(Pack)>::type tuple_foreach(std::tuple<Pack...> tup, Func fun) {
	fun(std::get<Index>(tup));							// Invoke payload...
	tuple_foreach<Index + 1, Func, Pack...>(tup, fun);	// ...and recurse
}


// Advance an iterator-to-Component-collection until it hits the end or an entity ID >= the target
// (Returns TRUE if it hit the target, FALSE otherwise)
template<typename ComponentType, typename IteratorType = std::vector<ComponentType>::iterator>
bool sync_iterator(entity_id_t eid, IteratorType& iter, const IteratorType& end) {
	while ((iter != end) && (iter->eid < eid)) { ++iter; }
	return (iter == end) ? false : (iter->eid == eid);
}

// An entity/component framework that supports a given list of component types (all subtypes of Component)
template<typename... ComponentTypes>
struct ECS {

	struct Entity {
		entity_id_t				id;		// Arbitrary/unique/opaque entity identifier
		component_mask_t		cmask;	// Bitmask of what components this has
		ECS<ComponentTypes...>&	sys;	// Reference back to parent system

		Entity(ECS& parent, entity_id_t eid) : id{ eid }, cmask{ 0u }, sys{ parent } {}

		bool has_all(component_mask_t mask) {
			return (cmask & mask) == mask;
		}

		bool has_any(component_mask_t mask) {
			return (cmask & mask);
		}

		// Construct and instance of type ComponentType with the given arguments and insert
		// it into the corresponding vector-of-ComponenntType we have in our parent ECS system...
		template<typename ComponentType, typename... Args>
		Entity& add(Args&&... args) {
			std::vector<ComponentType>& collection = std::get<std::vector<ComponentType>>(sys.components);
			insert_component(collection, ComponentType{ id, std::forward<Args>(args)... });
			cmask |= ComponentType::Mask;
			return *this;
		}
	};

	// This system's current max ID
	entity_id_t eid_seed;

	// The official game objects
	std::vector<Entity> entities;

	// And the vectors of components that make them up
	std::tuple<std::vector<ComponentTypes>...> components;

	ECS() : eid_seed { 0 } {}

	// The entity list will always be sorted--we always add new entities at the back,
	// and each new entity has an ID greater than all the previous ones (up until rollover--LOL)
	Entity& make_entity() {
		entities.emplace_back(*this, ++eid_seed);
		return entities.back();
	}

	template<typename ComponentType>
	std::vector<ComponentType>& get_components() {
		return std::get<std::vector<ComponentType>>(components);
	}

	// TODO: use some magic template-fu to make a generic
	// "iterate over all entities with X components and call
	// this callable passing in references to those components"


	// Drive user-control of grid movers
	void sys_user_controls() {
		std::vector<CGridMover>&	movers = get_components<CGridMover>();
		std::vector<CHacks>&		hacks = get_components<CHacks>();

		auto imover = movers.begin();
		auto ihack = hacks.begin();

		// For each entity...
		for (Entity& e : entities) {
			if (e.has_all(CGridMover::Mask | CHacks::Mask) && sync_iterator<CGridMover>(e.id, imover, movers.end()) && sync_iterator<CHacks>(e.id, ihack, hacks.end())) {
				CGridMover& mover = *imover;
				CHacks& hack = *ihack;

				if (hack.controller) {
					if (hack.controller->left()) {
						mover.move_dir = GridDirection::Left;
						mover.should_move = true;
					}
					else if (hack.controller->right()) {
						mover.move_dir = GridDirection::Right;
						mover.should_move = true;
					}
					else if (hack.controller->up()) {
						mover.move_dir = GridDirection::Up;
						mover.should_move = true;
					}
					else if (hack.controller->down()) {
						mover.move_dir = GridDirection::Down;
						mover.should_move = true;
					}
					else {
						mover.should_move = false;
					}
				}
			}
		}
	}

	// Drive grid-locked motion
	void sys_grid_moves() {
		std::vector<CSprite>&		sprites = get_components<CSprite>();
		std::vector<CGridMover>&	movers = get_components<CGridMover>();

		auto isprite = sprites.begin();
		auto imover = movers.begin();

		// For each entity...
		for (Entity& e : entities) {
			if (e.has_all(CSprite::Mask | CGridMover::Mask) && sync_iterator<CSprite>(e.id, isprite, sprites.end()) && sync_iterator<CGridMover>(e.id, imover, movers.end())) {
				CSprite& sprite = *isprite;
				CGridMover& mover = *imover;

				if (mover.moving) {
					// Move!
					sprite.x += mover.dx;
					sprite.y += mover.dy;

					// Have we entered a rest position?
					if ((int(sprite.x) % 16 == 0) && (int(sprite.y) % 16 == 0)) {
						// End busy mode (and stop moving)...
						mover.moving = false;
						mover.dx = mover.dy = 0.0f;
					}
				}
				else {
					// We are open to changing state
					if (mover.should_move) {
						mover.cur_dir = (ACTOR_DIRECTION)mover.move_dir;
						std::tie(mover.dx, mover.dy) = direction_delta(mover.move_dir, mover.move_scale);
						mover.moving = true;
					}
				}
			}
		}
	}

	// Sync grid motion with actor orientation/action
	void sys_grid_actors() {

	}

	// Drive animations
	void sys_animate(tick_t game_clock, const SpritesBin& sprite_data) {
		auto& animats = get_components<CAnimation>();
		auto& sprites = get_components<CSprite>();

		auto ianimat = animats.begin();
		auto isprite = sprites.begin();
		
		// For each entity...
		for (Entity& e : entities) {
			if (e.has_all(CAnimation::Mask | CSprite::Mask) && sync_iterator<CSprite>(e.id, isprite, sprites.end()) && sync_iterator<CAnimation>(e.id, ianimat, animats.end())) {
				CSprite& sprite = *isprite;
				CAnimation& animat = *ianimat;

				// Update the SPRITE's bitmap based on the computed frame (and known palette) of ANIMATION
				auto clock = (game_clock - animat.tbase) / animat.rate;
				sprite.bitmap = sprite_data.sprite(compute_frame(animat.seq, clock), animat.pal);
			}
		}
	}

	// Walk all CSprite components and render them
	void sys_render() {
		for (CSprite& s : get_components<CSprite>()) {
			if (s.bitmap) {
				al_draw_bitmap(s.bitmap, s.x, s.y, 0);

				// DEBUG HACKS
				if (s.flags) {
					unsigned char r = (s.flags & 4) ? 255 : 0;
					unsigned char g = (s.flags & 2) ? 255 : 0;
					unsigned char b = (s.flags & 1) ? 255 : 0;
					al_draw_rectangle(
						s.x + 0.5f,
						s.y + 0.5f,
						s.x + al_get_bitmap_width(s.bitmap),
						s.y + al_get_bitmap_height(s.bitmap),
						al_map_rgb(r, g, b), 1.0f);
				}
			}
		}
	}
};


/*void ECS::update(unsigned int interval) {
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
*/

int main(int argc, char **argv) {
	startup();

	EventQueuePtr events{ al_create_event_queue() };
	if (!events) { allegro_die("Unable to create event queue"); }
	al_register_event_source(events.get(), al_get_keyboard_event_source());
	al_register_event_source(events.get(), al_get_mouse_event_source());

	TimerPtr timer{ al_create_timer(1.0 / 64) };
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

	// Create an E/C manager for our given component types
	ECS<CSprite, CAnimation, CActor, CGridMover, CHacks> ecs;

	ecs.make_entity().add<CSprite>(bgrd.get());
	ecs.make_entity().add<CSprite>(nullptr, 16.f * 3, 16.f * 10, 2).add<CAnimation>(ANIMATION_TABLE[ANIM_WORM_RIGHT_MOVE], 8);
	ecs.make_entity().add<CSprite>(sprites.sprite(207), 16.f * 10, 16.f * 6, 4).add<CGridMover>().add<CHacks>(true, &ctrl);


	//ecs.make_entity().add_sprite(16.0f * 10, 16.0f * 6, sprites.sprite(207), 7).add_motion().add_grid_mo_ctrl().add_hack(true, &ctrl);
	
	// One palette-dependent
	//ecs.make_entity().add_sprite(16.0f * 5, 16.0f * 3, nullptr, 5).add_pal_shape(94);

	// One looping animation (moving right)
	//ecs.make_entity().add_sprite(0.0f, 0.0f, nullptr, 4).add_timer().add_motion(ANIMATION_TABLE[ANIM_BUBBLE_NA_SHOOT], 10, 1.0f, 0.f).add_hack(true);

	// Try to make Cuby!
	//auto cuby_id = ecs.make_entity().add_sprite().add_motion(nullptr, 4u).add_grid_mo_ctrl().add_timer().add_hack(true, &ctrl, &MODEL_TABLE[ACTOR_CUBY]).id;

	RenderBuffer frame_buff;	// All rendering goes here...
	al_start_timer(timer.get());
	bool done = false;
	bool render = true;
	tick_t game_clock = 0u;

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
				//ecs.pal = ResourceBin::PAL_DEFAULT;
				render = true;
				break;
			case '1':
				//ecs.pal = ResourceBin::PAL_RED_ENEMIES;
				render = true;
				break;
			case '2':
				//ecs.pal = ResourceBin::PAL_BLUE_ENEMIES;
				render = true;
				break;
			case '3':
				//ecs.pal = ResourceBin::PAL_DIM_ENEMIES;
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
				++game_clock;
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
			//ecs.update();

			// "Render"
			
			//al_draw_bitmap(sprites.sprite(cuby.shape_advance(), pal), spot.x, spot.y, 0);
			//al_draw_bitmap(sprites.sprite(figure.shape_advance(), pal), 0, 0, 0);

			//ecs.render();

			ecs.sys_user_controls();
			ecs.sys_grid_moves();
			ecs.sys_animate(game_clock, sprites);
			ecs.sys_render();
			

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
