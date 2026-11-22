/*************************************************************************/
/*  tile_set_editor_plugin.cpp                                           */
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

#include "tile_set_editor_plugin.h"

#include "io/resource_loader.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/split_container.h"
#include "scene/2d/physics_body_2d.h"
#include "scene/2d/sprite.h"
#include "scene/2d/tile_map.h"
#include "scene/resources/palette_texture.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "servers/visual_server.h"
void TileSetEditor::_import_node(Node *p_node, Ref<TileSet> p_library) {

	for (int i = 0; i < p_node->get_child_count(); i++) {

		Node *child = p_node->get_child(i);

		if (!child->cast_to<Sprite>()) {
			if (child->get_child_count() > 0) {
				_import_node(child, p_library);
			}

			continue;
		}

		Sprite *mi = child->cast_to<Sprite>();
		Ref<Texture> texture = mi->get_texture();
		Ref<CanvasItemMaterial> material = mi->get_material();

		if (texture.is_null())
			continue;

		int id = p_library->find_tile_by_name(mi->get_name());
		if (id < 0) {

			id = p_library->get_last_unused_tile_id();
			p_library->create_tile(id);
			p_library->tile_set_name(id, mi->get_name());
		}

		p_library->tile_set_texture(id, texture);
		p_library->tile_set_material(id, material);

		p_library->tile_set_modulate(id, mi->get_modulate());

		Vector2 phys_offset;
		Size2 s;

		if (mi->is_region()) {
			s = mi->get_region_rect().size;
			p_library->tile_set_region(id, mi->get_region_rect());
		} else {
			const int frame = mi->get_frame();
			const int hframes = mi->get_hframes();
			s = texture->get_size() / Size2(hframes, mi->get_vframes());
			p_library->tile_set_region(id, Rect2(Vector2(frame % hframes, frame / hframes) * s, s));
		}

		if (mi->is_centered()) {
			phys_offset += -s / 2;
		}

		Vector<Ref<Shape2D> > collisions;
		Ref<NavigationPolygon> nav_poly;
		Ref<OccluderPolygon2D> occluder;
		bool one_way_ok = true;
		Variant one_way_dir;
		float one_way_max_depth = 0.0f;

		for (int j = 0; j < mi->get_child_count(); j++) {

			Node *child2 = mi->get_child(j);

			if (child2->cast_to<NavigationPolygonInstance>())
				nav_poly = child2->cast_to<NavigationPolygonInstance>()->get_navigation_polygon();

			if (child2->cast_to<LightOccluder2D>())
				occluder = child2->cast_to<LightOccluder2D>()->get_occluder_polygon();

			if (!child2->cast_to<StaticBody2D>())
				continue;
			StaticBody2D *sb = child2->cast_to<StaticBody2D>();
			int shape_count = sb->get_shape_count();
			if (shape_count == 0)
				continue;
			for (int shape_index = 0; shape_index < shape_count; ++shape_index) {
				Ref<Shape2D> collision = sb->get_shape(shape_index);
				if (collision.is_valid()) {
					collisions.push_back(collision);
				}
			}

			phys_offset -= sb->get_pos();

			if (one_way_ok) {
				Vector2 curr_dir = sb->get_one_way_collision_direction();
				float curr_max_depth = sb->get_one_way_collision_max_depth();
				if (one_way_dir == Variant()) {
					one_way_dir = curr_dir;
					one_way_max_depth = curr_max_depth;
				} else {
					if (curr_dir != one_way_dir || curr_max_depth != one_way_max_depth) {
						one_way_ok = false;
						WARN_PRINT(String("Mismatch in one-way collision parameters for " + child->get_name()).utf8().get_data());
					}
				}
			}
		}

		if (collisions.size()) {

			p_library->tile_set_shapes(id, collisions);
			p_library->tile_set_shape_offset(id, -phys_offset);
			if (one_way_ok && one_way_dir != Variant()) {
				p_library->tile_set_one_way_collision_direction(id, one_way_dir);
				p_library->tile_set_one_way_collision_max_depth(id, one_way_max_depth);
			}
		} else {
			p_library->tile_set_shape_offset(id, Vector2());
		}

		p_library->tile_set_texture_offset(id, mi->get_offset());
		p_library->tile_set_navigation_polygon(id, nav_poly);
		p_library->tile_set_light_occluder(id, occluder);
		p_library->tile_set_occluder_offset(id, -phys_offset);
		p_library->tile_set_navigation_polygon_offset(id, -phys_offset);
	}
}

void TileSetEditor::_import_scene(Node *p_scene, Ref<TileSet> p_library, bool p_merge) {
	if (!p_merge)
		p_library->clear();

	_import_node(p_scene, p_library);
}

//////////////////////////////////////////////////////////////////////////////
// Tile proxy

void TileSetEditorTileProxy::setup(const Ref<TileSet> &p_tileset, int p_id) {

	tileset = p_tileset;
	id = p_id;
}

// The tile's (first) collision shape is edited right here: its properties are listed under
// "shape_settings/" just after "shape", instead of opening the shape in another inspector page.
#define SHAPE_SETTINGS "shape_settings/"

Ref<Shape2D> TileSetEditorTileProxy::_get_shape() const {

	if (tileset.is_null() || !tileset->has_tile(id))
		return Ref<Shape2D>();
	return tileset->tile_get_shape(id);
}

static bool _shape_setting_listed(const PropertyInfo &p_info) {

	if (!(p_info.usage & PROPERTY_USAGE_EDITOR) || (p_info.usage & PROPERTY_USAGE_CATEGORY))
		return false;
	return !p_info.name.begins_with("resource/") && !p_info.name.begins_with("script/");
}

bool TileSetEditorTileProxy::_set(const StringName &p_name, const Variant &p_value) {

	if (tileset.is_null() || !tileset->has_tile(id))
		return false;
	String n = p_name;
	if (n.begins_with(SHAPE_SETTINGS)) {
		Ref<Shape2D> shape = _get_shape();
		if (shape.is_null())
			return false;
		bool valid = false;
		shape->set(n.substr(String(SHAPE_SETTINGS).length(), n.length()), p_value, &valid);
		return valid;
	}
	bool valid = false;
	setting = true; // the TileSet's "changed" comes back while this runs: see TileSetEditor::_tileset_changed
	tileset->set(itos(id) + "/" + n, p_value, &valid);
	setting = false;
	if (valid && (n == "shape" || n == "shapes"))
		call_deferred("_notify_list_changed"); // the shape's settings below it change
	return valid;
}

bool TileSetEditorTileProxy::_get(const StringName &p_name, Variant &r_ret) const {

	if (tileset.is_null() || !tileset->has_tile(id))
		return false;
	String n = p_name;
	if (n.begins_with(SHAPE_SETTINGS)) {
		Ref<Shape2D> shape = _get_shape();
		if (shape.is_null())
			return false;
		bool valid = false;
		r_ret = shape->get(n.substr(String(SHAPE_SETTINGS).length(), n.length()), &valid);
		return valid;
	}
	bool valid = false;
	r_ret = tileset->get(itos(id) + "/" + n, &valid);
	return valid;
}

void TileSetEditorTileProxy::_get_property_list(List<PropertyInfo> *p_list) const {

	if (tileset.is_null() || !tileset->has_tile(id))
		return;
	String pre = itos(id) + "/";
	List<PropertyInfo> all;
	tileset->get_property_list(&all);
	for (List<PropertyInfo>::Element *E = all.front(); E; E = E->next()) {
		if (!E->get().name.begins_with(pre))
			continue;
		PropertyInfo pi = E->get();
		pi.name = pi.name.substr(pre.length(), pi.name.length());
		p_list->push_back(pi);
		if (pi.name != "shape")
			continue;
		Ref<Shape2D> shape = _get_shape();
		if (shape.is_null())
			continue;
		List<PropertyInfo> shape_props;
		shape->get_property_list(&shape_props);
		for (List<PropertyInfo>::Element *F = shape_props.front(); F; F = F->next()) {
			if (!_shape_setting_listed(F->get()))
				continue;
			PropertyInfo spi = F->get();
			spi.name = SHAPE_SETTINGS + spi.name;
			spi.usage = PROPERTY_USAGE_EDITOR; // not stored through the proxy (the shape is)
			p_list->push_back(spi);
		}
	}
}

