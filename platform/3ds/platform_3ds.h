/*************************************************************************/
/*  platform_3ds.h                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifndef PLATFORM_3DS_H
#define PLATFORM_3DS_H

#include "object.h"
#include "dictionary.h"

// Script singleton "Platform3DS": 3DS-specific queries exposed to GDScript.
class Platform3DS : public Object {

	OBJ_TYPE(Platform3DS, Object);

	static Platform3DS *singleton;

	mutable uint32_t heap_peak;
	mutable uint32_t linear_peak;
	mutable uint32_t vram_peak;
	bool cpu_speedup;

	// Frame timing, smoothed (exponential moving average), in milliseconds.
	float frame_ms, logic_ms, draw_ms, gpu_wait_ms, vblank_ms;

protected:
	static void _bind_methods();

public:
	static Platform3DS *get_singleton() { return singleton; }

	// Bytes currently allocated with malloc/new (main application heap).
	uint32_t get_heap_used() const;
	uint32_t get_heap_size() const;

	// Samples all counters and updates the peaks. Cheap enough to call once per second.
	void update_peaks() const;
	uint32_t get_heap_peak() const;

	// Keys: heap_used, heap_size, heap_peak, linear_used, linear_size, linear_peak,
	// vram_used, vram_size, vram_peak, app_region_used, is_new_3ds.
	Dictionary get_memory_info() const;

	bool is_new_3ds() const;

	// New 3DS only: 804 MHz + L2 cache when enabled (default), 268 MHz like an Old 3DS when not.
	void set_cpu_speedup(bool p_enabled);
	bool get_cpu_speedup() const;

	// Optimization level the engine was built with (scons optimize=...): "max", "speed" or "size".
	String get_build_optimization() const;

	// Microsecond timer (OS.get_ticks_usec is not exposed to scripts in 2.1).
	uint64_t get_ticks_usec() const;

	enum {
		SCREEN_NONE = 0,
		SCREEN_TOP = 1,
		SCREEN_BOTTOM = 2,
		SCREEN_BOTH = 3,
	};

	// Dual screen mode: draw canvas layer p_layer (0 = the world, CanvasLayer nodes = their
	// "layer") only on these screens. Default: both.
	void set_canvas_layer_screens(int p_layer, int p_screens);
	int get_canvas_layer_screens(int p_layer) const;

	// Where the frame time goes (ms, smoothed): frame_ms, logic_ms (scripts, physics, scene),
	// draw_ms (visual server + renderer, CPU), gpu_wait_ms (waiting for the GPU to finish the
	// previous frame), vblank_ms (waiting for the screen refresh), plus the last frame's
	// draw_calls, vertices, items (canvas items drawn) and cmdbuf_percent.
	Dictionary get_frame_stats() const;
	// Called by OS_3DS after each frame.
	void record_frame(uint64_t p_frame_ticks, uint64_t p_vblank_ticks);

	Platform3DS();
	~Platform3DS();
};

#endif // PLATFORM_3DS_H
