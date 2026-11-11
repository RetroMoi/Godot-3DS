/*************************************************************************/
/*  export.cpp                                                           */
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
#include "export.h"
#include "editor/editor_import_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "globals.h"
#include "io/image_loader.h"
#include "os/dir_access.h"
#include "os/file_access.h"
#include "platform/3ds/logo.gen.h"
#include "scene/resources/texture.h"

/*
 * Nintendo 3DS exporter.
 *
 * Produces a ready-to-run .3dsx from an export template (the engine built with platform=3ds,
 * as a .3dsx) entirely inside the editor, without devkitPro:
 *
 *   template .3dsx (code)  +  SMDH (title/description/author/icons)  +  RomFS { data.pck }
 *
 * At runtime the 3DS port mounts the RomFS and loads romfs:/data.pck as the main pack.
 * The SMDH, RomFS and 3DSX layouts were validated byte for byte against devkitPro's
 * smdhtool/3dsxtool output (apart from the 24x24 icon downscale, which is smdhtool-specific).
 */

namespace {

void put_u16(Vector<uint8_t> &v, uint16_t x) {
	v.push_back(x & 0xFF);
	v.push_back(x >> 8);
}

void put_u32(Vector<uint8_t> &v, uint32_t x) {
	for (int i = 0; i < 4; i++)
		v.push_back((x >> (8 * i)) & 0xFF);
}

void put_u64(Vector<uint8_t> &v, uint64_t x) {
	for (int i = 0; i < 8; i++)
		v.push_back((x >> (8 * i)) & 0xFF);
}

void set_u32(Vector<uint8_t> &v, int p_ofs, uint32_t x) {
	for (int i = 0; i < 4; i++)
		v[p_ofs + i] = (x >> (8 * i)) & 0xFF;
}

uint32_t get_u32(const Vector<uint8_t> &v, int p_ofs) {
	return v[p_ofs] | (v[p_ofs + 1] << 8) | (v[p_ofs + 2] << 16) | ((uint32_t)v[p_ofs + 3] << 24);
}

uint16_t get_u16(const Vector<uint8_t> &v, int p_ofs) {
	return v[p_ofs] | (v[p_ofs + 1] << 8);
}

void pad_to(Vector<uint8_t> &v, int p_align) {
	while (v.size() % p_align)
		v.push_back(0);
}

Vector<uint16_t> to_utf16(const String &p_str) {
	Vector<uint16_t> out;
	for (int i = 0; i < p_str.length(); i++) {
		uint32_t c = p_str[i];
		if (c >= 0x10000) {
			c -= 0x10000;
			out.push_back(0xD800 | (c >> 10));
			out.push_back(0xDC00 | (c & 0x3FF));
		} else {
			out.push_back(c);
		}
	}
	return out;
}

// Fixed-size UTF-16 field, always NUL-terminated.
void put_utf16_field(Vector<uint8_t> &v, const String &p_str, int p_chars) {
	Vector<uint16_t> s = to_utf16(p_str);
	for (int i = 0; i < p_chars; i++)
		put_u16(v, i < p_chars - 1 && i < s.size() ? s[i] : 0);
}

/* RomFS (level 3, as read by libctru's romfs_dev) holding a single file at the root. */

uint32_t romfs_hash(uint32_t p_parent, const Vector<uint16_t> &p_name) {
	uint32_t h = p_parent ^ 123456789;
	for (int i = 0; i < p_name.size(); i++) {
		h = (h >> 5) | (h << 27);
		h ^= p_name[i];
	}
	return h;
}

Vector<uint8_t> build_romfs(const String &p_file_name, const Vector<uint8_t> &p_data) {
	const uint32_t EMPTY = 0xFFFFFFFF;
	const uint32_t HASH_SLOTS = 3; // libctru/3dsxtool use 3 buckets for tables this small
	Vector<uint16_t> name = to_utf16(p_file_name);
	int name_bytes = name.size() * 2;
	int name_padded = (name_bytes + 3) & ~3;

	const uint32_t header_size = 40;
	const uint32_t dir_hash_off = header_size, dir_hash_size = HASH_SLOTS * 4;
	const uint32_t dir_table_off = dir_hash_off + dir_hash_size, dir_table_size = 24;
	const uint32_t file_hash_off = dir_table_off + dir_table_size, file_hash_size = HASH_SLOTS * 4;
	const uint32_t file_table_off = file_hash_off + file_hash_size, file_table_size = 32 + name_padded;
	const uint32_t data_off = (file_table_off + file_table_size + 7) & ~7;

	Vector<uint8_t> v;
	put_u32(v, header_size);
	put_u32(v, dir_hash_off);
	put_u32(v, dir_hash_size);
	put_u32(v, dir_table_off);
	put_u32(v, dir_table_size);
	put_u32(v, file_hash_off);
	put_u32(v, file_hash_size);
	put_u32(v, file_table_off);
	put_u32(v, file_table_size);
	put_u32(v, data_off);

	// Directory hash table: the root directory (table offset 0).
	uint32_t root_slot = romfs_hash(0, Vector<uint16_t>()) % HASH_SLOTS;
	for (uint32_t i = 0; i < HASH_SLOTS; i++)
		put_u32(v, i == root_slot ? 0 : EMPTY);

	// Root directory: parent, sibling, first child dir, first child file, next in hash, name length.
	put_u32(v, 0);
	put_u32(v, EMPTY);
	put_u32(v, EMPTY);
	put_u32(v, 0);
	put_u32(v, EMPTY);
	put_u32(v, 0);

	uint32_t file_slot = romfs_hash(0, name) % HASH_SLOTS;
	for (uint32_t i = 0; i < HASH_SLOTS; i++)
		put_u32(v, i == file_slot ? 0 : EMPTY);

	// File: parent, sibling, data offset, data size, next in hash, name length, name.
	put_u32(v, 0);
	put_u32(v, EMPTY);
	put_u64(v, 0);
	put_u64(v, p_data.size());
	put_u32(v, EMPTY);
	put_u32(v, name_bytes);
	for (int i = 0; i < name.size(); i++)
		put_u16(v, name[i]);
	pad_to(v, 4);

	while (v.size() < (int)data_off)
		v.push_back(0);

	int base = v.size();
	v.resize(base + p_data.size());
	copymem(&v[base], p_data.ptr(), p_data.size());
	pad_to(v, 4);
	return v;
}

/* SMDH: title/description/author for all 16 languages, default settings, 24x24 and 48x48 icons. */

// RGB565 in 8x8 tiles, pixels in Morton (Z) order, colors premultiplied by alpha (as smdhtool does).
// p_scale = 2 averages 2x2 blocks to build the small icon from the 48x48 one.
void put_icon(Vector<uint8_t> &v, const Image &p_icon48, int p_scale) {
	DVector<uint8_t> data = p_icon48.get_data();
	DVector<uint8_t>::Read r = data.read();
	int size = 48 / p_scale;
	for (int ty = 0; ty < size; ty += 8) {
		for (int tx = 0; tx < size; tx += 8) {
			for (int i = 0; i < 64; i++) {
				int x = tx + ((i & 1) | ((i >> 1) & 2) | ((i >> 2) & 4));
				int y = ty + (((i >> 1) & 1) | ((i >> 2) & 2) | ((i >> 3) & 4));
				int acc[3] = { 0, 0, 0 };
				for (int dy = 0; dy < p_scale; dy++) {
					for (int dx = 0; dx < p_scale; dx++) {
						const uint8_t *px = &r[((y * p_scale + dy) * 48 + (x * p_scale + dx)) * 4];
						for (int c = 0; c < 3; c++)
							acc[c] += px[c] * px[3] / 255;
					}
				}
				int n = p_scale * p_scale;
				int rr = (acc[0] + n / 2) / n, gg = (acc[1] + n / 2) / n, bb = (acc[2] + n / 2) / n;
				put_u16(v, ((rr >> 3) << 11) | ((gg >> 2) << 5) | (bb >> 3));
			}
		}
	}
}

Vector<uint8_t> build_smdh(const String &p_title, const String &p_desc, const String &p_author, const Image &p_icon48) {
	Vector<uint8_t> v;
	v.push_back('S');
	v.push_back('M');
	v.push_back('D');
	v.push_back('H');
	put_u16(v, 0); // version
	put_u16(v, 0);
	for (int lang = 0; lang < 16; lang++) {
		put_utf16_field(v, p_title, 0x40);
		put_utf16_field(v, p_desc, 0x80);
		put_utf16_field(v, p_author, 0x40);
	}
	// Settings, same defaults as smdhtool.
	static const uint8_t ratings[16] = { 0xa0, 0xa0, 0x00, 0xa0, 0xa0, 0x00, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0, 0, 0, 0, 0 };
	for (int i = 0; i < 16; i++)
		v.push_back(ratings[i]);
	put_u32(v, 0xFFFFFFFF); // region lock: all regions
	for (int i = 0; i < 12; i++)
		v.push_back(0); // match maker ids
	put_u32(v, 0x141); // flags: visible, allow 3D, record usage
	put_u16(v, 0); // EULA version
	put_u16(v, 0);
	put_u32(v, 0); // optimal animation frame (float 0.0)
	put_u32(v, 0); // StreetPass id
	for (int i = 0; i < 8; i++)
		v.push_back(0);
	put_icon(v, p_icon48, 2);
	put_icon(v, p_icon48, 1);
	return v;
}

/* 3DSX: keep the template's code/relocations, (re)write the extended header, append SMDH + RomFS. */

Error build_3dsx(const Vector<uint8_t> &p_template, const Vector<uint8_t> &p_smdh, const Vector<uint8_t> &p_romfs, Vector<uint8_t> &r_out) {
	const int BASE_HEADER = 0x20, EXT_HEADER = 0x2C;
	if (p_template.size() < BASE_HEADER || p_template[0] != '3' || p_template[1] != 'D' || p_template[2] != 'S' || p_template[3] != 'X')
		return ERR_FILE_CORRUPT;

	int header_size = get_u16(p_template, 4);
	int body_end = p_template.size();
	if (header_size >= EXT_HEADER) {
		// Template already has SMDH/RomFS: they always come after the code, drop them.
		uint32_t offsets[2] = { get_u32(p_template, 0x20), get_u32(p_template, 0x28) };
		for (int i = 0; i < 2; i++)
			if (offsets[i] && (int)offsets[i] < body_end)
				body_end = offsets[i];
	}
	if (header_size < BASE_HEADER || body_end < header_size)
		return ERR_FILE_CORRUPT;

	int body_size = body_end - header_size;
	r_out.resize(EXT_HEADER + body_size);
	copymem(&r_out[0], p_template.ptr(), BASE_HEADER);
	r_out[4] = EXT_HEADER & 0xFF;
	r_out[5] = EXT_HEADER >> 8;
	uint32_t smdh_off = EXT_HEADER + body_size;
	set_u32(r_out, 0x20, smdh_off);
	set_u32(r_out, 0x24, p_smdh.size());
	set_u32(r_out, 0x28, smdh_off + p_smdh.size());
	copymem(&r_out[EXT_HEADER], &p_template[header_size], body_size);

	int ofs = r_out.size();
	r_out.resize(ofs + p_smdh.size() + p_romfs.size());
	copymem(&r_out[ofs], p_smdh.ptr(), p_smdh.size());
	copymem(&r_out[ofs + p_smdh.size()], p_romfs.ptr(), p_romfs.size());
	return OK;
}

} // namespace