void TileSetEditorTileProxy::_bind_methods() {

	ObjectTypeDB::bind_method("_notify_list_changed", &TileSetEditorTileProxy::_notify_list_changed);
}

//////////////////////////////////////////////////////////////////////////////
// Tile preview

TileSetTilePreview::TileSetTilePreview() {

	tile = -1;
	cell_size = Size2(16, 16);
	origin = TileMap::TILE_ORIGIN_TOP_LEFT;
	undo_redo = NULL;
	view_zoom = 1;
	drag = DRAG_NONE;
	set_focus_mode(FOCUS_CLICK);

	manual_zoom = 0;
	panning = false;
	zoom_buttons = memnew(HBoxContainer);
	add_child(zoom_buttons);
	zoom_less = memnew(ToolButton);
	zoom_less->set_tooltip(TTR("Zoom Out"));
	zoom_less->connect("pressed", this, "_zoom_less");
	zoom_buttons->add_child(zoom_less);
	zoom_reset = memnew(ToolButton);
	zoom_reset->set_tooltip(TTR("Fit the tile (zoom and position)"));
	zoom_reset->connect("pressed", this, "_zoom_fit");
	zoom_buttons->add_child(zoom_reset);
	zoom_more = memnew(ToolButton);
	zoom_more->set_tooltip(TTR("Zoom In"));
	zoom_more->connect("pressed", this, "_zoom_more");
	zoom_buttons->add_child(zoom_more);
	set_tooltip(TTR("Ctrl + wheel: zoom. Wheel: scroll (Shift: sideways). Middle button: pan. Drag the shape to move it, its handle to resize it."));
}

// Zoom levels: whole numbers from 1x up, so pixel art stays sharp.
static const float tile_preview_zooms[] = { 0.25, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };
static const int tile_preview_zoom_count = sizeof(tile_preview_zooms) / sizeof(tile_preview_zooms[0]);

void TileSetTilePreview::_zoom_at(float p_zoom, const Vector2 &p_screen_pos) {

	if (manual_zoom <= 0) { // leaving the fitted view: start from what is on screen
		manual_zoom = view_zoom;
		manual_ofs = view_ofs;
	}
	// Keep the point under the cursor in place.
	Vector2 tile_pos = (p_screen_pos - manual_ofs) / manual_zoom;
	manual_zoom = p_zoom;
	manual_ofs = (p_screen_pos - tile_pos * manual_zoom).floor();
	update();
}

void TileSetTilePreview::_zoom_step(int p_dir, const Vector2 &p_screen_pos) {

	float current = manual_zoom > 0 ? manual_zoom : view_zoom;
	float next = current;
	if (p_dir > 0) {
		for (int i = 0; i < tile_preview_zoom_count; i++) {
			if (tile_preview_zooms[i] > current + 0.001) {
				next = tile_preview_zooms[i];
				break;
			}
		}
	} else {
		for (int i = tile_preview_zoom_count - 1; i >= 0; i--) {
			if (tile_preview_zooms[i] < current - 0.001) {
				next = tile_preview_zooms[i];
				break;
			}
		}
	}
	if (next != current)
		_zoom_at(next, p_screen_pos);
}

void TileSetTilePreview::_zoom_less() {

	_zoom_step(-1, get_size() / 2);
}

void TileSetTilePreview::_zoom_more() {

	_zoom_step(1, get_size() / 2);
}

void TileSetTilePreview::_zoom_fit() {

	manual_zoom = 0;
	update();
}

Size2 TileSetTilePreview::_tile_size() const {

	Ref<Texture> tex = tileset->tile_get_texture(tile);
	Rect2 region = tileset->tile_get_region(tile);
	return region != Rect2() ? region.size : (tex.is_valid() ? tex->get_size() : cell_size);
}

Vector2 TileSetTilePreview::_shape_pos() const {

	// The TileMap places the shape like the texture, moved by Shape Offset instead of Tex Offset.
	return _texture_pos(_tile_size(), tileset->tile_get_shape_offset(tile));
}

Vector2 TileSetTilePreview::_handle_pos() const {

	Ref<Shape2D> shape = tileset->tile_get_shape(tile);
	if (shape.is_null())
		return Vector2();
	if (shape->cast_to<RectangleShape2D>())
		return shape->cast_to<RectangleShape2D>()->get_extents();
	if (shape->cast_to<CircleShape2D>())
		return Vector2(shape->cast_to<CircleShape2D>()->get_radius(), 0);
	return Vector2();
}

void TileSetTilePreview::add_rect_shape() {

	if (tileset.is_null() || !tileset->has_tile(tile) || !undo_redo)
		return;
	Ref<RectangleShape2D> rect;
	rect.instance();
	rect->set_extents(cell_size / 2);
	// Shape Offset that puts the rectangle's center (its origin) at the cell's center.
	Vector2 ofs = cell_size / 2 - _texture_pos(_tile_size(), Vector2());
	undo_redo->create_action(TTR("Add Tile Collision Shape"));
	undo_redo->add_do_method(tileset.ptr(), "tile_set_shape", tile, rect);
	undo_redo->add_do_method(tileset.ptr(), "tile_set_shape_offset", tile, ofs);
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_shapes", tile, tileset->call("tile_get_shapes", tile));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_shape_offset", tile, tileset->tile_get_shape_offset(tile));
	undo_redo->commit_action();
	emit_signal("shape_edited");
}

