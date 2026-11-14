/*************************************************************************/
/*  platform_3ds.cpp                                                     */
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
#include "platform_3ds.h"

#include <malloc.h>
#include "os/os.h"
#include "drivers/3ds/citro3d/rasterizer_citro3d.h"

extern "C" {
#include <3ds/types.h>
#include <3ds/os.h>
#include <3ds/env.h>
#include <3ds/allocator/linear.h>
#include <3ds/allocator/vram.h>
#include <3ds/services/apt.h>
}

// VRAM managed by libctru's vram allocator (two 3 MB banks).
#define VRAM_TOTAL_SIZE 0x600000

Platform3DS *Platform3DS::singleton = NULL;

uint32_t Platform3DS::get_heap_used() const {
	struct mallinfo mi = mallinfo();
	return mi.uordblks;
}

uint32_t Platform3DS::get_heap_size() const {
	return envGetHeapSize();
}

void Platform3DS::update_peaks() const {
	uint32_t heap = get_heap_used();
	uint32_t linear = envGetLinearHeapSize() - linearSpaceFree();
	uint32_t vram = VRAM_TOTAL_SIZE - vramSpaceFree();
	if (heap > heap_peak) heap_peak = heap;
	if (linear > linear_peak) linear_peak = linear;
	if (vram > vram_peak) vram_peak = vram;
}

uint32_t Platform3DS::get_heap_peak() const {
	update_peaks();
	return heap_peak;
}

Dictionary Platform3DS::get_memory_info() const {
	update_peaks();

	Dictionary d;
	d["heap_used"] = (int)get_heap_used();
	d["heap_size"] = (int)get_heap_size();
	d["heap_peak"] = (int)heap_peak;
	d["linear_used"] = (int)(envGetLinearHeapSize() - linearSpaceFree());
	d["linear_size"] = (int)envGetLinearHeapSize();
	d["linear_peak"] = (int)linear_peak;
	d["vram_used"] = (int)(VRAM_TOTAL_SIZE - vramSpaceFree());
	d["vram_size"] = (int)VRAM_TOTAL_SIZE;
	d["vram_peak"] = (int)vram_peak;
	// Everything the app holds: code + data + malloc heap + linear heap + stacks.
	// (The region *size* reported by the kernel config page is not reliable under emulators.)
	d["app_region_used"] = (int)osGetMemRegionUsed(MEMREGION_APPLICATION);
	d["is_new_3ds"] = is_new_3ds();
	return d;
}

bool Platform3DS::is_new_3ds() const {
	bool n3ds = false;
	APT_CheckNew3DS(&n3ds);
	return n3ds;
}

void Platform3DS::set_cpu_speedup(bool p_enabled) {
	osSetSpeedupEnable(p_enabled);
	cpu_speedup = p_enabled;
}

bool Platform3DS::get_cpu_speedup() const {
	return cpu_speedup;
}

String Platform3DS::get_build_optimization() const {
#ifdef GODOT_3DS_OPTIMIZE
	return GODOT_3DS_OPTIMIZE;
#else
	return "unknown";
#endif
}

uint64_t Platform3DS::get_ticks_usec() const {
	return OS::get_singleton()->get_ticks_usec();
}

void Platform3DS::set_canvas_layer_screens(int p_layer, int p_screens) {
	RasterizerCitro3d *r = RasterizerCitro3d::get_singleton();
	ERR_FAIL_COND(!r);
	r->set_layer_screens(p_layer, p_screens);
}

int Platform3DS::get_canvas_layer_screens(int p_layer) const {
	RasterizerCitro3d *r = RasterizerCitro3d::get_singleton();
	return r ? r->get_layer_screens(p_layer) : SCREEN_BOTH;
}

static inline float _ticks_to_ms(uint64_t p_ticks) {
	return p_ticks / (SYSCLOCK_ARM11 / 1000.0f);
}