class EditorExportPlatform3DS : public EditorExportPlatform {

	OBJ_TYPE(EditorExportPlatform3DS, EditorExportPlatform);

	String custom_template;
	String title;
	String description;
	String author;
	String icon;
	Ref<ImageTexture> logo;

	static const char *TEMPLATE_NAME;

	String _get_template_path(String *r_error) const {
		if (custom_template != "") {
			if (FileAccess::exists(custom_template))
				return custom_template;
			if (r_error)
				*r_error = "Custom template not found:\n" + custom_template;
			return "";
		}
		return find_export_template(TEMPLATE_NAME, r_error);
	}

	Image _load_icon() const {
		String path = icon;
		if (path == "")
			path = Globals::get_singleton()->get("application/icon");
		Image img;
		if (path == "" || ImageLoader::load_image(path, &img) != OK || img.empty())
			img = Image(_3ds_logo);
		img.convert(Image::FORMAT_RGBA);
		img.resize(48, 48, Image::INTERPOLATE_CUBIC);
		return img;
	}

protected:
	bool _set(const StringName &p_name, const Variant &p_value) {
		String n = p_name;
		if (n == "custom_template/release")
			custom_template = p_value;
		else if (n == "application/title")
			title = p_value;
		else if (n == "application/description")
			description = p_value;
		else if (n == "application/author")
			author = p_value;
		else if (n == "application/icon")
			icon = p_value;
		else
			return false;
		return true;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		String n = p_name;
		if (n == "custom_template/release")
			r_ret = custom_template;
		else if (n == "application/title")
			r_ret = title;
		else if (n == "application/description")
			r_ret = description;
		else if (n == "application/author")
			r_ret = author;
		else if (n == "application/icon")
			r_ret = icon;
		else
			return false;
		return true;
	}