void TileSetTilePreview::_input_event(const InputEvent &p_event) {

	if (tileset.is_null() || !tileset->has_tile(tile))
		return;
	Ref<Shape2D> shape = tileset->tile_get_shape(tile);

	// View, as in the atlas grid next to it (and Godot 4): Ctrl + wheel zooms towards the
	// cursor, the wheel scrolls (Shift: sideways), the middle button pans.
	if (p_event.type == InputEvent::MOUSE_BUTTON && p_event.mouse_button.pressed && drag == DRAG_NONE &&
			(p_event.mouse_button.button_index == BUTTON_WHEEL_UP || p_event.mouse_button.button_index == BUTTON_WHEEL_DOWN)) {
		int dir = p_event.mouse_button.button_index == BUTTON_WHEEL_UP ? 1 : -1;
		if (p_event.mouse_button.mod.control) {
			_zoom_step(dir, Vector2(p_event.mouse_button.x, p_event.mouse_button.y));
		} else {
			if (manual_zoom <= 0) {
				manual_zoom = view_zoom;
				manual_ofs = view_ofs;
			}
			if (p_event.mouse_button.mod.shift)
				manual_ofs.x += 48 * dir;
			else
				manual_ofs.y += 48 * dir;
			update();
		}
		accept_event();
		return;
	}
	if (p_event.type == InputEvent::MOUSE_BUTTON && p_event.mouse_button.button_index == BUTTON_MIDDLE) {
		panning = p_event.mouse_button.pressed;
		pan_last = Vector2(p_event.mouse_button.x, p_event.mouse_button.y);
		if (panning && manual_zoom <= 0) {
			manual_zoom = view_zoom;
			manual_ofs = view_ofs;
		}
		accept_event();
		return;
	}
	if (p_event.type == InputEvent::MOUSE_MOTION && panning) {
		Vector2 p(p_event.mouse_motion.x, p_event.mouse_motion.y);
		manual_ofs += p - pan_last;
		pan_last = p;
		update();
		accept_event();
		return;
	}

	if (p_event.type == InputEvent::MOUSE_BUTTON && p_event.mouse_button.button_index == BUTTON_LEFT) {
		Vector2 p = (Vector2(p_event.mouse_button.x, p_event.mouse_button.y) - view_ofs) / view_zoom;
		if (p_event.mouse_button.pressed) {
			if (shape.is_null())
				return;
			Vector2 local = p - _shape_pos();
			Vector2 h = _handle_pos();
			drag = DRAG_NONE;
			if (h != Vector2() && (local - h).length() * view_zoom <= 6) {
				drag = shape->cast_to<RectangleShape2D>() ? DRAG_EXTENTS : DRAG_RADIUS;
				drag_start_value = drag == DRAG_EXTENTS ? Variant(shape->cast_to<RectangleShape2D>()->get_extents()) : Variant(shape->cast_to<CircleShape2D>()->get_radius());
			} else if (shape->get_rect().grow(2 / view_zoom).has_point(local)) {
				drag = DRAG_MOVE;
			}
			drag_from = p;
			drag_start_offset = tileset->tile_get_shape_offset(tile);
			accept_event();
		} else if (drag != DRAG_NONE) {
			// One undoable action for the whole drag (the values were applied live).
			if (undo_redo && shape.is_valid()) {
				if (drag == DRAG_MOVE) {
					undo_redo->create_action(TTR("Move Tile Collision Shape"));
					undo_redo->add_do_method(tileset.ptr(), "tile_set_shape_offset", tile, tileset->tile_get_shape_offset(tile));
					undo_redo->add_undo_method(tileset.ptr(), "tile_set_shape_offset", tile, drag_start_offset);
				} else {
					String prop = drag == DRAG_EXTENTS ? "extents" : "radius";
					undo_redo->create_action(TTR("Resize Tile Collision Shape"));
					undo_redo->add_do_property(shape.ptr(), prop, shape->get(prop));
					undo_redo->add_undo_property(shape.ptr(), prop, drag_start_value);
				}
				undo_redo->add_do_method(this, "update");
				undo_redo->add_undo_method(this, "update");
				undo_redo->commit_action();
			}
			drag = DRAG_NONE;
			update(); // refit the view
			emit_signal("shape_edited");
			accept_event();
		}
	}

	if (p_event.type == InputEvent::MOUSE_MOTION && drag != DRAG_NONE && shape.is_valid()) {
		Vector2 p = (Vector2(p_event.mouse_motion.x, p_event.mouse_motion.y) - view_ofs) / view_zoom;
		Vector2 delta = (p - drag_from).snapped(Vector2(1, 1)); // whole pixels
		if (drag == DRAG_MOVE) {
			tileset->tile_set_shape_offset(tile, drag_start_offset + delta);
		} else if (drag == DRAG_EXTENTS) {
			Vector2 e = drag_start_value;
			shape->cast_to<RectangleShape2D>()->set_extents(Vector2(MAX(1, e.x + delta.x), MAX(1, e.y + delta.y)));
		} else if (drag == DRAG_RADIUS) {
			float r = drag_start_value;
			shape->cast_to<CircleShape2D>()->set_radius(MAX(1, r + delta.x));
		}
		update();
		accept_event();
	}
}

void TileSetTilePreview::_bind_methods() {

	ObjectTypeDB::bind_method("_input_event", &TileSetTilePreview::_input_event);
	ObjectTypeDB::bind_method("_zoom_less", &TileSetTilePreview::_zoom_less);
	ObjectTypeDB::bind_method("_zoom_more", &TileSetTilePreview::_zoom_more);
	ObjectTypeDB::bind_method("_zoom_fit", &TileSetTilePreview::_zoom_fit);
	ADD_SIGNAL(MethodInfo("shape_edited"));
}

void TileSetTilePreview::set_tile(const Ref<TileSet> &p_tileset, int p_tile) {

	if (p_tileset != tileset || p_tile != tile)
		manual_zoom = 0; // another tile: fit it
	tileset = p_tileset;
	tile = p_tile;
	update();
}

void TileSetTilePreview::set_cell(const Size2 &p_cell_size, int p_origin) {

	cell_size = Size2(MAX(1, p_cell_size.x), MAX(1, p_cell_size.y));
	origin = p_origin;
	update();
}

Vector2 TileSetTilePreview::_cell_anchor() const {

	if (origin == TileMap::TILE_ORIGIN_CENTER)
		return cell_size / 2;
	if (origin == TileMap::TILE_ORIGIN_BOTTOM_LEFT)
		return Vector2(0, cell_size.y);
	return Vector2();
}

// Where the TileMap puts something of size p_size moved by p_ofs (as in TileMap's
// _update_dirty_quadrants, no flips), relative to the cell's top-left.
Vector2 TileSetTilePreview::_texture_pos(const Size2 &p_size, const Vector2 &p_ofs) const {

	if (origin == TileMap::TILE_ORIGIN_CENTER)
		return cell_size / 2 - p_size / 2 + p_ofs;
	if (origin == TileMap::TILE_ORIGIN_BOTTOM_LEFT)
		return Vector2(0, cell_size.y) + p_ofs;
	return p_ofs;
}

