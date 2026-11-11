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

void Platform3DS::_bind_methods() {
	ObjectTypeDB::bind_method(_MD("get_memory_info"), &Platform3DS::get_memory_info);
	ObjectTypeDB::bind_method(_MD("is_new_3ds"), &Platform3DS::is_new_3ds);
}

Platform3DS::Platform3DS() {
	singleton = this;
	heap_peak = 0;
	linear_peak = 0;
	vram_peak = 0;
}

Platform3DS::~Platform3DS() {
	if (singleton == this)
		singleton = NULL;
}