void Platform3DS::record_frame(uint64_t p_frame_ticks, uint64_t p_vblank_ticks) {
	RasterizerCitro3d *r = RasterizerCitro3d::get_singleton();
	if (!r)
		return;
	const RasterizerCitro3d::FrameStats &fs = r->last_frame_stats;
	float frame = _ticks_to_ms(p_frame_ticks);
	float gpu = _ticks_to_ms(fs.gpu_wait_ticks);
	float draw = _ticks_to_ms(fs.draw_ticks);
	float vblank = _ticks_to_ms(p_vblank_ticks);
	float end = _ticks_to_ms(fs.end_ticks);
	float logic = MAX(frame - gpu - draw - vblank, 0.0f);
	const float a = 0.1f;
	frame_ms += (frame - frame_ms) * a;
	logic_ms += (logic - logic_ms) * a;
	draw_ms += (draw - draw_ms) * a;
	frame_end_ms += (end - frame_end_ms) * a;
	// Worst frame of the last second (the averages hide single-frame hitches).
	if (++peak_frames >= 60) {
		peak_frames = 0;
		frame_max_ms = cur_frame_max;
		logic_max_ms = cur_logic_max;
		draw_max_ms = cur_draw_max;
		cur_frame_max = cur_logic_max = cur_draw_max = 0;
	}
	cur_frame_max = MAX(cur_frame_max, frame - vblank);
	cur_logic_max = MAX(cur_logic_max, logic);
	cur_draw_max = MAX(cur_draw_max, draw);
	gpu_wait_ms += (gpu - gpu_wait_ms) * a;
	vblank_ms += (vblank - vblank_ms) * a;
}

Dictionary Platform3DS::get_frame_stats() const {
	Dictionary d;
	d["frame_ms"] = frame_ms;
	d["logic_ms"] = logic_ms;
	d["draw_ms"] = draw_ms;
	d["frame_end_ms"] = frame_end_ms;
	d["frame_max_ms"] = frame_max_ms; // worst frame of the last second, without the VSync wait
	d["logic_max_ms"] = logic_max_ms;
	d["draw_max_ms"] = draw_max_ms;
	d["gpu_wait_ms"] = gpu_wait_ms;
	d["vblank_ms"] = vblank_ms;
	RasterizerCitro3d *r = RasterizerCitro3d::get_singleton();
	if (r) {
		const RasterizerCitro3d::FrameStats &fs = r->last_frame_stats;
		d["draw_calls"] = (int)fs.draw_calls;
		d["vertices"] = (int)fs.vertices;
		d["items"] = (int)fs.items;
		d["cmdbuf_percent"] = (int)(fs.cmdbuf_usage * 100.0f);
	}
	return d;
}

void Platform3DS::_bind_methods() {
	ObjectTypeDB::bind_method(_MD("set_canvas_layer_screens", "layer", "screens"), &Platform3DS::set_canvas_layer_screens);
	ObjectTypeDB::bind_method(_MD("get_canvas_layer_screens", "layer"), &Platform3DS::get_canvas_layer_screens);
	ObjectTypeDB::bind_method(_MD("get_frame_stats"), &Platform3DS::get_frame_stats);
	BIND_CONSTANT(SCREEN_NONE);
	BIND_CONSTANT(SCREEN_TOP);
	BIND_CONSTANT(SCREEN_BOTTOM);
	BIND_CONSTANT(SCREEN_BOTH);
	ObjectTypeDB::bind_method(_MD("get_memory_info"), &Platform3DS::get_memory_info);
	ObjectTypeDB::bind_method(_MD("is_new_3ds"), &Platform3DS::is_new_3ds);
	ObjectTypeDB::bind_method(_MD("set_cpu_speedup", "enabled"), &Platform3DS::set_cpu_speedup);
	ObjectTypeDB::bind_method(_MD("get_cpu_speedup"), &Platform3DS::get_cpu_speedup);
	ObjectTypeDB::bind_method(_MD("get_build_optimization"), &Platform3DS::get_build_optimization);
	ObjectTypeDB::bind_method(_MD("get_ticks_usec"), &Platform3DS::get_ticks_usec);
}

Platform3DS::Platform3DS() {
	singleton = this;
	heap_peak = 0;
	linear_peak = 0;
	vram_peak = 0;
	cpu_speedup = true; // OS_3DS enables it at startup
	frame_ms = logic_ms = draw_ms = frame_end_ms = gpu_wait_ms = vblank_ms = 0;
	frame_max_ms = logic_max_ms = draw_max_ms = 0;
	cur_frame_max = cur_logic_max = cur_draw_max = 0;
	peak_frames = 0;
}

Platform3DS::~Platform3DS() {
	if (singleton == this)
		singleton = NULL;
}