void TileSetTilePreview::_notification(int p_what) {

	if (p_what == NOTIFICATION_ENTER_TREE) {
		zoom_less->set_icon(get_icon("ZoomLess", "EditorIcons"));
		zoom_reset->set_icon(get_icon("ZoomReset", "EditorIcons"));
		zoom_more->set_icon(get_icon("ZoomMore", "EditorIcons"));
	}
	if (p_what == NOTIFICATION_RESIZED || p_what == NOTIFICATION_ENTER_TREE) {
		Size2 bs = zoom_buttons->get_combined_minimum_size();
		zoom_buttons->set_pos(Point2(get_size().x - bs.x - 2, 2));
		zoom_buttons->set_size(bs);
	}
	if (p_what != NOTIFICATION_DRAW)
		return;

	VisualServer::get_singleton()->canvas_item_set_clip(get_canvas_item(), true); // the grid goes past the edges
	Size2 area = get_size();
	draw_rect(Rect2(Point2(), area), Color(0.12, 0.12, 0.14));
	if (tileset.is_null() || !tileset->has_tile(tile))
		return;

	Ref<Texture> tex = tileset->tile_get_texture(tile);
	Rect2 region = tileset->tile_get_region(tile);
	Size2 s = region != Rect2() ? region.size : (tex.is_valid() ? tex->get_size() : cell_size);
	Vector2 tex_pos = _texture_pos(s, tileset->tile_get_texture_offset(tile));
	Ref<Shape2D> shape = tileset->tile_get_shape(tile);
	// The TileMap places the shape like the texture, moved by Shape Offset instead of Tex Offset.
	Vector2 shape_pos = _texture_pos(s, tileset->tile_get_shape_offset(tile));

	// What to fit: the cell, the texture, the shape and a margin.
	Rect2 bounds(Point2(), cell_size);
	bounds = bounds.merge(Rect2(tex_pos, s));
	if (shape.is_valid()) {
		Rect2 sr = shape->get_rect();
		sr.pos += shape_pos;
		bounds = bounds.merge(sr);
	}
	bounds = bounds.grow(MAX(cell_size.x, cell_size.y) * 0.5);
	float zoom = MIN(area.x / bounds.size.x, area.y / bounds.size.y);
	if (zoom >= 1)
		zoom = Math::floor(zoom);
	zoom = MIN(zoom, 16.0f); // a wide preview shows the tile bigger
	Vector2 ofs = (area - bounds.size * zoom) / 2 - bounds.pos * zoom; // screen = ofs + world * zoom
	ofs = ofs.floor();
	if (manual_zoom > 0) { // zoomed or panned by the user
		zoom = manual_zoom;
		ofs = manual_ofs;
	}
	if (drag != DRAG_NONE) {
		// Keep the view still while dragging: refitting it to a growing shape would feed back
		// into the drag.
		ofs = view_ofs;
		zoom = view_zoom;
	}
	view_ofs = ofs;
	view_zoom = zoom;

	// Neighbouring cells, faint; the tile's cell, bright.
	Color grid_col(1, 1, 1, 0.08);
	for (int y = -2; y <= 2; y++) {
		for (int x = -2; x <= 2; x++) {
			Rect2 c(ofs + Vector2(x, y) * cell_size * zoom, cell_size * zoom);
			draw_line(c.pos, c.pos + Vector2(c.size.x, 0), grid_col);
			draw_line(c.pos, c.pos + Vector2(0, c.size.y), grid_col);
		}
	}
	Rect2 cell(ofs, cell_size * zoom);
	draw_rect(cell, Color(1, 1, 1, 0.06));

	if (tex.is_valid()) {
		Rect2 dst(ofs + tex_pos * zoom, s * zoom);
		if (region != Rect2())
			draw_texture_rect_region(tex, dst, region, tileset->tile_get_modulate(tile));
		else
			draw_texture_rect(tex, dst, false, tileset->tile_get_modulate(tile));
		Color tc(1, 0.8, 0.2, 0.7);
		draw_line(dst.pos, dst.pos + Vector2(dst.size.x, 0), tc);
		draw_line(dst.pos, dst.pos + Vector2(0, dst.size.y), tc);
		draw_line(dst.pos + dst.size, dst.pos + Vector2(dst.size.x, 0), tc);
		draw_line(dst.pos + dst.size, dst.pos + Vector2(0, dst.size.y), tc);
		draw_circle(dst.pos, 2, tc); // the texture's top-left (Tex Offset moves it)
	}

	Color cc(1, 1, 1, 0.6);
	draw_line(cell.pos, cell.pos + Vector2(cell.size.x, 0), cc);
	draw_line(cell.pos, cell.pos + Vector2(0, cell.size.y), cc);
	draw_line(cell.pos + cell.size, cell.pos + Vector2(cell.size.x, 0), cc);
	draw_line(cell.pos + cell.size, cell.pos + Vector2(0, cell.size.y), cc);

	if (shape.is_valid()) {
		draw_set_transform(ofs + shape_pos * zoom, 0, Size2(zoom, zoom));
		shape->draw(get_canvas_item(), Color(0.0, 0.6, 0.7, 0.5));
		draw_set_transform(Point2(), 0, Size2(1, 1));
		Vector2 h = _handle_pos();
		if (h != Vector2()) {
			// Drag the shape to move it, this handle to resize it.
			Vector2 hp = (ofs + (shape_pos + h) * zoom).floor();
			draw_rect(Rect2(hp - Vector2(4, 4), Size2(8, 8)), Color(1, 1, 1));
			draw_rect(Rect2(hp - Vector2(3, 3), Size2(6, 6)), Color(0.0, 0.6, 0.7));
		}
	}

	// The cell's origin for the TileMap's Tile Origin.
	Vector2 a = (ofs + _cell_anchor() * zoom).floor();
	Color oc(1, 0.3, 0.3);
	draw_line(a - Vector2(6, 0), a + Vector2(7, 0), oc, 2);
	draw_line(a - Vector2(0, 6), a + Vector2(0, 7), oc, 2);

	// Zoom level, bottom left (the buttons are top right; the preview can be narrow).
	Ref<Font> font = get_font("font", "Label");
	String zt = itos(int(zoom * 100 + 0.5)) + "%" + (manual_zoom > 0 ? "" : " (" + TTR("fit") + ")");
	draw_string(font, Point2(6, area.y - 6 - font->get_descent()), zt, Color(1, 1, 1, 0.7));
}

//////////////////////////////////////////////////////////////////////////////
// TileSet editor

TileSetEditor *TileSetEditor::singleton = NULL;

TileSetEditorTileProxy *TileSetEditor::_get_proxy(int p_id) {

	if (!proxies.has(p_id)) {
		TileSetEditorTileProxy *p = memnew(TileSetEditorTileProxy);
		p->setup(tileset, p_id);
		proxies[p_id] = p;
	}
	return proxies[p_id];
}

void TileSetEditor::edit(const Ref<TileSet> &p_tileset) {

	if (tileset == p_tileset)
		return;
	if (tileset.is_valid())
		tileset->disconnect("changed", this, "_tileset_changed");
	tileset = p_tileset;
	if (tileset.is_valid())
		tileset->connect("changed", this, "_tileset_changed");

	tile_properties->edit(NULL);
	for (Map<int, TileSetEditorTileProxy *>::Element *E = proxies.front(); E; E = E->next())
		memdelete(E->get());
	proxies.clear();

	selected_tile = -1;
	current_source = -1;
	atlas_view->set_tileset(tileset);
	atlas_view->clear_source();
	_update_sources();
	if (sources.size())
		_source_selected(0);
	_select_tile(-1);
}

void TileSetEditor::_tileset_changed() {

	if (tileset.is_null())
		return;
	_update_sources();
	if (selected_tile >= 0 && !tileset->has_tile(selected_tile))
		_select_tile(-1);
	else if (selected_tile >= 0) {
		// A change made in the properties panel itself needs no rebuild of it: rebuilding would
		// drop the focus of the field being edited (Up/Down would step only once).
		TileSetEditorTileProxy *proxy = _get_proxy(selected_tile);
		if (!proxy->is_setting())
			proxy->notify_changed();
		tile_label->set_text(TTR("Tile") + " " + itos(selected_tile) + ": " + tileset->tile_get_name(selected_tile));
	}
	atlas_view->update();
	_update_preview();
}

void TileSetEditor::_update_sources() {

	Ref<Texture> current;
	if (current_source >= 0 && current_source < sources.size())
		current = sources[current_source].texture;

	TileAtlasView::get_sources(tileset, &sources);

	source_list->clear();
	int new_current = -1;
	for (int i = 0; i < sources.size(); i++) {
		String name = sources[i].texture->get_path().get_file();
		if (name == "")
			name = TTR("Texture") + " " + itos(i);
		if (sources[i].atlas < 0)
			name += " *";
		source_list->add_item(name, sources[i].texture);
		source_list->set_item_tooltip(i, sources[i].texture->get_path() + (sources[i].atlas < 0 ? "\n" + TTR("* Grid guessed from the tiles; changing it saves it in the TileSet.") : String()));
		if (sources[i].texture == current)
			new_current = i;
	}

	if (new_current >= 0) {
		current_source = new_current;
		source_list->select(new_current);
		atlas_view->set_source(sources[new_current]);
		_update_grid_fields();
	} else if (current.is_valid() || current_source >= sources.size()) {
		current_source = -1;
		atlas_view->clear_source();
		if (sources.size())
			_source_selected(0);
	}
}

void TileSetEditor::_source_selected(int p_index) {

	if (p_index < 0 || p_index >= sources.size())
		return;
	current_source = p_index;
	source_list->select(p_index);
	atlas_view->set_source(sources[p_index]);
	_update_grid_fields();
}

void TileSetEditor::_update_grid_fields() {

	updating = true;
	bool has = current_source >= 0;
	TileAtlasView::Source s = has ? sources[current_source] : TileAtlasView::Source();
	size_x->set_val(has ? s.tile_size.x : 16);
	size_y->set_val(has ? s.tile_size.y : 16);
	margin_x->set_val(s.margin.x);
	margin_y->set_val(s.margin.y);
	sep_x->set_val(s.separation.x);
	sep_y->set_val(s.separation.y);
	updating = false;
}

void TileSetEditor::_register_source_atlas(UndoRedo *p_ur) {

	// A texture shown with a guessed grid: give it atlas settings, so the grid is saved.
	const TileAtlasView::Source &s = sources[current_source];
	if (s.atlas >= 0)
		return;
	int idx = tileset->atlas_get_count();
	p_ur->add_do_method(tileset.ptr(), "atlas_add", s.texture);
	p_ur->add_do_method(tileset.ptr(), "atlas_set_tile_size", idx, s.tile_size);
	p_ur->add_undo_method(tileset.ptr(), "atlas_remove", idx);
}

