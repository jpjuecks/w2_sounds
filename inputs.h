#pragma once
#include <allegro5/events.h>

// Abstraction of a player input (directions and "fire")
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


// Concrete Inputs subclass driven by Allegro keyboard events
class KeyboardInputs : public Inputs {
	// Allegro key-code mappings for the movements requested
	int key_down_, key_left_, key_up_, key_right_, key_fire_;
public:
	KeyboardInputs(int key_down, int key_left, int key_up, int key_right, int key_fire) :
		key_down_(key_down), key_left_(key_left), key_up_(key_up), key_right_(key_right), key_fire_(key_fire)
	{}

	void update(const ALLEGRO_EVENT& ev) override;
};
