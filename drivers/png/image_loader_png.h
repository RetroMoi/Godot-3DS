/*************************************************************************/
/*  image_loader_png.h                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifndef IMAGE_LOADER_PNG_H
#define IMAGE_LOADER_PNG_H

#include "io/image_loader.h"

#include <png.h>

/**
	@author Juan Linietsky <reduzio@gmail.com>
*/
class ImageLoaderPNG : public ImageFormatLoader {

	static void _read_png_data(png_structp png_ptr, png_bytep data, png_size_t p_length);

public:
	// p_keep_palette: a palette PNG stays indexed (FORMAT_INDEXED[_ALPHA], transparency from tRNS
	// in the palette) instead of being expanded to RGBA. Used by PaletteTexture.
	static Error _load_image(void *rf_up, png_rw_ptr p_func, Image *p_image, bool p_keep_palette = false);
	virtual Error load_image(Image *p_image, FileAccess *f);
	static Error load_indexed(const String &p_path, Image *r_image); // keeps the palette (Image::_png_load_indexed_func)
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	ImageLoaderPNG();
};

#endif