void TileSetEditor::_grid_changed(double) {

	if (updating || tileset.is_null() || current_source < 0)
		return;
	const TileAtlasView::Source &s = sources[current_source];
	Vector2 size(size_x->get_val(), size_y->get_val());
	Vector2 margin(margin_x->get_val(), margin_y->get_val());
	Vector2 sep(sep_x->get_val(), sep_y->get_val());
	if (size == s.tile_size && margin == s.margin && sep == s.separation)
		return;

	UndoRedo *ur = editor->get_undo_redo();
	ur->create_action(TTR("Change Atlas Grid"), UndoRedo::MERGE_ENDS);
	int idx = s.atlas;
	if (idx < 0) {
		idx = tileset->atlas_get_count();
		ur->add_do_method(tileset.ptr(), "atlas_add", s.texture);
		ur->add_undo_method(tileset.ptr(), "atlas_remove", idx);
	} else {
		ur->add_undo_method(tileset.ptr(), "atlas_set_tile_size", idx, s.tile_size);
		ur->add_undo_method(tileset.ptr(), "atlas_set_margin", idx, s.margin);
		ur->add_undo_method(tileset.ptr(), "atlas_set_separation", idx, s.separation);
	}
	ur->add_do_method(tileset.ptr(), "atlas_set_tile_size", idx, size);
	ur->add_do_method(tileset.ptr(), "atlas_set_margin", idx, margin);
	ur->add_do_method(tileset.ptr(), "atlas_set_separation", idx, sep);
	ur->commit_action();
}

void TileSetEditor::_add_atlas_pressed() {

	if (tileset.is_null())
		return;
	file_dialog->popup_centered_ratio();
}

void TileSetEditor::_atlas_file_selected(const String &p_path) {

	Ref<Texture> tex = ResourceLoader::load(p_path, "Texture");
	if (tex.is_null()) {
		err_dialog->set_text(TTR("Not a texture:") + " " + p_path);
		err_dialog->popup_centered_minsize();
		return;
	}
	if (tileset->atlas_find(tex) < 0) {
		bool used = false;
		for (int i = 0; i < sources.size(); i++)
			used = used || sources[i].texture == tex;
		if (!used) {
			UndoRedo *ur = editor->get_undo_redo();
			ur->create_action(TTR("Add Atlas"));
			ur->add_do_method(tileset.ptr(), "atlas_add", tex);
			ur->add_undo_method(tileset.ptr(), "atlas_remove", tileset->atlas_get_count());
			ur->commit_action();
		}
	}
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i].texture == tex)
			_source_selected(i);
	}
}

void TileSetEditor::_select_tile(int p_id) {

	selected_tile = p_id;
	Vector<int> sel;
	if (p_id >= 0)
		sel.push_back(p_id);
	atlas_view->set_selected(sel);
	remove_tile_button->set_disabled(p_id < 0);
	if (p_id < 0 || tileset.is_null() || !tileset->has_tile(p_id)) {
		tile_label->set_text(TTR("No tile selected."));
		tile_properties->edit(NULL);
		_update_preview();
		return;
	}
	tile_label->set_text(TTR("Tile") + " " + itos(p_id) + ": " + tileset->tile_get_name(p_id));
	tile_properties->edit(_get_proxy(p_id));
	_update_preview();
}

void TileSetEditor::_create_tile(const Rect2 &p_cells) {

	const TileAtlasView::Source &s = sources[current_source];
	int id = tileset->get_last_unused_tile_id();
	Rect2 region = atlas_view->cells_region(p_cells);
	String name = s.texture->get_path().get_file().basename() + "_" + itos(p_cells.pos.x) + "_" + itos(p_cells.pos.y);

	UndoRedo *ur = editor->get_undo_redo();
	ur->create_action(TTR("Create Tile"));
	_register_source_atlas(ur);
	ur->add_do_method(tileset.ptr(), "create_tile", id);
	ur->add_do_method(tileset.ptr(), "tile_set_texture", id, s.texture);
	ur->add_do_method(tileset.ptr(), "tile_set_region", id, region);
	ur->add_do_method(tileset.ptr(), "tile_set_name", id, name);
	ur->add_undo_method(tileset.ptr(), "remove_tile", id);
	ur->commit_action();
	_select_tile(id);
}

void TileSetEditor::_remove_tile(int p_id) {

	if (tileset.is_null() || !tileset->has_tile(p_id))
		return;
	UndoRedo *ur = editor->get_undo_redo();
	ur->create_action(TTR("Remove Tile"));
	ur->add_do_method(tileset.ptr(), "remove_tile", p_id);
	ur->add_undo_method(tileset.ptr(), "create_tile", p_id);
	// Everything the tile stores comes back on undo.
	String pre = itos(p_id) + "/";
	List<PropertyInfo> props;
	tileset->get_property_list(&props);
	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		if (E->get().name.begins_with(pre) && (E->get().usage & PROPERTY_USAGE_STORAGE))
			ur->add_undo_method(tileset.ptr(), "set", E->get().name, tileset->get(E->get().name));
	}
	ur->commit_action();
	if (selected_tile == p_id)
		_select_tile(-1);
}

void TileSetEditor::_mode_pressed(bool p_create) {

	create_mode = p_create;
	mode_select->set_pressed(!p_create);
	mode_create->set_pressed(p_create);
}

void TileSetEditor::_cells_selected(const Rect2 &p_cells, bool p_shift, bool p_ctrl) {

	if (tileset.is_null() || current_source < 0)
		return;
	if (p_cells.size == Vector2(1, 1)) {
		int id = atlas_view->tile_at_cell(p_cells.pos);
		if (id >= 0) {
			_select_tile(id);
			return;
		}
	}
	Vector<int> inside;
	atlas_view->get_tiles_in(p_cells, &inside);
	if (inside.size()) {
		_select_tile(inside[0]);
		return;
	}
	// Empty cells: one tile covering them all (a 2x2 tree, a 3x3 house...).
	if (create_mode)
		_create_tile(p_cells);
	else
		_select_tile(-1);
}

void TileSetEditor::_cells_rmb(const Rect2 &p_cells) {

	if (!create_mode)
		return;
	int id = atlas_view->tile_at_cell(p_cells.pos);
	if (id >= 0)
		_remove_tile(id);
}

void TileSetEditor::_remove_tile_pressed() {

	if (selected_tile >= 0)
		_remove_tile(selected_tile);
}

void TileSetEditor::_show_ids_toggled(bool p_on) {

	atlas_view->set_show_ids(p_on);
}

void TileSetEditor::_create_tiles_in_atlas() {

	if (tileset.is_null() || current_source < 0)
		return;
	const TileAtlasView::Source &s = sources[current_source];
	Image img;
	Ref<ImageTexture> itex = s.texture;
	Ref<PaletteTexture> ptex = s.texture;
	if (itex.is_valid())
		img = itex->get_data();
	else if (ptex.is_valid())
		img = ptex->get_data();
	if (img.empty())
		img.load(s.texture->get_path());
	if (img.empty()) {
		err_dialog->set_text(TTR("Couldn't read the atlas image to find its non-empty cells."));
		err_dialog->popup_centered_minsize();
		return;
	}

	Vector2 grid = atlas_view->get_grid_size();
	int id = tileset->get_last_unused_tile_id();
	int created = 0;
	UndoRedo *ur = editor->get_undo_redo();
	ur->create_action(TTR("Create Tiles in Atlas"));
	_register_source_atlas(ur);
	for (int y = 0; y < grid.y; y++) {
		for (int x = 0; x < grid.x; x++) {
			Vector2 cell(x, y);
			if (atlas_view->tile_at_cell(cell) >= 0 || atlas_view->cell_is_empty(img, cell))
				continue;
			String name = s.texture->get_path().get_file().basename() + "_" + itos(x) + "_" + itos(y);
			ur->add_do_method(tileset.ptr(), "create_tile", id);
			ur->add_do_method(tileset.ptr(), "tile_set_texture", id, s.texture);
			ur->add_do_method(tileset.ptr(), "tile_set_region", id, atlas_view->cells_region(Rect2(cell, Vector2(1, 1))));
			ur->add_do_method(tileset.ptr(), "tile_set_name", id, name);
			ur->add_undo_method(tileset.ptr(), "remove_tile", id);
			id++;
			created++;
		}
	}
	ur->commit_action();
	if (created == 0) {
		err_dialog->set_text(TTR("Every non-empty cell already has a tile."));
		err_dialog->popup_centered_minsize();
	}
}

