/*************************************************************************/
/*  tile_atlas_view.cpp                                                  */
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

#include "tile_atlas_view.h"

#include "os/input.h"
#include "os/keyboard.h"
#include "servers/visual_server.h"
#include "editor/editor_scale.h"

// Tiles with no region use the whole texture.
static Rect2 _tile_region(const Ref<TileSet> &p_tileset, int p_id) {

	Rect2 r = p_tileset->tile_get_region(p_id);
	if (r.has_no_area()) {
		Ref<Texture> tex = p_tileset->tile_get_texture(p_id);
		if (tex.is_valid())
			r = Rect2(Point2(), tex->get_size());
	}
	return r;
}

void TileAtlasView::get_sources(const Ref<TileSet> &p_tileset, Vector<Source> *r_sources) {

	r_sources->clear();
	if (p_tileset.is_null())
		return;

	for (int i = 0; i < p_tileset->atlas_get_count(); i++) {
		Source s;
		s.texture = p_tileset->atlas_get_texture(i);
		if (s.texture.is_null())
			continue;
		s.atlas = i;
		s.tile_size = p_tileset->atlas_get_tile_size(i);
		s.margin = p_tileset->atlas_get_margin(i);
		s.separation = p_tileset->atlas_get_separation(i);
		r_sources->push_back(s);
	}

	// Textures used by tiles but without atlas settings: guess the grid from the most common
	// tile size (multi-cell tiles such as 2x2 trees are the exception, not the rule).
	List<int> ids;
	p_tileset->get_tile_list(&ids);
	Vector<Ref<Texture> > implicit;
	for (List<int>::Element *E = ids.front(); E; E = E->next()) {
		Ref<Texture> tex = p_tileset->tile_get_texture(E->get());
		if (tex.is_null() || p_tileset->atlas_find(tex) >= 0 || implicit.find(tex) >= 0)
			continue;
		implicit.push_back(tex);
	}
	for (int i = 0; i < implicit.size(); i++) {
		Map<Vector2, int> sizes;
		for (List<int>::Element *E = ids.front(); E; E = E->next()) {
			if (p_tileset->tile_get_texture(E->get()) != implicit[i])
				continue;
			Vector2 s = _tile_region(p_tileset, E->get()).size;
			if (s.x > 0 && s.y > 0)
				sizes[s] = (sizes.has(s) ? sizes[s] : 0) + 1;
		}
		Vector2 best(16, 16);
		int best_count = 0;
		for (Map<Vector2, int>::Element *E = sizes.front(); E; E = E->next()) {
			if (E->get() > best_count) {
				best = E->key();
				best_count = E->get();
			}
		}
		Source s;
		s.texture = implicit[i];
		s.atlas = -1;
		s.tile_size = best;
		r_sources->push_back(s);
	}
}

void TileAtlasView::set_tileset(const Ref<TileSet> &p_tileset) {

	tileset = p_tileset;
	update();
}

void TileAtlasView::set_source(const Source &p_source) {

	bool same_texture = has_source && source.texture == p_source.texture;
	source = p_source;
	source.tile_size = Vector2(MAX(1, source.tile_size.x), MAX(1, source.tile_size.y));
	has_source = source.texture.is_valid();
	if (!same_texture)
		fit();
	update();
}

void TileAtlasView::clear_source() {

	has_source = false;
	source = Source();
	update();
}

void TileAtlasView::set_selected(const Vector<int> &p_ids) {

	selected.clear();
	for (int i = 0; i < p_ids.size(); i++)
		selected.insert(p_ids[i]);
	update();
}

void TileAtlasView::set_show_ids(bool p_show) {

	show_ids = p_show;
	update();
}

void TileAtlasView::fit() {

	if (!has_source)
		return;
	Size2 s = get_size();
	if (s.x < 2 || s.y < 2) {
		fit_pending = true; // not laid out yet
		return;
	}
	fit_pending = false;
	Size2 ts = source.texture->get_size();
	float z = MIN((s.x - 16) / ts.x, (s.y - 16) / ts.y);
	if (z >= 1) {
		// The whole atlas fits: whole zoom steps keep pixel art crisp.
		zoom = Math::floor(z);
		ofs = ((s - ts * zoom) / 2).floor();
	} else {
		// Too big (or too tall) to fit: fit the width and start at the top; the wheel scrolls.
		zoom = CLAMP(Math::floor((s.x - 16) / ts.x), 1, 4);
		ofs = Vector2(MAX(8, Math::floor((s.x - ts.x * zoom) / 2)), 8);
	}
	update();
}

