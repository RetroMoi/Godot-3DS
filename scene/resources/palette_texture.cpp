/*************************************************************************/
/*  palette_texture.cpp                                                  */
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

#include "palette_texture.h"

#include "io/image_loader.h"
#include "os/file_access.h"
#include "servers/visual_server.h"

static inline uint8_t _to_byte(float p_v) {

	return (uint8_t)CLAMP(int(p_v * 255.0f + 0.5f), 0, 255);
}

PaletteTexture::PaletteTexture() {

	texture = VisualServer::get_singleton()->texture_create();
	width = height = 0;
	flags = 0; // nearest, no repeat: pixel art (and the 3DS fast path writes no padding then)
	allocated = false;
}

PaletteTexture::~PaletteTexture() {

	VisualServer::get_singleton()->free(texture);
}

int PaletteTexture::_find(const String &p_name) const {

	for (int i = 0; i < palettes.size(); i++) {
		if (palettes[i].name == p_name)
			return i;
	}
	return -1;
}

Error PaletteTexture::_fail(const String &p_error) {

	last_error = p_error;
	ERR_PRINT(("PaletteTexture: " + p_error).utf8().get_data());
	return ERR_INVALID_DATA;
}

void PaletteTexture::_setup_texture() {

	if (width <= 0 || height <= 0 || indices.size() != width * height)
		return;
	VisualServer *vs = VisualServer::get_singleton();
	vs->texture_allocate(texture, width, height, Image::FORMAT_RGBA, flags);
	if (vs->has_feature(VisualServer::FEATURE_PALETTE_TEXTURE))
		vs->texture_set_palette_indices(texture, indices);
	allocated = true;
}

void PaletteTexture::_apply(const DVector<Color> &p_colors) {

	applied = p_colors;
	if (!allocated)
		_setup_texture();
	if (!allocated)
		return;

	// The palette as RGBA bytes (up to 256 entries; missing ones are transparent black).
	int n = MIN(p_colors.size(), 256);
	DVector<uint8_t> pal;
	pal.resize(256 * 4);
	{
		DVector<uint8_t>::Write w = pal.write();
		DVector<Color>::Read r = p_colors.read();
		for (int i = 0; i < 256; i++) {
			if (i < n) {
				const Color &c = r[i];
				w[i * 4 + 0] = _to_byte(c.r);
				w[i * 4 + 1] = _to_byte(c.g);
				w[i * 4 + 2] = _to_byte(c.b);
				w[i * 4 + 3] = _to_byte(c.a);
			} else {
				w[i * 4 + 0] = w[i * 4 + 1] = w[i * 4 + 2] = w[i * 4 + 3] = 0;
			}
		}
	}

	VisualServer *vs = VisualServer::get_singleton();
	if (vs->has_feature(VisualServer::FEATURE_PALETTE_TEXTURE)) {
		// The renderer writes palette[index] into the texture itself.
		vs->texture_set_palette(texture, pal);
		return;
	}

	// Elsewhere (PC): expand to an RGBA image and upload it.
	int count = width * height;
	DVector<uint8_t> data;
	data.resize(count * 4);
	{
		DVector<uint8_t>::Write w = data.write();
		DVector<uint8_t>::Read ri = indices.read();
		DVector<uint8_t>::Read rp = pal.read();
		const uint32_t *p32 = (const uint32_t *)rp.ptr();
		uint32_t *o32 = (uint32_t *)w.ptr();
		for (int i = 0; i < count; i++)
			o32[i] = p32[ri[i]];
	}
	vs->texture_set_data(texture, Image(width, height, 0, Image::FORMAT_RGBA, data));
}

void PaletteTexture::_apply_current() {

	int idx = _find(current_palette);
	if (idx < 0 && palettes.size())
		idx = 0;
	if (idx >= 0)
		_apply(palettes[idx].colors);
}

/////////////////////////////////////////////////////////////////////////////
// Building from files

String PaletteTexture::palette_name_for(const String &p_image_path, const String &p_palette_path) {

	// outside_atlas-dusk.hex next to outside_atlas.png -> "dusk"; outside_atlas.hex -> "base".
	String base = p_image_path.get_file().basename();
	String stem = p_palette_path.get_file().basename();
	if (stem == base)
		return "base";
	if (stem.length() > base.length() + 1 && stem.begins_with(base) && (stem[base.length()] == '-' || stem[base.length()] == '_'))
		return stem.substr(base.length() + 1, stem.length());
	return stem;
}