void TileSetEditor::_menu_confirm() {

	switch (option) {

		case MENU_OPTION_MERGE_FROM_SCENE:
		case MENU_OPTION_CREATE_FROM_SCENE: {

			EditorNode *en = editor;
			Node *scene = en->get_edited_scene();
			if (!scene)
				break;

			_import_scene(scene, tileset, option == MENU_OPTION_MERGE_FROM_SCENE);

		} break;
	}
}

void TileSetEditor::_name_dialog_confirm(const String &name) {

	switch (option) {

		case MENU_OPTION_REMOVE_ITEM: {

			int id = tileset->find_tile_by_name(name);

			if (id < 0 && name.is_valid_integer())
				id = name.to_int();

			if (tileset->has_tile(id)) {
				_remove_tile(id);
			} else {
				err_dialog->set_text(TTR("Could not find tile:") + " " + name);
				err_dialog->popup_centered(Size2(300, 60));
			}
		} break;
	}
}

void TileSetEditor::_menu_cbk(int p_option) {

	if (tileset.is_null())
		return;
	option = p_option;
	switch (p_option) {

		case MENU_OPTION_ADD_ITEM: {

			int id = tileset->get_last_unused_tile_id();
			UndoRedo *ur = editor->get_undo_redo();
			ur->create_action(TTR("Add Item"));
			ur->add_do_method(tileset.ptr(), "create_tile", id);
			ur->add_undo_method(tileset.ptr(), "remove_tile", id);
			ur->commit_action();
		} break;
		case MENU_OPTION_REMOVE_ITEM: {

			nd->set_title(TTR("Remove Item"));
			nd->set_text(TTR("Item name or ID:"));
			nd->popup_centered(Size2(300, 95));
		} break;
		case MENU_OPTION_CREATE_FROM_SCENE: {

			cd->set_text(TTR("Create from scene?"));
			cd->popup_centered(Size2(300, 60));
		} break;
		case MENU_OPTION_MERGE_FROM_SCENE: {

			cd->set_text(TTR("Merge from scene?"));
			cd->popup_centered(Size2(300, 60));
		} break;
		case MENU_OPTION_CREATE_TILES_IN_ATLAS: {

			_create_tiles_in_atlas();
		} break;
		case MENU_OPTION_USE_PALETTE_TEXTURE: {

			if (tileset.is_valid())
				palette_dialog->popup_centered_ratio();
		} break;
		case MENU_OPTION_CUSTOM_DATA_LAYERS: {

			// The layers (name + type) are TileSet properties: edit them in the Inspector. Each
			// tile then shows one field per layer under "Custom Data" in its properties here.
			if (tileset.is_valid())
				editor->push_item(tileset.ptr());
		} break;
		case MENU_OPTION_REMOVE_ATLAS: {

			// Forgets the atlas grid; its tiles stay (the texture is still listed while used).
			if (current_source < 0 || sources[current_source].atlas < 0)
				break;
			const TileAtlasView::Source &s = sources[current_source];
			UndoRedo *ur = editor->get_undo_redo();
			ur->create_action(TTR("Remove Atlas"));
			ur->add_do_method(tileset.ptr(), "atlas_remove", s.atlas);
			int idx = tileset->atlas_get_count() - 1;
			ur->add_undo_method(tileset.ptr(), "atlas_add", s.texture);
			ur->add_undo_method(tileset.ptr(), "atlas_set_tile_size", idx, s.tile_size);
			ur->add_undo_method(tileset.ptr(), "atlas_set_margin", idx, s.margin);
			ur->add_undo_method(tileset.ptr(), "atlas_set_separation", idx, s.separation);
			ur->commit_action();
		} break;
	}
}

Error TileSetEditor::update_library_file(Node *p_base_scene, Ref<TileSet> ml, bool p_merge) {

	_import_scene(p_base_scene, ml, p_merge);
	return OK;
}

void TileSetEditor::_notification(int p_what) {

	if (p_what == NOTIFICATION_ENTER_TREE) {
		remove_tile_button->set_icon(get_icon("Remove", "EditorIcons"));
	}
}

void TileSetEditor::_bind_methods() {

	ObjectTypeDB::bind_method("_menu_cbk", &TileSetEditor::_menu_cbk);
	ObjectTypeDB::bind_method("_menu_confirm", &TileSetEditor::_menu_confirm);
	ObjectTypeDB::bind_method("_name_dialog_confirm", &TileSetEditor::_name_dialog_confirm);
	ObjectTypeDB::bind_method("_tileset_changed", &TileSetEditor::_tileset_changed);
	ObjectTypeDB::bind_method("_source_selected", &TileSetEditor::_source_selected);
	ObjectTypeDB::bind_method("_grid_changed", &TileSetEditor::_grid_changed);
	ObjectTypeDB::bind_method("_add_atlas_pressed", &TileSetEditor::_add_atlas_pressed);
	ObjectTypeDB::bind_method("_atlas_file_selected", &TileSetEditor::_atlas_file_selected);
	ObjectTypeDB::bind_method("_palette_texture_selected", &TileSetEditor::_palette_texture_selected);
	ObjectTypeDB::bind_method("_cells_selected", &TileSetEditor::_cells_selected);
	ObjectTypeDB::bind_method("_cells_rmb", &TileSetEditor::_cells_rmb);
	ObjectTypeDB::bind_method("_remove_tile_pressed", &TileSetEditor::_remove_tile_pressed);
	ObjectTypeDB::bind_method("_show_ids_toggled", &TileSetEditor::_show_ids_toggled);
	ObjectTypeDB::bind_method("_mode_pressed", &TileSetEditor::_mode_pressed);
	ObjectTypeDB::bind_method("_preview_origin_selected", &TileSetEditor::_preview_origin_selected);
	ObjectTypeDB::bind_method("_preview_shape_edited", &TileSetEditor::_preview_shape_edited);
	ObjectTypeDB::bind_method("_preview_add_shape_pressed", &TileSetEditor::_preview_add_shape_pressed);
	ObjectTypeDB::bind_method("_property_edited", &TileSetEditor::_property_edited);
	ObjectTypeDB::bind_method("_resource_selected", &TileSetEditor::_resource_selected);
}

void TileSetEditor::_update_preview() {

	bool has = tileset.is_valid() && selected_tile >= 0 && tileset->has_tile(selected_tile);
	tile_preview->set_tile(tileset, has ? selected_tile : -1);
	preview_add_shape->set_hidden(!(has && tileset->tile_get_shape(selected_tile).is_null()));
	if (!has) {
		preview_info->set_text("");
		return;
	}
	Vector2 o = tileset->tile_get_texture_offset(selected_tile);
	preview_info->set_text(TTR("Offset") + " " + String::num(o.x) + ", " + String::num(o.y));
}

void TileSetEditor::set_cell_context(const Size2 &p_cell_size, int p_origin) {

	tile_preview->set_cell(p_cell_size, p_origin);
	preview_origin->select(CLAMP(p_origin, 0, preview_origin->get_item_count() - 1)); // items in TILE_ORIGIN_* order
}

