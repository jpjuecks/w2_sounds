// Define the master data table of all animation frame sequence numbers for various actors/directions/actions
// as a macro that uses the currently-undefined function macro X().
#undef X
#define LOOP (0)
#define ONCE (1)
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
	/* Sharks have no idle/fire sequences (they also move diagonally) */ \
	/*	"DOWN" = SW, "LEFT" = NW, "UP" = NE, "RIGHT" = SE */ \
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
	X(PENGUIN, RIGHT, FIRE, ONCE, 135) \
	/* Non-actor animations of various sorts */ \
	X(BEE, NA, DEAD, LOOP, 136, 143, 150, 157) \
	X(WORM, NA, DEAD, LOOP, 137, 144, 151, 158) \
	X(SHARK, NA, DEAD, LOOP, 138, 145, 152, 159) \
	X(GHOST, NA, DEAD, LOOP, 139, 146, 153, 160) \
	X(PUTTY, NA, DEAD, LOOP, 140, 147, 154, 161) \
	X(MOUSE, NA, DEAD, LOOP, 141, 148, 155, 162) \
	X(PENGUIN, NA, DEAD, LOOP, 142, 149, 156, 163) \
	X(BUBBLE, NA, SHOOT, 4, 164, 164, 165, 166, 167, 168, 169, 170, 169) \
	X(BUBBLE, NA, POP, ONCE, 173, 174, 175) \
	X(FLUBBER, NA, SHOOT, LOOP, 176, 177, 178, 179) \
	X(SNOWBALL, NA, SHOOT, LOOP, 180, 181, 182, 183) \
	X(SNOWBALL, NA, POP, ONCE, 184, 185, 186) \
	X(ANY, NA, POP, ONCE, 187, 188, 189) \
	X(STARS, NA, POP, ONCE, 223, 224, 225) \
	X(FLAME, NA, SPIN, LOOP, 226, 227, 228, 229) \
	X(DUST, NA, POP, ONCE, 230, 231, 232)