static int _hex_digit(CharType c) {

	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

Error PaletteTexture::load_hex(const String &p_path, DVector<Color> *r_colors, String *r_error) {

	// One color per line: RRGGBB or RRGGBBAA, an optional leading '#'. Blank lines and lines
	// starting with ';' are skipped.
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		if (r_error)
			*r_error = "Can't open " + p_path + ".";
		return ERR_CANT_OPEN;
	}
	r_colors->resize(0);
	int line_no = 0;
	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		line_no++;
		if (line == "" || line.begins_with(";"))
			continue;
		if (line.begins_with("#"))
			line = line.substr(1, line.length());
		bool ok = line.length() == 6 || line.length() == 8;
		int v[4] = { 0, 0, 0, 255 };
		for (int i = 0; ok && i < line.length() / 2; i++) {
			int hi = _hex_digit(line[i * 2]), lo = _hex_digit(line[i * 2 + 1]);
			if (hi < 0 || lo < 0)
				ok = false;
			else
				v[i] = hi * 16 + lo;
		}
		if (!ok) {
			memdelete(f);
			if (r_error)
				*r_error = p_path.get_file() + ", line " + itos(line_no) + ": expected RRGGBB or RRGGBBAA, found \"" + line + "\".";
			return ERR_PARSE_ERROR;
		}
		// Alpha -1 marks "not given": the caller takes it from the indexed image.
		r_colors->push_back(Color(v[0] / 255.0, v[1] / 255.0, v[2] / 255.0, line.length() == 8 ? v[3] / 255.0 : -1.0));
	}
	memdelete(f);
	return OK;
}

// The indices and palette of an indexed image (palette: 256 entries, alpha 1 without tRNS).
static bool _indexed_parts(const Image &p_img, DVector<uint8_t> *r_indices, DVector<Color> *r_palette) {

	if (p_img.get_format() != Image::FORMAT_INDEXED && p_img.get_format() != Image::FORMAT_INDEXED_ALPHA)
		return false;
	int count = p_img.get_width() * p_img.get_height();
	int comps = p_img.get_format() == Image::FORMAT_INDEXED_ALPHA ? 4 : 3;
	DVector<uint8_t> data = p_img.get_data();
	if (data.size() < count + 256 * comps)
		return false;
	DVector<uint8_t>::Read r = data.read();
	r_indices->resize(count);
	{
		DVector<uint8_t>::Write w = r_indices->write();
		copymem(w.ptr(), r.ptr(), count);
	}
	r_palette->resize(256);
	DVector<Color>::Write wp = r_palette->write();
	const uint8_t *p = r.ptr() + count;
	for (int i = 0; i < 256; i++, p += comps)
		wp[i] = Color(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0, comps == 4 ? p[3] / 255.0 : 1.0);
	return true;
}

// Palette PNGs keep their indices here (the usual loading expands them to RGBA).
static Error _load_indexed(const String &p_path, Image *r_image) {

	if (Image::_png_load_indexed_func && p_path.extension().to_lower() == "png")
		return Image::_png_load_indexed_func(p_path, r_image);
	return ImageLoader::load_image(p_path, r_image);
}