void TileSetEditor::_preview_origin_selected(int p_index) {

	tile_preview->set_cell(tile_preview->get_cell_size(), preview_origin->get_item_ID(p_index));
}

void TileSetEditor::_palette_texture_selected(const String &p_path) {

	// Tiles (and atlas grids) drawn from the PaletteTexture's source PNG now use the
	// PaletteTexture: same pixels and regions, colors from its palettes.
	Ref<PaletteTexture> pt = ResourceLoader::load(p_path);
	if (pt.is_null()) {
		err_dialog->set_text(p_path.get_file() + ": " + TTR("not a PaletteTexture (import one with Import > Palette Texture)."));
		err_dialog->popup_centered_minsize();
		return;
	}
	String src = Globals::get_singleton()->localize_path(pt->get_source_image());
	UndoRedo *ur = editor->get_undo_redo();
	ur->create_action(TTR("Use Palette Texture"));
	int count = 0;
	List<int> ids;
	tileset->get_tile_list(&ids);
	for (List<int>::Element *E = ids.front(); E; E = E->next()) {
		Ref<Texture> t = tileset->tile_get_texture(E->get());
		if (t.is_valid() && t != pt && t->get_path() == src) {
			ur->add_do_method(tileset.ptr(), "tile_set_texture", E->get(), pt);
			ur->add_undo_method(tileset.ptr(), "tile_set_texture", E->get(), t);
			count++;
		}
	}
	for (int i = 0; i < tileset->atlas_get_count(); i++) {
		Ref<Texture> t = tileset->atlas_get_texture(i);
		if (t.is_valid() && t != pt && t->get_path() == src) {
			String prop = "atlas/" + itos(i) + "/texture";
			ur->add_do_property(tileset.ptr(), prop, pt);
			ur->add_undo_property(tileset.ptr(), prop, t);
		}
	}
	ur->commit_action(); // (empty when nothing matched)
	if (!count) {
		err_dialog->set_text(TTR("No tile uses") + " " + src + " " + TTR("(the PNG this PaletteTexture was made from)."));
		err_dialog->popup_centered_minsize();
		return;
	}
	info_dialog->set_text(itos(count) + " " + TTR("tiles now use") + " " + p_path.get_file() + ".\n" + TTR("Save the TileSet to keep it."));
	info_dialog->popup_centered_minsize();
}

void TileSetEditor::_preview_shape_edited() {

	// Shape settings changed in the shape itself: refresh the fields.
	if (tileset.is_valid() && selected_tile >= 0 && tileset->has_tile(selected_tile))
		_get_proxy(selected_tile)->notify_changed();
	_update_preview();
}

void TileSetEditor::_preview_add_shape_pressed() {

	tile_preview->add_rect_shape();
}

void TileSetEditor::_property_edited(const Variant &p_prop) {

	_update_preview(); // shape settings change the shape, not the TileSet: no "changed" from it
}

void TileSetEditor::_resource_selected(Object *p_res, const String &p_prop) {

	// The shape's settings are already listed below it; other resources (material, navigation,
	// occluder) open in the Inspector.
	if (p_prop == "shape" || !p_res)
		return;
	editor->push_item(p_res);
}

static SpinBox *_grid_spin(Control *p_parent, float p_min, Object *p_target) {

	SpinBox *sb = memnew(SpinBox);
	sb->set_min(p_min);
	sb->set_max(4096);
	sb->set_step(1);
	sb->set_custom_minimum_size(Size2(44 * EDSCALE, 0));
	sb->connect("value_changed", p_target, "_grid_changed");
	p_parent->add_child(sb);
	return sb;
}