Vector2 TileAtlasView::get_grid_size() const {

	if (!has_source)
		return Vector2();
	Size2 ts = source.texture->get_size();
	Vector2 step = source.tile_size + source.separation;
	return Vector2(MAX(0, Math::floor((ts.x - source.margin.x + source.separation.x) / step.x)),
			MAX(0, Math::floor((ts.y - source.margin.y + source.separation.y) / step.y)));
}

Vector2 TileAtlasView::cell_at_texel(const Vector2 &p_texel) const {

	Vector2 step = source.tile_size + source.separation;
	return ((p_texel - source.margin) / step).floor();
}

Rect2 TileAtlasView::cells_region(const Rect2 &p_cells) const {

	Vector2 step = source.tile_size + source.separation;
	return Rect2(source.margin + p_cells.pos * step, p_cells.size * source.tile_size + (p_cells.size - Vector2(1, 1)) * source.separation);
}

Rect2 TileAtlasView::tile_cells(int p_id) const {

	Rect2 r = _tile_region(tileset, p_id);
	Vector2 step = source.tile_size + source.separation;
	Vector2 from = ((r.pos - source.margin) / step).floor();
	Vector2 to = ((r.pos + r.size - source.margin - Vector2(1, 1)) / step).floor();
	return Rect2(from, to - from + Vector2(1, 1));
}

int TileAtlasView::tile_at_cell(const Vector2 &p_cell) const {

	if (tileset.is_null() || !has_source)
		return -1;
	Rect2 cell = cells_region(Rect2(p_cell, Vector2(1, 1)));
	Vector2 center = cell.pos + cell.size / 2;
	List<int> ids;
	tileset->get_tile_list(&ids);
	for (List<int>::Element *E = ids.front(); E; E = E->next()) {
		if (tileset->tile_get_texture(E->get()) != source.texture)
			continue;
		if (_tile_region(tileset, E->get()).has_point(center))
			return E->get();
	}
	return -1;
}

void TileAtlasView::get_tiles_in(const Rect2 &p_cells, Vector<int> *r_ids) const {

	r_ids->clear();
	if (tileset.is_null() || !has_source)
		return;
	Rect2 area = cells_region(p_cells);
	List<int> ids;
	tileset->get_tile_list(&ids);
	for (List<int>::Element *E = ids.front(); E; E = E->next()) {
		if (tileset->tile_get_texture(E->get()) != source.texture)
			continue;
		Rect2 r = _tile_region(tileset, E->get());
		if (area.has_point(r.pos + r.size / 2))
			r_ids->push_back(E->get());
	}
}

bool TileAtlasView::cell_is_empty(const Image &p_image, const Vector2 &p_cell) const {

	if (p_image.empty())
		return false;
	Rect2 r = cells_region(Rect2(p_cell, Vector2(1, 1)));
	int x0 = MAX(0, (int)r.pos.x), y0 = MAX(0, (int)r.pos.y);
	int x1 = MIN(p_image.get_width(), (int)(r.pos.x + r.size.x));
	int y1 = MIN(p_image.get_height(), (int)(r.pos.y + r.size.y));
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) {
			if (p_image.get_pixel(x, y).a > 0.01)
				return false;
		}
	}
	return true;
}

Vector2 TileAtlasView::_texel_at(const Point2 &p_pos) const {

	return (p_pos - ofs) / zoom;
}

void TileAtlasView::_draw_rect_outline(const Rect2 &p_rect, const Color &p_color, float p_width) {

	Point2 a = p_rect.pos, b = p_rect.pos + p_rect.size;
	draw_line(a, Point2(b.x, a.y), p_color, p_width);
	draw_line(Point2(b.x, a.y), b, p_color, p_width);
	draw_line(b, Point2(a.x, b.y), p_color, p_width);
	draw_line(Point2(a.x, b.y), a, p_color, p_width);
}