Error PaletteTexture::create_from_sources(const String &p_image_path, const Vector<String> &p_palette_paths) {

	last_error = "";
	Image img;
	if (_load_indexed(p_image_path, &img) != OK || img.empty())
		return _fail("Can't load the image " + p_image_path + ".");
	DVector<uint8_t> idx;
	DVector<Color> base_colors;
	if (!_indexed_parts(img, &idx, &base_colors))
		return _fail(p_image_path.get_file() + " is not an indexed (palette) PNG: save it with an indexed color mode.");

	// Palettes must cover every index the image uses.
	int needed = 0;
	{
		DVector<uint8_t>::Read r = idx.read();
		for (int i = 0; i < idx.size(); i++)
			needed = MAX(needed, int(r[i]) + 1);
	}
	base_colors.resize(needed);

	Vector<Palette> new_palettes;
	Palette base;
	base.name = "base";
	base.colors = base_colors;
	base.source = p_image_path;
	new_palettes.push_back(base);

	for (int i = 0; i < p_palette_paths.size(); i++) {
		const String &path = p_palette_paths[i];
		String ext = path.extension().to_lower();
		Palette pal;
		pal.name = palette_name_for(p_image_path, path);
		pal.source = path;
		if (ext == "hex") {
			String err;
			if (load_hex(path, &pal.colors, &err) != OK)
				return _fail(err);
			if (pal.colors.size() < needed)
				return _fail(path.get_file() + " has " + itos(pal.colors.size()) + " colors; " + p_image_path.get_file() + " uses " + itos(needed) + " (indices 0 to " + itos(needed - 1) + ").");
			pal.colors.resize(needed);
			DVector<Color>::Write w = pal.colors.write();
			DVector<Color>::Read rb = base_colors.read();
			for (int j = 0; j < needed; j++) {
				if (w[j].a < 0)
					w[j].a = rb[j].a; // transparency per index comes from the image
			}
		} else if (ext == "png") {
			Image other;
			DVector<uint8_t> other_idx;
			if (_load_indexed(path, &other) != OK || other.empty())
				return _fail("Can't load " + path + ".");
			if (!_indexed_parts(other, &other_idx, &pal.colors))
				return _fail(path.get_file() + " is not an indexed (palette) PNG.");
			if (other.get_width() != img.get_width() || other.get_height() != img.get_height())
				return _fail(path.get_file() + " is " + itos(other.get_width()) + "x" + itos(other.get_height()) + "; " + p_image_path.get_file() + " is " + itos(img.get_width()) + "x" + itos(img.get_height()) + ".");
			int differ = 0;
			{
				DVector<uint8_t>::Read a = idx.read();
				DVector<uint8_t>::Read b = other_idx.read();
				for (int j = 0; j < idx.size(); j++)
					differ += a[j] != b[j];
			}
			if (differ)
				return _fail(path.get_file() + " is not the same image with other colors: " + itos(differ) + " pixels use other palette indices than in " + p_image_path.get_file() + ".");
			pal.colors.resize(needed);
		} else {
			return _fail(path.get_file() + ": palettes come from .hex files or indexed .png files.");
		}

		int existing = -1;
		for (int j = 0; j < new_palettes.size(); j++) {
			if (new_palettes[j].name == pal.name)
				existing = j;
		}
		if (existing == 0)
			new_palettes[0] = pal; // a file for the image's own palette (e.g. outside_atlas.hex) replaces it
		else if (existing > 0)
			return _fail("Two palettes named \"" + pal.name + "\" (" + new_palettes[existing].source.get_file() + " and " + path.get_file() + ").");
		else
			new_palettes.push_back(pal);
	}

	bool resized = width != img.get_width() || height != img.get_height();
	width = img.get_width();
	height = img.get_height();
	indices = idx;
	palettes = new_palettes;
	source_image = p_image_path;
	if (_find(current_palette) < 0)
		current_palette = "base";
	if (resized || !allocated)
		allocated = false;
	_setup_texture();
	_apply_current();
	_change_notify();
	emit_changed();
	return OK;
}

Error PaletteTexture::_create_from_sources_bind(const String &p_image_path, const StringArray &p_palette_paths) {

	Vector<String> paths;
	for (int i = 0; i < p_palette_paths.size(); i++)
		paths.push_back(p_palette_paths[i]);
	return create_from_sources(p_image_path, paths);
}

/////////////////////////////////////////////////////////////////////////////
// Palettes

int PaletteTexture::get_palette_count() const {

	return palettes.size();
}

StringArray PaletteTexture::get_palette_names() const {

	StringArray names;
	for (int i = 0; i < palettes.size(); i++)
		names.push_back(palettes[i].name);
	return names;
}

bool PaletteTexture::has_palette(const String &p_name) const {

	return _find(p_name) >= 0;
}

DVector<Color> PaletteTexture::get_palette_colors(const String &p_name) const {

	int i = _find(p_name);
	ERR_FAIL_COND_V(i < 0, DVector<Color>());
	return palettes[i].colors;
}