TileSetEditor::TileSetEditor(EditorNode *p_editor) {

	singleton = this;
	editor = p_editor;
	current_source = -1;
	selected_tile = -1;
	updating = false;
	create_mode = false;
	option = 0;

	set_custom_minimum_size(Size2(0, 300 * EDSCALE));

	// Left column: atlases and the selected atlas' grid. Middle: the atlas. Right: the tile.
	HSplitContainer *split = memnew(HSplitContainer);
	split->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(split);

	VBoxContainer *left = memnew(VBoxContainer);
	left->set_custom_minimum_size(Size2(150 * EDSCALE, 0));
	split->add_child(left);

	HBoxContainer *left_hb = memnew(HBoxContainer);
	left->add_child(left_hb);
	Button *add_atlas = memnew(Button);
	add_atlas->set_text(TTR("Add Atlas..."));
	add_atlas->set_tooltip(TTR("Add a texture to cut into tiles."));
	add_atlas->set_h_size_flags(SIZE_EXPAND_FILL);
	add_atlas->connect("pressed", this, "_add_atlas_pressed");
	left_hb->add_child(add_atlas);
	menu = memnew(MenuButton);
	menu->set_text(TTR("TileSet"));
	menu->set_flat(false);
	PopupMenu *p = menu->get_popup();
	p->add_item(TTR("Create Tiles in Non-Empty Cells"), MENU_OPTION_CREATE_TILES_IN_ATLAS);
	p->add_item(TTR("Forget Atlas Grid"), MENU_OPTION_REMOVE_ATLAS);
	p->add_separator();
	p->add_item(TTR("Custom Data Layers (Inspector)..."), MENU_OPTION_CUSTOM_DATA_LAYERS);
	p->add_item(TTR("Use Palette Texture..."), MENU_OPTION_USE_PALETTE_TEXTURE);
	p->add_separator();
	p->add_item(TTR("Add Empty Tile"), MENU_OPTION_ADD_ITEM);
	p->add_item(TTR("Remove Tile by Name or ID..."), MENU_OPTION_REMOVE_ITEM);
	p->add_separator();
	p->add_item(TTR("Create from Scene"), MENU_OPTION_CREATE_FROM_SCENE);
	p->add_item(TTR("Merge from Scene"), MENU_OPTION_MERGE_FROM_SCENE);
	p->connect("item_pressed", this, "_menu_cbk");
	left_hb->add_child(menu);

	source_list = memnew(ItemList);
	source_list->set_v_size_flags(SIZE_EXPAND_FILL);
	source_list->set_fixed_icon_size(Size2(32, 32) * EDSCALE);
	source_list->connect("item_selected", this, "_source_selected");
	left->add_child(source_list);

	GridContainer *grid_fields = memnew(GridContainer);
	grid_fields->set_columns(3);
	left->add_child(grid_fields);
	Label *l = memnew(Label);
	l->set_text(TTR("Tile"));
	grid_fields->add_child(l);
	size_x = _grid_spin(grid_fields, 1, this);
	size_y = _grid_spin(grid_fields, 1, this);
	l = memnew(Label);
	l->set_text(TTR("Margin"));
	grid_fields->add_child(l);
	margin_x = _grid_spin(grid_fields, 0, this);
	margin_y = _grid_spin(grid_fields, 0, this);
	l = memnew(Label);
	l->set_text(TTR("Separ."));
	l->set_tooltip(TTR("Separation between cells"));
	grid_fields->add_child(l);
	sep_x = _grid_spin(grid_fields, 0, this);
	sep_y = _grid_spin(grid_fields, 0, this);

	show_ids = memnew(CheckBox);
	show_ids->set_text(TTR("Show tile IDs"));
	show_ids->set_pressed(true);
	show_ids->connect("toggled", this, "_show_ids_toggled");
	left->add_child(show_ids);

	HSplitContainer *split2 = memnew(HSplitContainer);
	split2->set_h_size_flags(SIZE_EXPAND_FILL);
	split->add_child(split2);

	VBoxContainer *middle = memnew(VBoxContainer);
	middle->set_h_size_flags(SIZE_EXPAND_FILL);
	split2->add_child(middle);

	// Modes, as Godot 4's Select / Setup: selecting never creates tiles by accident.
	HBoxContainer *modes = memnew(HBoxContainer);
	middle->add_child(modes);
	mode_select = memnew(Button);
	mode_select->set_text(TTR("Select"));
	mode_select->set_toggle_mode(true);
	mode_select->set_pressed(true);
	mode_select->set_tooltip(TTR("Click a tile to edit its properties."));
	mode_select->connect("pressed", this, "_mode_pressed", varray(false));
	modes->add_child(mode_select);
	mode_create = memnew(Button);
	mode_create->set_text(TTR("Create / Erase"));
	mode_create->set_toggle_mode(true);
	mode_create->set_tooltip(TTR("Click an empty cell to create a tile; drag over empty cells for a tile of several cells (a 2x2 tree); right click a tile to remove it."));
	mode_create->connect("pressed", this, "_mode_pressed", varray(true));
	modes->add_child(mode_create);

	atlas_view = memnew(TileAtlasView);
	atlas_view->set_v_size_flags(SIZE_EXPAND_FILL);
	atlas_view->connect("cells_selected", this, "_cells_selected");
	atlas_view->connect("cells_rmb", this, "_cells_rmb");
	middle->add_child(atlas_view);

	VBoxContainer *props = memnew(VBoxContainer);
	props->set_custom_minimum_size(Size2(330 * EDSCALE, 0));
	split2->add_child(props);

	HBoxContainer *props_hb = memnew(HBoxContainer);
	props->add_child(props_hb);
	tile_label = memnew(Label);
	tile_label->set_h_size_flags(SIZE_EXPAND_FILL);
	tile_label->set_clip_text(true);
	props_hb->add_child(tile_label);
	remove_tile_button = memnew(Button);
	remove_tile_button->set_tooltip(TTR("Remove Tile") + " " + TTR("(or right click it in the atlas)"));
	remove_tile_button->connect("pressed", this, "_remove_tile_pressed");
	props_hb->add_child(remove_tile_button);

	// Preview | properties. The preview takes the width the panel gains when it is widened (more
	// room for the zoom); the divider between the two can be dragged too.
	HSplitContainer *props_body = memnew(HSplitContainer);
	props_body->set_v_size_flags(SIZE_EXPAND_FILL);
	props->add_child(props_body);

	// The tile as the TileMap draws it: Tex Offset and Shape Offset at a glance.
	VBoxContainer *preview_vb = memnew(VBoxContainer);
	preview_vb->set_custom_minimum_size(Size2(120 * EDSCALE, 0));
	preview_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	props_body->add_child(preview_vb);
	tile_preview = memnew(TileSetTilePreview);
	tile_preview->set_v_size_flags(SIZE_EXPAND_FILL);
	tile_preview->set_tooltip(TTR("The tile as the TileMap draws it.\nWhite: its cell. Red cross: the cell's origin (Tile Origin).\nYellow: the texture, moved by Tex Offset. Blue: the collision shape, moved by Shape Offset."));
	preview_vb->add_child(tile_preview);
	preview_origin = memnew(OptionButton);
	preview_origin->add_item(TTR("Top Left"), TileMap::TILE_ORIGIN_TOP_LEFT);
	preview_origin->add_item(TTR("Center"), TileMap::TILE_ORIGIN_CENTER);
	preview_origin->add_item(TTR("Bottom Left"), TileMap::TILE_ORIGIN_BOTTOM_LEFT);
	preview_origin->set_tooltip(TTR("Tile Origin used by the preview (the edited TileMap's, when there is one)."));
	preview_origin->connect("item_selected", this, "_preview_origin_selected");
	preview_vb->add_child(preview_origin);
	preview_info = memnew(Label);
	preview_info->set_clip_text(true);
	preview_vb->add_child(preview_info);
	preview_add_shape = memnew(Button);
	preview_add_shape->set_text(TTR("+ Rect Shape"));
	preview_add_shape->set_tooltip(TTR("Give the tile a rectangle collision shape covering its cell (then drag it, or its corner, in the preview)."));
	preview_add_shape->connect("pressed", this, "_preview_add_shape_pressed");
	preview_vb->add_child(preview_add_shape);
	tile_preview->set_undo_redo(editor->get_undo_redo());
	tile_preview->connect("shape_edited", this, "_preview_shape_edited");

	tile_properties = memnew(PropertyEditor);
	tile_properties->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
	tile_properties->set_v_size_flags(SIZE_EXPAND_FILL);
	tile_properties->hide_top_label();
	tile_properties->set_show_categories(false);
	tile_properties->set_undo_redo(editor->get_undo_redo());
	tile_properties->connect("property_edited", this, "_property_edited");
	tile_properties->connect("resource_selected", this, "_resource_selected");
	props_body->add_child(tile_properties);

	info_dialog = memnew(AcceptDialog);
	info_dialog->set_title(TTR("Use Palette Texture"));
	add_child(info_dialog);

	palette_dialog = memnew(EditorFileDialog);
	palette_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	palette_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	palette_dialog->set_title(TTR("Use Palette Texture (replaces its PNG in the tiles)"));
	palette_dialog->add_filter("*.res ; Binary Resource");
	palette_dialog->add_filter("*.tres ; Text Resource");
	palette_dialog->connect("file_selected", this, "_palette_texture_selected");
	add_child(palette_dialog);

	file_dialog = memnew(EditorFileDialog);
	file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("Texture", &extensions);
	for (List<String>::Element *E = extensions.front(); E; E = E->next())
		file_dialog->add_filter("*." + E->get() + " ; " + E->get().to_upper());
	file_dialog->connect("file_selected", this, "_atlas_file_selected");
	add_child(file_dialog);

	cd = memnew(ConfirmationDialog);
	add_child(cd);
	cd->get_ok()->connect("pressed", this, "_menu_confirm");

	nd = memnew(EditorNameDialog);
	add_child(nd);
	nd->set_hide_on_ok(true);
	nd->get_line_edit()->set_margin(MARGIN_TOP, 28);
	nd->connect("name_confirmed", this, "_name_dialog_confirm");

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);
	err_dialog->set_title(TTR("Error"));

	_select_tile(-1);
}

TileSetEditor::~TileSetEditor() {

	for (Map<int, TileSetEditorTileProxy *>::Element *E = proxies.front(); E; E = E->next())
		memdelete(E->get());
	if (singleton == this)
		singleton = NULL;
}

//////////////////////////////////////////////////////////////////////////////
// Plugin

TileSetEditorPlugin *TileSetEditorPlugin::singleton = NULL;

void TileSetEditorPlugin::_update_button() {

	button->set_hidden(!(editing_resource || shown_for_tilemap));
}

void TileSetEditorPlugin::show_for_tilemap(const Ref<TileSet> &p_tileset) {

	shown_for_tilemap = p_tileset.is_valid();
	if (!editing_resource)
		tileset_editor->edit(p_tileset);
	_update_button();
}

void TileSetEditorPlugin::hide_for_tilemap() {

	shown_for_tilemap = false;
	if (!editing_resource && tileset_editor->is_visible())
		editor->hide_bottom_panel();
	_update_button();
}

void TileSetEditorPlugin::edit(Object *p_node) {

	if (p_node && p_node->cast_to<TileSet>())
		tileset_editor->edit(p_node->cast_to<TileSet>());
}

bool TileSetEditorPlugin::handles(Object *p_node) const {

	return p_node->is_type("TileSet");
}

void TileSetEditorPlugin::make_visible(bool p_visible) {

	editing_resource = p_visible;
	_update_button();
	if (p_visible)
		editor->make_bottom_panel_item_visible(tileset_editor);
	else if (!shown_for_tilemap && tileset_editor->is_visible())
		editor->hide_bottom_panel();
}

TileSetEditorPlugin::TileSetEditorPlugin(EditorNode *p_node) {

	singleton = this;
	editor = p_node;
	editing_resource = false;
	shown_for_tilemap = false;
	tileset_editor = memnew(TileSetEditor(p_node));
	button = p_node->add_bottom_panel_item(TTR("TileSet"), tileset_editor);
	button->hide();
}
