#include "inputs.h"

void KeyboardInputs::update(const ALLEGRO_EVENT& ev) {
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