void PaletteTexture::set_palette_colors(const String &p_name, const DVector<Color> &p_colors) {

	ERR_FAIL_COND(p_name == "");
	int i = _find(p_name);
	if (i < 0) {
		Palette p;
		p.name = p_name;
		palettes.push_back(p);
		i = palettes.size() - 1;
	}
	palettes[i].colors = p_colors;
	if (p_name == current_palette)
		_apply(p_colors);
	_change_notify();
}

void PaletteTexture::remove_palette(const String &p_name) {

	int i = _find(p_name);
	ERR_FAIL_COND(i < 0);
	palettes.remove(i);
	_change_notify();
}

void PaletteTexture::set_palette(const String &p_name) {

	int i = _find(p_name);
	ERR_FAIL_COND(i < 0);
	current_palette = p_name;
	_apply(palettes[i].colors);
}

void PaletteTexture::blend_palettes(const String &p_from, const String &p_to, float p_weight) {

	int a = _find(p_from), b = _find(p_to);
	ERR_FAIL_COND(a < 0 || b < 0);
	float t = CLAMP(p_weight, 0.0f, 1.0f);
	const DVector<Color> &ca = palettes[a].colors;
	const DVector<Color> &cb = palettes[b].colors;
	int n = MIN(ca.size(), cb.size());
	DVector<Color> out;
	out.resize(n);
	{
		DVector<Color>::Read ra = ca.read();
		DVector<Color>::Read rb = cb.read();
		DVector<Color>::Write w = out.write();
		for (int i = 0; i < n; i++) {
			const Color &x = ra[i];
			const Color &y = rb[i];
			float alpha = x.a + (y.a - x.a) * t;
			if (x.a == y.a || alpha <= 0) {
				w[i] = Color(x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t, alpha);
			} else {
				// Premultiplied, as ImageTexture.create_from_blend: a transparent color does not bleed.
				float ka = x.a * (1 - t), kb = y.a * t;
				w[i] = Color((x.r * ka + y.r * kb) / alpha, (x.g * ka + y.g * kb) / alpha, (x.b * ka + y.b * kb) / alpha, alpha);
			}
		}
	}
	_apply(out);
}

void PaletteTexture::set_colors(const DVector<Color> &p_colors) {

	_apply(p_colors);
}

Image PaletteTexture::get_data() const {

	if (width <= 0 || height <= 0 || indices.size() != width * height)
		return Image();
	int n = applied.size();
	DVector<uint8_t> data;
	data.resize(width * height * 4);
	{
		DVector<uint8_t>::Write w = data.write();
		DVector<uint8_t>::Read ri = indices.read();
		DVector<Color>::Read rc = applied.read();
		for (int i = 0; i < width * height; i++) {
			int k = ri[i];
			Color c = k < n ? rc[k] : Color(0, 0, 0, 0);
			w[i * 4 + 0] = _to_byte(c.r);
			w[i * 4 + 1] = _to_byte(c.g);
			w[i * 4 + 2] = _to_byte(c.b);
			w[i * 4 + 3] = _to_byte(c.a);
		}
	}
	return Image(width, height, 0, Image::FORMAT_RGBA, data);
}

/////////////////////////////////////////////////////////////////////////////
// Texture

int PaletteTexture::get_width() const {

	return width;
}

int PaletteTexture::get_height() const {

	return height;
}

RID PaletteTexture::get_rid() const {

	return texture;
}

bool PaletteTexture::has_alpha() const {

	DVector<Color>::Read r = applied.read();
	for (int i = 0; i < applied.size(); i++) {
		if (r[i].a < 1)
			return true;
	}
	return false;
}

void PaletteTexture::set_flags(uint32_t p_flags) {

	flags = p_flags;
	if (allocated)
		VisualServer::get_singleton()->texture_set_flags(texture, flags);
}

uint32_t PaletteTexture::get_flags() const {

	return flags;
}

/////////////////////////////////////////////////////////////////////////////
// Storage

