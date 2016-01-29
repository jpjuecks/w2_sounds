#pragma once

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

// Enumeration type giving names to all our animation sequences
#include "horror.h"
#define X(Actor, Direction, Action, Mode, ...) \
	ANIM_##Actor##_##Direction##_##Action,
enum ANIMATION {
	MODEL_ACTION_SEQUENCE_TABLE
};
#undef X
#undef ONCE
#undef LOOP

// Type alias for an actor model mapping table node
using sequence_t = const int *;
using actor_model_t = const sequence_t[DIR_MAX][ACTION_MAX];

// Declarations of global animation metadata tables
//-------------------------------------------------
extern const sequence_t ANIMATION_TABLE[];		// Array of pointers to frame-sequences (terminated by a value < 0)
extern const size_t NUM_ANIMATIONS;				// Size of ANIMATION_TABLE in elements
extern const char * const ANIMATION_NAMES[];	// Parallel array of C-strings naming each animation sequence
extern actor_model_t MODEL_TABLE[ACTOR_MAX];	// Array of actor_model_t

// Gets the <tick>th frame number from the sequence <seq>
int compute_frame(sequence_t seq, unsigned int tick);