void TileAtlasView::_zoom_at(float p_factor, const Point2 &p_pos) {

	float new_zoom = CLAMP(zoom * p_factor, 0.25, 32);
	if (new_zoom >= 1)
		new_zoom = Math::round(new_zoom);
	if (new_zoom == zoom)
		new_zoom = CLAMP(zoom + (p_factor > 1 ? 1 : -1), 0.25, 32);
	Vector2 texel = _texel_at(p_pos);
	zoom = new_zoom;
	ofs = (p_pos - texel * zoom).floor();
	update();
}

void TileAtlasView::_notification(int p_what) {

	if (p_what == NOTIFICATION_RESIZED && fit_pending)
		fit();

	if (p_what != NOTIFICATION_DRAW)
		return;

	VisualServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), true);
	Size2 size = get_size();
	draw_rect(Rect2(Point2(), size), Color(0.11, 0.11, 0.13));
	if (!has_source) {
		Ref<Font> font = get_font("font", "Label");
		String msg = TTR("No atlas selected");
		draw_string(font, ((size - font->get_string_size(msg)) / 2).floor() + Vector2(0, font->get_ascent()), msg, Color(1, 1, 1, 0.5));
		return;
	}

	Size2 ts = source.texture->get_size();
	Rect2 tex_rect(ofs, ts * zoom);
	// Checkerboard under the texture so transparent areas read as such.
	draw_rect(tex_rect, Color(0.2, 0.2, 0.22));
	draw_texture_rect_region(source.texture, tex_rect, Rect2(Point2(), ts));

	Vector2 grid = get_grid_size();
	Vector2 step = (source.tile_size + source.separation) * zoom;
	Color grid_col(1, 1, 1, 0.12);
	for (int x = 0; x < grid.x; x++) {
		for (int y = 0; y < grid.y; y++) {
			Rect2 r = cells_region(Rect2(x, y, 1, 1));
			Rect2 sr(ofs + r.pos * zoom, r.size * zoom);
			// Cells that are not (part of) a tile are dimmed, as in Godot 4.
			if (tileset.is_valid() && tile_at_cell(Vector2(x, y)) < 0)
				draw_rect(sr, Color(0, 0, 0, 0.5));
			if (step.x >= 4 && step.y >= 4)
				_draw_rect_outline(sr, grid_col, 1);
		}
	}

	if (tileset.is_valid()) {
		List<int> ids;
		tileset->get_tile_list(&ids);
		Ref<Font> font = get_font("font", "Label");
		bool ids_fit = show_ids && source.tile_size.x * zoom >= 20;
		for (List<int>::Element *E = ids.front(); E; E = E->next()) {
			int id = E->get();
			if (tileset->tile_get_texture(id) != source.texture)
				continue;
			Rect2 r = _tile_region(tileset, id);
			Rect2 sr(ofs + r.pos * zoom, r.size * zoom);
			bool sel = selected.has(id);
			draw_rect(sr, sel ? Color(1, 0.75, 0.2, 0.28) : Color(0.35, 0.65, 1, 0.12));
			_draw_rect_outline(sr, sel ? Color(1, 0.8, 0.25, 1) : Color(0.35, 0.65, 1, 0.75), sel ? 2 : 1);
			if (ids_fit)
				draw_string(font, sr.pos + Vector2(3, font->get_ascent() + 1), itos(id), Color(1, 1, 1, 0.85));
		}
	}

	if (drag == DRAG_SELECT) {
		Vector2 a(MIN(drag_from.x, drag_to.x), MIN(drag_from.y, drag_to.y));
		Vector2 b(MAX(drag_from.x, drag_to.x), MAX(drag_from.y, drag_to.y));
		Rect2 r = cells_region(Rect2(a, b - a + Vector2(1, 1)));
		Rect2 sr(ofs + r.pos * zoom, r.size * zoom);
		draw_rect(sr, Color(1, 1, 1, 0.15));
		_draw_rect_outline(sr, Color(1, 1, 1, 0.9), 2);
	} else if (hover_valid) {
		Rect2 r = cells_region(Rect2(hover, Vector2(1, 1)));
		_draw_rect_outline(Rect2(ofs + r.pos * zoom, r.size * zoom), Color(1, 1, 1, 0.6), 1);
	}
}