bool PaletteTexture::_set(const StringName &p_name, const Variant &p_value) {

	String n = p_name;
	if (n == "flags") {
		set_flags(p_value);
	} else if (n == "size") {
		Vector2 s = p_value;
		if (int(s.x) != width || int(s.y) != height)
			allocated = false;
		width = s.x;
		height = s.y;
	} else if (n == "indices") {
		indices = p_value;
		allocated = false;
	} else if (n == "source_image") {
		source_image = p_value;
	} else if (n.begins_with("palettes/")) {
		int i = n.get_slice("/", 1).to_int();
		String what = n.get_slice("/", 2);
		ERR_FAIL_COND_V(i < 0 || i > 1024, false);
		if (i >= palettes.size())
			palettes.resize(i + 1);
		if (what == "name")
			palettes[i].name = p_value;
		else if (what == "colors")
			palettes[i].colors = p_value;
		else if (what == "source")
			palettes[i].source = p_value;
		else
			return false;
	} else if (n == "current_palette") {
		// Last in the property list: everything else is loaded by now.
		current_palette = p_value;
		_apply_current();
	} else {
		return false;
	}
	return true;
}

bool PaletteTexture::_get(const StringName &p_name, Variant &r_ret) const {

	String n = p_name;
	if (n == "flags")
		r_ret = flags;
	else if (n == "size")
		r_ret = Vector2(width, height);
	else if (n == "indices")
		r_ret = indices;
	else if (n == "source_image")
		r_ret = source_image;
	else if (n.begins_with("palettes/")) {
		int i = n.get_slice("/", 1).to_int();
		String what = n.get_slice("/", 2);
		if (i < 0 || i >= palettes.size())
			return false;
		if (what == "name")
			r_ret = palettes[i].name;
		else if (what == "colors")
			r_ret = palettes[i].colors;
		else if (what == "source")
			r_ret = palettes[i].source;
		else
			return false;
	} else if (n == "current_palette")
		r_ret = current_palette;
	else
		return false;
	return true;
}

void PaletteTexture::_get_property_list(List<PropertyInfo> *p_list) const {

	p_list->push_back(PropertyInfo(Variant::INT, "flags", PROPERTY_HINT_FLAGS, "Mipmaps,Repeat,Filter,Anisotropic,sRGB,Mirrored Repeat"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2, "size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	p_list->push_back(PropertyInfo(Variant::RAW_ARRAY, "indices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	p_list->push_back(PropertyInfo(Variant::STRING, "source_image", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	for (int i = 0; i < palettes.size(); i++) {
		String pre = "palettes/" + itos(i) + "/";
		p_list->push_back(PropertyInfo(Variant::STRING, pre + "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::COLOR_ARRAY, pre + "colors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::STRING, pre + "source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	}
	// The palette shown when loaded; picking one in the Inspector previews it.
	String names;
	for (int i = 0; i < palettes.size(); i++) {
		if (i)
			names += ",";
		names += palettes[i].name;
	}
	p_list->push_back(PropertyInfo(Variant::STRING, "current_palette", PROPERTY_HINT_ENUM, names));
}

void PaletteTexture::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("create_from_sources", "image_path", "palette_paths"), &PaletteTexture::_create_from_sources_bind);
	ObjectTypeDB::bind_method(_MD("get_last_error"), &PaletteTexture::get_last_error);
	ObjectTypeDB::bind_method(_MD("get_source_image"), &PaletteTexture::get_source_image);
	ObjectTypeDB::bind_method(_MD("get_palette_count"), &PaletteTexture::get_palette_count);
	ObjectTypeDB::bind_method(_MD("get_palette_names"), &PaletteTexture::get_palette_names);
	ObjectTypeDB::bind_method(_MD("has_palette", "name"), &PaletteTexture::has_palette);
	ObjectTypeDB::bind_method(_MD("get_palette_colors", "name"), &PaletteTexture::get_palette_colors);
	ObjectTypeDB::bind_method(_MD("set_palette_colors", "name", "colors"), &PaletteTexture::set_palette_colors);
	ObjectTypeDB::bind_method(_MD("remove_palette", "name"), &PaletteTexture::remove_palette);
	ObjectTypeDB::bind_method(_MD("set_palette", "name"), &PaletteTexture::set_palette);
	ObjectTypeDB::bind_method(_MD("get_palette"), &PaletteTexture::get_palette);
	ObjectTypeDB::bind_method(_MD("blend_palettes", "from", "to", "weight"), &PaletteTexture::blend_palettes);
	ObjectTypeDB::bind_method(_MD("set_colors", "colors"), &PaletteTexture::set_colors);
	ObjectTypeDB::bind_method(_MD("get_colors"), &PaletteTexture::get_colors);
	ObjectTypeDB::bind_method(_MD("get_data"), &PaletteTexture::get_data);
}
