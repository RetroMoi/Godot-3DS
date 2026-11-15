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

bool TileSetEditorTileProxy::_set(const StringName &p_name, const Variant &p_value) {

	if (tileset.is_null() || !tileset->has_tile(id))
		return false;
	bool valid = false;
	tileset->set(itos(id) + "/" + String(p_name), p_value, &valid);
	return valid;
}

bool TileSetEditorTileProxy::_get(const StringName &p_name, Variant &r_ret) const {

	if (tileset.is_null() || !tileset->has_tile(id))
		return false;
	bool valid = false;
	r_ret = tileset->get(itos(id) + "/" + String(p_name), &valid);
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
	}
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
		_get_proxy(selected_tile)->notify_changed();
		tile_label->set_text(TTR("Tile") + " " + itos(selected_tile) + ": " + tileset->tile_get_name(selected_tile));
	}
	atlas_view->update();
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
		return;
	}
	tile_label->set_text(TTR("Tile") + " " + itos(p_id) + ": " + tileset->tile_get_name(p_id));
	tile_properties->edit(_get_proxy(p_id));
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
	if (itex.is_valid())
		img = itex->get_data();
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
	ObjectTypeDB::bind_method("_cells_selected", &TileSetEditor::_cells_selected);
	ObjectTypeDB::bind_method("_cells_rmb", &TileSetEditor::_cells_rmb);
	ObjectTypeDB::bind_method("_remove_tile_pressed", &TileSetEditor::_remove_tile_pressed);
	ObjectTypeDB::bind_method("_show_ids_toggled", &TileSetEditor::_show_ids_toggled);
	ObjectTypeDB::bind_method("_mode_pressed", &TileSetEditor::_mode_pressed);
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
	props->set_custom_minimum_size(Size2(190 * EDSCALE, 0));
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

	tile_properties = memnew(PropertyEditor);
	tile_properties->set_v_size_flags(SIZE_EXPAND_FILL);
	tile_properties->hide_top_label();
	tile_properties->set_show_categories(false);
	tile_properties->set_undo_redo(editor->get_undo_redo());
	props->add_child(tile_properties);

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
