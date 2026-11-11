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

	Platform3DS();
	~Platform3DS();
};

#endif // PLATFORM_3DS_H