void TileAtlasView::_input_event(const InputEvent &p_event) {

	if (p_event.type == InputEvent::MOUSE_BUTTON) {

		const InputEventMouseButton &mb = p_event.mouse_button;
		Point2 pos(mb.x, mb.y);

		if (mb.pressed && (mb.button_index == BUTTON_WHEEL_UP || mb.button_index == BUTTON_WHEEL_DOWN)) {
			// As in Godot 4: Ctrl + wheel zooms, the wheel scrolls (Shift: sideways).
			float dir = mb.button_index == BUTTON_WHEEL_UP ? 1 : -1;
			if (mb.mod.control)
				_zoom_at(dir > 0 ? 1.25 : 0.8, pos);
			else {
				float amount = 48 * dir;
				if (mb.mod.shift)
					ofs.x += amount;
				else
					ofs.y += amount;
				update();
			}
			accept_event();
			return;
		}

		bool pan_button = mb.button_index == BUTTON_MIDDLE || (mb.button_index == BUTTON_LEFT && Input::get_singleton()->is_key_pressed(KEY_SPACE));
		if (pan_button) {
			if (mb.pressed) {
				drag = DRAG_PAN;
				pan_mouse = pos;
				pan_ofs = ofs;
			} else if (drag == DRAG_PAN) {
				drag = DRAG_NONE;
			}
			accept_event();
			return;
		}

		if (!has_source)
			return;

		Vector2 cell = cell_at_texel(_texel_at(pos));
		Vector2 grid = get_grid_size();
		cell = Vector2(CLAMP(cell.x, 0, MAX(grid.x - 1, 0)), CLAMP(cell.y, 0, MAX(grid.y - 1, 0)));

		if (mb.button_index == BUTTON_LEFT) {
			if (mb.pressed) {
				drag = DRAG_SELECT;
				drag_from = cell;
				drag_to = cell;
				update();
			} else if (drag == DRAG_SELECT) {
				drag = DRAG_NONE;
				Vector2 a(MIN(drag_from.x, drag_to.x), MIN(drag_from.y, drag_to.y));
				Vector2 b(MAX(drag_from.x, drag_to.x), MAX(drag_from.y, drag_to.y));
				update();
				emit_signal("cells_selected", Rect2(a, b - a + Vector2(1, 1)), mb.mod.shift, mb.mod.control);
			}
			accept_event();
		} else if (mb.button_index == BUTTON_RIGHT && mb.pressed) {
			emit_signal("cells_rmb", Rect2(cell, Vector2(1, 1)));
			accept_event();
		}

	} else if (p_event.type == InputEvent::MOUSE_MOTION) {

		const InputEventMouseMotion &mm = p_event.mouse_motion;
		Point2 pos(mm.x, mm.y);

		if (drag == DRAG_PAN) {
			ofs = pan_ofs + pos - pan_mouse;
			update();
			return;
		}
		if (!has_source)
			return;

		Vector2 texel = _texel_at(pos);
		Vector2 cell = cell_at_texel(texel);
		Vector2 grid = get_grid_size();
		bool inside = cell.x >= 0 && cell.y >= 0 && cell.x < grid.x && cell.y < grid.y;

		if (drag == DRAG_SELECT) {
			drag_to = Vector2(CLAMP(cell.x, 0, MAX(grid.x - 1, 0)), CLAMP(cell.y, 0, MAX(grid.y - 1, 0)));
			update();
		} else if (inside != hover_valid || cell != hover) {
			hover_valid = inside;
			hover = cell;
			update();
		}
	}
}

void TileAtlasView::_bind_methods() {

	ObjectTypeDB::bind_method("_input_event", &TileAtlasView::_input_event);

	ADD_SIGNAL(MethodInfo("cells_selected", PropertyInfo(Variant::RECT2, "cells"), PropertyInfo(Variant::BOOL, "shift"), PropertyInfo(Variant::BOOL, "ctrl")));
	ADD_SIGNAL(MethodInfo("cells_rmb", PropertyInfo(Variant::RECT2, "cells")));
}

TileAtlasView::TileAtlasView() {

	has_source = false;
	zoom = 1;
	fit_pending = false;
	show_ids = true;
	drag = DRAG_NONE;
	hover_valid = false;
	VisualServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), true);
	set_focus_mode(FOCUS_CLICK);
	set_custom_minimum_size(Size2(100, 100) * EDSCALE);
}
