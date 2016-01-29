#include <array>

#include "actors.h"

// constexpr function templated on a variadic list of integer values
// returns a constexpr std::array<int, ?> containing:
// * [0]: total number of frames
// * [1]: number of frames to loop (1 means loop on last frame, 2 means loop on last 2 frames, etc.)
// * [2..?]: frame shape numbers
template<int... frame_numbers>
constexpr std::array<int, 2 + sizeof...(frame_numbers)> make_seq(int loop_count = 0) {
	return{
		static_cast<int>(sizeof...(frame_numbers)),
		(loop_count == 0) ? static_cast<int>(sizeof...(frame_numbers)) : loop_count,
		frame_numbers... };
}

// Bring on the horror of X macros!
#include "horror.h"

// Define an array of animation sequence frame numbers (terminated by -1)
#define X(Actor, Direction, Action, Mode, ...) \
	static auto Actor##_##Direction##_##Action = make_seq<__VA_ARGS__>(Mode);
MODEL_ACTION_SEQUENCE_TABLE
#undef X

// Define a master array of pointers to the raw sequence data
#define X(Actor, Direction, Action, Mode, ...) \
	Actor##_##Direction##_##Action.data(),
const sequence_t ANIMATION_TABLE[] = {
	MODEL_ACTION_SEQUENCE_TABLE
};
const size_t NUM_ANIMATIONS = sizeof(ANIMATION_TABLE) / sizeof(ANIMATION_TABLE[0]);
#undef X

// Define a master table of animation name strings (for debugging)
#define X(Actor, Direction, Action, Mode, ...) \
	#Actor "_" #Direction  "_" #Action,
const char * const ANIMATION_NAMES[] = {
	MODEL_ACTION_SEQUENCE_TABLE
};
#undef X
#undef LOOP
#undef ONCE

// Master actor model mapping table (mapping modelID -> direction -> action -> ptr to sequence
actor_model_t MODEL_TABLE[ACTOR_MAX] = {
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

int compute_frame(sequence_t seq, unsigned int tick) {
	const unsigned int total = static_cast<unsigned int>(seq[0]),
		loop = static_cast<unsigned int>(seq[1]);
	sequence_t frames = &seq[2];

	if (tick < total) {
		return frames[tick];
	}
	else {
		auto offset = total - loop;
		return frames[((tick - offset) % loop) + offset];
	}
}