	void _get_property_list(List<PropertyInfo> *p_list) const {
		p_list->push_back(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "3dsx"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/title"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/description"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/author"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "png"));
	}

public:
	virtual String get_name() const { return "Nintendo 3DS"; }
	virtual ImageCompression get_image_compression() const { return IMAGE_COMPRESSION_NONE; }
	virtual Ref<Texture> get_logo() const { return logo; }
	virtual String get_binary_extension() const { return "3dsx"; }

	virtual bool can_export(String *r_error = NULL) const {
		String err;
		bool ok = _get_template_path(&err) != "";
		if (!ok && r_error)
			*r_error = err + "\nBuild it with misc/3ds-build/build.sh and copy bin/godot.3dsx there as " + TEMPLATE_NAME + ".";
		return ok;
	}

	virtual Error export_project(const String &p_path, bool p_debug, int p_flags = 0) {
		EditorProgress ep("export", "Exporting for Nintendo 3DS", 5);

		ep.step("Reading template..", 0);
		String err;
		String template_path = _get_template_path(&err);
		if (template_path == "") {
			EditorNode::add_io_error(err);
			return ERR_FILE_NOT_FOUND;
		}
		Vector<uint8_t> tmpl = FileAccess::get_file_as_array(template_path);
		if (tmpl.size() == 0) {
			EditorNode::add_io_error("Couldn't read export template:\n" + template_path);
			return ERR_FILE_CANT_READ;
		}

		ep.step("Packing project (data.pck)..", 1);
		String tmp_dir = EditorSettings::get_singleton()->get_settings_path() + "/tmp";
		String pck_path = tmp_dir + "/3ds_data.pck";
		{
			DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			dir->make_dir_recursive(tmp_dir);
			memdelete(dir);
		}
		FileAccess *pck = FileAccess::open(pck_path, FileAccess::WRITE);
		if (!pck) {
			EditorNode::add_io_error("Couldn't create temporary file:\n" + pck_path);
			return ERR_FILE_CANT_WRITE;
		}
		Error pack_err = save_pack(pck);
		memdelete(pck);
		if (pack_err != OK) {
			EditorNode::add_io_error("Couldn't pack the project.");
			return pack_err;
		}
		Vector<uint8_t> pck_data = FileAccess::get_file_as_array(pck_path);
		DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->remove(pck_path);
		memdelete(da);

		ep.step("Building RomFS and SMDH..", 2);
		String app_name = Globals::get_singleton()->get("application/name");
		String smdh_title = title != "" ? title : (app_name != "" ? app_name : String("Godot Game"));
		String smdh_desc = description != "" ? description : String("Made with Godot Engine");
		String smdh_author = author != "" ? author : String("Unknown");
		Vector<uint8_t> smdh = build_smdh(smdh_title, smdh_desc, smdh_author, _load_icon());
		Vector<uint8_t> romfs = build_romfs("data.pck", pck_data);

		ep.step("Writing .3dsx..", 3);
		Vector<uint8_t> out;
		Error build_err = build_3dsx(tmpl, smdh, romfs, out);
		if (build_err != OK) {
			EditorNode::add_io_error("Invalid export template (not a .3dsx):\n" + template_path);
			return build_err;
		}
		FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
		if (!f) {
			EditorNode::add_io_error("Couldn't write:\n" + p_path);
			return ERR_FILE_CANT_WRITE;
		}
		f->store_buffer(out.ptr(), out.size());
		memdelete(f);

		ep.step("Done.", 4);
		return OK;
	}

	EditorExportPlatform3DS() {
		Image img(_3ds_logo);
		logo = Ref<ImageTexture>(memnew(ImageTexture));
		logo->create_from_image(img);
	}
};

const char *EditorExportPlatform3DS::TEMPLATE_NAME = "3ds_release.3dsx";

void register_3ds_exporter() {

	Ref<EditorExportPlatform3DS> exporter = Ref<EditorExportPlatform3DS>(memnew(EditorExportPlatform3DS));
	EditorImportExport::get_singleton()->add_export_platform(exporter);
}
