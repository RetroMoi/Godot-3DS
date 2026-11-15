/*************************************************************************/
/*  palette_texture.h                                                    */
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

#ifndef PALETTE_TEXTURE_H
#define PALETTE_TEXTURE_H

#include "scene/resources/texture.h"

// An indexed image (one palette index per pixel) with named palettes. The GPU texture shows the
// image with the current palette; changing or blending palettes rewrites its pixels, much cheaper
// than blending images (on the 3DS the renderer keeps the indices in the GPU's tiled order and
// just writes palette[index] per texel). For palette swaps such as day/night variants of a tile
// atlas: same indices, different colors.
//
// Built from an indexed PNG (indices + its own palette, named "base") and palette sources: .hex
// files (one RRGGBB or RRGGBBAA color per line, line N = index N, as Lospec/Aseprite save them)
// or other indexed PNGs with exactly the same indices. Everything is stored in the resource: no
// PNG or .hex is read at run time.
class PaletteTexture : public Texture {

	OBJ_TYPE(PaletteTexture, Texture);

	struct Palette {
		String name;
		DVector<Color> colors;
		String source;
	};

	RID texture;
	int width, height;
	uint32_t flags;
	DVector<uint8_t> indices; // width * height
	Vector<Palette> palettes;
	String source_image;
	String current_palette; // the palette shown when loaded (and set by set_palette)
	DVector<Color> applied; // the colors on the GPU texture now
	bool allocated;
	String last_error;

	int _find(const String &p_name) const;
	void _setup_texture(); // allocates the GPU texture and hands it the indices
	void _apply(const DVector<Color> &p_colors);
	void _apply_current();
	Error _fail(const String &p_error);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	// Building (also what the editor's importer uses). On error, get_last_error() says why.
	Error create_from_sources(const String &p_image_path, const Vector<String> &p_palette_paths);
	Error _create_from_sources_bind(const String &p_image_path, const StringArray &p_palette_paths);
	String get_last_error() const { return last_error; }
	String get_source_image() const { return source_image; }
	static String palette_name_for(const String &p_image_path, const String &p_palette_path);
	static Error load_hex(const String &p_path, DVector<Color> *r_colors, String *r_error);

	// Palettes.
	int get_palette_count() const;
	StringArray get_palette_names() const;
	bool has_palette(const String &p_name) const;
	DVector<Color> get_palette_colors(const String &p_name) const;
	void set_palette_colors(const String &p_name, const DVector<Color> &p_colors); // adds or replaces
	void remove_palette(const String &p_name);

	// What the texture shows. All of these rewrite the GPU texture right away.
	void set_palette(const String &p_name);
	String get_palette() const { return current_palette; }
	void blend_palettes(const String &p_from, const String &p_to, float p_weight); // 0 = from, 1 = to
	void set_colors(const DVector<Color> &p_colors); // any colors (index N = color N)
	DVector<Color> get_colors() const { return applied; }

	Image get_data() const; // RGBA, with the colors shown now (for editor tools)

	virtual int get_width() const;
	virtual int get_height() const;
	virtual RID get_rid() const;
	virtual bool has_alpha() const;
	virtual void set_flags(uint32_t p_flags);
	virtual uint32_t get_flags() const;

	PaletteTexture();
	~PaletteTexture();
};

#endif // PALETTE_TEXTURE_H
