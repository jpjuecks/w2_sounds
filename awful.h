#pragma once
#ifndef AWFUL_H
#define AWFUL_H

// We use std::unique_ptr
#include <memory>

// And lots of Allegro structures
#include <allegro5/allegro.h>

// Macro-driven template specialization: used to have ALLEGRO_XXX ptr objectes "deleted" with al_destroy_xxx
#define ALLEGRO_PTR_DELETER(struct_t, deleter_proc) \
namespace std { \
	template<> \
	class default_delete<struct_t> \
	{ \
	public: \
		void operator()(struct_t *ptr) { \
			deleter_proc(ptr); \
		} \
	}; \
}

// Deleters for standard types
ALLEGRO_PTR_DELETER(ALLEGRO_DISPLAY, al_destroy_display)
ALLEGRO_PTR_DELETER(ALLEGRO_BITMAP, al_destroy_bitmap)
ALLEGRO_PTR_DELETER(ALLEGRO_FILE, al_fclose)
ALLEGRO_PTR_DELETER(ALLEGRO_FS_ENTRY, al_destroy_fs_entry)
ALLEGRO_PTR_DELETER(ALLEGRO_EVENT_QUEUE, al_destroy_event_queue)
ALLEGRO_PTR_DELETER(ALLEGRO_TIMER, al_destroy_timer)


// For audio types (opt-out with proper define)
#ifndef AWFUL_NO_AUDIO
#include <allegro5/allegro_audio.h>
ALLEGRO_PTR_DELETER(ALLEGRO_SAMPLE, al_destroy_sample)
#endif

// Define new C++ types inside a namespace
namespace awful {
	
	// Use unique_ptrs to manage the lifetime of various objects
	// (These are convenience aliases)
	using DisplayPtr = std::unique_ptr<ALLEGRO_DISPLAY>;
	using BitmapPtr = std::unique_ptr<ALLEGRO_BITMAP>;
	using FilePtr = std::unique_ptr<ALLEGRO_FILE>;
	using FsEntryPtr = std::unique_ptr<ALLEGRO_FS_ENTRY>;
	using EventQueuePtr = std::unique_ptr<ALLEGRO_EVENT_QUEUE>;
	using TimerPtr = std::unique_ptr<ALLEGRO_TIMER>;

	// Audio types are opt-out
#ifndef AWFUL_NO_AUDIO
	using SamplePtr = std::unique_ptr<ALLEGRO_SAMPLE>;
#endif

	// RAII type for managing temporary changes of Allegro 5's target bitmap
	class TempTargetBitmap {
		ALLEGRO_BITMAP		*original_bitmap_;		// To be restored
		ALLEGRO_TRANSFORM	original_projection_;	// Because this gets reset!
	public:
		explicit TempTargetBitmap(ALLEGRO_BITMAP *new_target) :
			original_bitmap_(al_get_target_bitmap()),
			original_projection_(*al_get_current_projection_transform())
		{
			al_set_target_bitmap(new_target);
		}

		// No copy/move/assign
		TempTargetBitmap(const TempTargetBitmap& other) = delete;
		TempTargetBitmap(TempTargetBitmap&& other) = delete;
		TempTargetBitmap& operator=(const TempTargetBitmap& other) = delete;

		// Auto restore target bitmap (with transforms) on cleanup
		~TempTargetBitmap() {
			al_set_target_bitmap(original_bitmap_);
			al_use_projection_transform(&original_projection_);
		}
	};

}

#endif
