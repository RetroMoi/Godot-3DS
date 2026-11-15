/*************************************************************************/
/*  tile_set.cpp                                                         */
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

#include "tile_set.h"

bool TileSet::_set_atlas(const String &p_name, const Variant &p_value) {

	// atlas/<index>/<what>
	int idx = p_name.get_slice("/", 1).to_int();
	String what = p_name.get_slice("/", 2);
	ERR_FAIL_COND_V(idx < 0 || idx > 1024, false);
	if (idx >= atlases.size())
		atlases.resize(idx + 1);
	if (what == "texture")
		atlases[idx].texture = p_value;
	else if (what == "tile_size")
		atlases[idx].tile_size = p_value;
	else if (what == "margin")
		atlases[idx].margin = p_value;
	else if (what == "separation")
		atlases[idx].separation = p_value;
	else
		return false;
	return true;
}

bool TileSet::_get_atlas(const String &p_name, Variant &r_ret) const {

	int idx = p_name.get_slice("/", 1).to_int();
	String what = p_name.get_slice("/", 2);
	if (idx < 0 || idx >= atlases.size())
		return false;
	if (what == "texture")
		r_ret = atlases[idx].texture;
	else if (what == "tile_size")
		r_ret = atlases[idx].tile_size;
	else if (what == "margin")
		r_ret = atlases[idx].margin;
	else if (what == "separation")
		r_ret = atlases[idx].separation;
	else
		return false;
	return true;
}

bool TileSet::_set(const StringName &p_name, const Variant &p_value) {

	String n = p_name;
	if (n.begins_with("atlas/"))
		return _set_atlas(n, p_value);
	if (n.begins_with("random_group/")) {
		int idx = n.get_slice("/", 1).to_int();
		String what = n.get_slice("/", 2);
		ERR_FAIL_COND_V(idx < 0 || idx > 1024, false);
		if (idx >= random_groups.size())
			random_groups.resize(idx + 1);
		if (what == "name")
			random_groups[idx].name = p_value;
		else if (what == "tiles")
			random_groups[idx].tiles = p_value;
		else
			return false;
		return true;
	}
	int slash = n.find("/");
	if (slash == -1)
		return false;
	int id = String::to_int(n.c_str(), slash);

	if (!tile_map.has(id))
		create_tile(id);
	String what = n.substr(slash + 1, n.length());

	if (what == "name")
		tile_set_name(id, p_value);
	else if (what == "texture")
		tile_set_texture(id, p_value);
	else if (what == "tex_offset")
		tile_set_texture_offset(id, p_value);
	else if (what == "material")
		tile_set_material(id, p_value);
	else if (what == "modulate")
		tile_set_modulate(id, p_value);
	else if (what == "shape_offset")
		tile_set_shape_offset(id, p_value);
	else if (what == "region")
		tile_set_region(id, p_value);
	else if (what == "shape")
		tile_set_shape(id, p_value);
	else if (what == "shapes")
		_tile_set_shapes(id, p_value);
	else if (what == "one_way_collision_direction")
		tile_set_one_way_collision_direction(id, p_value);
	else if (what == "one_way_collision_max_depth")
		tile_set_one_way_collision_max_depth(id, p_value);
	else if (what == "occluder")
		tile_set_light_occluder(id, p_value);
	else if (what == "occluder_offset")
		tile_set_occluder_offset(id, p_value);
	else if (what == "navigation")
		tile_set_navigation_polygon(id, p_value);
	else if (what == "navigation_offset")
		tile_set_navigation_polygon_offset(id, p_value);
	else if (what == "probability")
		tile_set_probability(id, p_value);
	else
		return false;

	return true;
}

bool TileSet::_get(const StringName &p_name, Variant &r_ret) const {

	String n = p_name;
	if (n.begins_with("atlas/"))
		return _get_atlas(n, r_ret);
	if (n.begins_with("random_group/")) {
		int idx = n.get_slice("/", 1).to_int();
		String what = n.get_slice("/", 2);
		if (idx < 0 || idx >= random_groups.size())
			return false;
		if (what == "name")
			r_ret = random_groups[idx].name;
		else if (what == "tiles")
			r_ret = random_groups[idx].tiles;
		else
			return false;
		return true;
	}
	int slash = n.find("/");
	if (slash == -1)
		return false;
	int id = String::to_int(n.c_str(), slash);

	ERR_FAIL_COND_V(!tile_map.has(id), false);

	String what = n.substr(slash + 1, n.length());

	if (what == "name")
		r_ret = tile_get_name(id);
	else if (what == "texture")
		r_ret = tile_get_texture(id);
	else if (what == "tex_offset")
		r_ret = tile_get_texture_offset(id);
	else if (what == "material")
		r_ret = tile_get_material(id);
	else if (what == "modulate")
		r_ret = tile_get_modulate(id);
	else if (what == "shape_offset")
		r_ret = tile_get_shape_offset(id);
	else if (what == "region")
		r_ret = tile_get_region(id);
	else if (what == "shape")
		r_ret = tile_get_shape(id);
	else if (what == "shapes")
		r_ret = _tile_get_shapes(id);
	else if (what == "one_way_collision_direction")
		r_ret = tile_get_one_way_collision_direction(id);
	else if (what == "one_way_collision_max_depth")
		r_ret = tile_get_one_way_collision_max_depth(id);
	else if (what == "occluder")
		r_ret = tile_get_light_occluder(id);
	else if (what == "occluder_offset")
		r_ret = tile_get_occluder_offset(id);
	else if (what == "navigation")
		r_ret = tile_get_navigation_polygon(id);
	else if (what == "navigation_offset")
		r_ret = tile_get_navigation_polygon_offset(id);
	else if (what == "probability")
		r_ret = tile_get_probability(id);
	else
		return false;

	return true;
}

void TileSet::_get_property_list(List<PropertyInfo> *p_list) const {

	for (int i = 0; i < random_groups.size(); i++) {
		String pre = "random_group/" + itos(i) + "/";
		p_list->push_back(PropertyInfo(Variant::STRING, pre + "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::INT_ARRAY, pre + "tiles", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	}

	for (int i = 0; i < atlases.size(); i++) {
		String pre = "atlas/" + itos(i) + "/";
		p_list->push_back(PropertyInfo(Variant::OBJECT, pre + "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "tile_size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "margin", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "separation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
	}

	for (Map<int, Data>::Element *E = tile_map.front(); E; E = E->next()) {

		int id = E->key();
		String pre = itos(id) + "/";
		p_list->push_back(PropertyInfo(Variant::STRING, pre + "name"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, pre + "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "tex_offset"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, pre + "material", PROPERTY_HINT_RESOURCE_TYPE, "CanvasItemMaterial"));
		p_list->push_back(PropertyInfo(Variant::COLOR, pre + "modulate"));
		p_list->push_back(PropertyInfo(Variant::RECT2, pre + "region"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "occluder_offset"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, pre + "occluder", PROPERTY_HINT_RESOURCE_TYPE, "OccluderPolygon2D"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "navigation_offset"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, pre + "navigation", PROPERTY_HINT_RESOURCE_TYPE, "NavigationPolygon"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "shape_offset"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, pre + "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape2D", PROPERTY_USAGE_EDITOR));
		p_list->push_back(PropertyInfo(Variant::ARRAY, pre + "shapes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, pre + "one_way_collision_direction"));
		p_list->push_back(PropertyInfo(Variant::REAL, pre + "one_way_collision_max_depth"));
		// Saved only when not the default, so existing TileSets stay as they are.
		p_list->push_back(PropertyInfo(Variant::REAL, pre + "probability", PROPERTY_HINT_RANGE, "0,100,0.01", PROPERTY_USAGE_EDITOR | (E->get().probability != 1.0f ? PROPERTY_USAGE_STORAGE : 0)));
	}
}

void TileSet::create_tile(int p_id) {

	ERR_FAIL_COND(tile_map.has(p_id));
	tile_map[p_id] = Data();
	_change_notify("");
	emit_changed();
}

void TileSet::tile_set_texture(int p_id, const Ref<Texture> &p_texture) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].texture = p_texture;
	emit_changed();
}

Ref<Texture> TileSet::tile_get_texture(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Ref<Texture>());
	return tile_map[p_id].texture;
}

void TileSet::tile_set_material(int p_id, const Ref<CanvasItemMaterial> &p_material) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].material = p_material;
	emit_changed();
}

Ref<CanvasItemMaterial> TileSet::tile_get_material(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Ref<CanvasItemMaterial>());
	return tile_map[p_id].material;
}

void TileSet::tile_set_modulate(int p_id, const Color &p_modulate) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].modulate = p_modulate;
	emit_changed();
}

Color TileSet::tile_get_modulate(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Color(1, 1, 1));
	return tile_map[p_id].modulate;
}

void TileSet::tile_set_texture_offset(int p_id, const Vector2 &p_offset) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].offset = p_offset;
	emit_changed();
}

Vector2 TileSet::tile_get_texture_offset(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Vector2());
	return tile_map[p_id].offset;
}

void TileSet::tile_set_shape_offset(int p_id, const Vector2 &p_offset) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].shape_offset = p_offset;
	emit_changed();
}

Vector2 TileSet::tile_get_shape_offset(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Vector2());
	return tile_map[p_id].shape_offset;
}

void TileSet::tile_set_region(int p_id, const Rect2 &p_region) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].region = p_region;
	emit_changed();
}

Rect2 TileSet::tile_get_region(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Rect2());
	return tile_map[p_id].region;
}

void TileSet::tile_set_name(int p_id, const String &p_name) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].name = p_name;
	emit_changed();
}

String TileSet::tile_get_name(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), String());
	return tile_map[p_id].name;
}

void TileSet::tile_set_shape(int p_id, const Ref<Shape2D> &p_shape) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].shapes.resize(1);
	tile_map[p_id].shapes[0] = p_shape;
	emit_changed();
}

Ref<Shape2D> TileSet::tile_get_shape(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Ref<Shape2D>());
	if (tile_map[p_id].shapes.size() > 0)
		return tile_map[p_id].shapes[0];

	return Ref<Shape2D>();
}

void TileSet::tile_set_light_occluder(int p_id, const Ref<OccluderPolygon2D> &p_light_occluder) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].occluder = p_light_occluder;
}

Ref<OccluderPolygon2D> TileSet::tile_get_light_occluder(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Ref<OccluderPolygon2D>());
	return tile_map[p_id].occluder;
}

void TileSet::tile_set_navigation_polygon_offset(int p_id, const Vector2 &p_offset) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].navigation_polygon_offset = p_offset;
}

Vector2 TileSet::tile_get_navigation_polygon_offset(int p_id) const {
	ERR_FAIL_COND_V(!tile_map.has(p_id), Vector2());
	return tile_map[p_id].navigation_polygon_offset;
}

void TileSet::tile_set_navigation_polygon(int p_id, const Ref<NavigationPolygon> &p_navigation_polygon) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].navigation_polygon = p_navigation_polygon;
}

Ref<NavigationPolygon> TileSet::tile_get_navigation_polygon(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Ref<NavigationPolygon>());
	return tile_map[p_id].navigation_polygon;
}

void TileSet::tile_set_occluder_offset(int p_id, const Vector2 &p_offset) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].occluder_offset = p_offset;
}

Vector2 TileSet::tile_get_occluder_offset(int p_id) const {
	ERR_FAIL_COND_V(!tile_map.has(p_id), Vector2());
	return tile_map[p_id].occluder_offset;
}

void TileSet::tile_set_shapes(int p_id, const Vector<Ref<Shape2D> > &p_shapes) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].shapes = p_shapes;
	emit_changed();
}

Vector<Ref<Shape2D> > TileSet::tile_get_shapes(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Vector<Ref<Shape2D> >());
	return tile_map[p_id].shapes;
}

void TileSet::_tile_set_shapes(int p_id, const Array &p_shapes) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	Vector<Ref<Shape2D> > shapes;
	for (int i = 0; i < p_shapes.size(); i++) {

		Ref<Shape2D> s = p_shapes[i];
		if (s.is_valid())
			shapes.push_back(s);
	}

	tile_set_shapes(p_id, shapes);
}

Array TileSet::_tile_get_shapes(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Array());
	Array arr;

	Vector<Ref<Shape2D> > shp = tile_map[p_id].shapes;
	for (int i = 0; i < shp.size(); i++)
		arr.push_back(shp[i]);

	return arr;
}

void TileSet::tile_set_one_way_collision_direction(int p_id, Vector2 p_direction) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].one_way_collision_direction = p_direction;
}

Vector2 TileSet::tile_get_one_way_collision_direction(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), Vector2());
	return tile_map[p_id].one_way_collision_direction;
}

void TileSet::tile_set_one_way_collision_max_depth(int p_id, float p_max_depth) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].one_way_collision_max_depth = p_max_depth;
}

float TileSet::tile_get_one_way_collision_max_depth(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), 0.0f);
	return tile_map[p_id].one_way_collision_max_depth;
}

Array TileSet::_get_tiles_ids() const {

	Array arr;

	for (Map<int, Data>::Element *E = tile_map.front(); E; E = E->next()) {
		arr.push_back(E->key());
	}

	return arr;
}

void TileSet::get_tile_list(List<int> *p_tiles) const {

	for (Map<int, Data>::Element *E = tile_map.front(); E; E = E->next()) {

		p_tiles->push_back(E->key());
	}
}

bool TileSet::has_tile(int p_id) const {

	return tile_map.has(p_id);
}

void TileSet::tile_set_probability(int p_id, float p_probability) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map[p_id].probability = MAX(0.0f, p_probability);
	emit_changed();
}

float TileSet::tile_get_probability(int p_id) const {

	ERR_FAIL_COND_V(!tile_map.has(p_id), 1.0f);
	return tile_map[p_id].probability;
}

void TileSet::remove_tile(int p_id) {

	ERR_FAIL_COND(!tile_map.has(p_id));
	tile_map.erase(p_id);
	_change_notify("");
	emit_changed();
}

int TileSet::get_last_unused_tile_id() const {

	if (tile_map.size())
		return tile_map.back()->key() + 1;
	else
		return 0;
}

int TileSet::find_tile_by_name(const String &p_name) const {

	for (Map<int, Data>::Element *E = tile_map.front(); E; E = E->next()) {

		if (p_name == E->get().name)
			return E->key();
	}
	return -1;
}

int TileSet::random_group_add(const String &p_name, const DVector<int> &p_tiles) {

	RandomGroup g;
	g.name = p_name;
	g.tiles = p_tiles;
	random_groups.push_back(g);
	emit_changed();
	return random_groups.size() - 1;
}

void TileSet::random_group_remove(int p_group) {

	ERR_FAIL_INDEX(p_group, random_groups.size());
	random_groups.remove(p_group);
	emit_changed();
}

int TileSet::random_group_get_count() const {

	return random_groups.size();
}

String TileSet::random_group_get_name(int p_group) const {

	ERR_FAIL_INDEX_V(p_group, random_groups.size(), String());
	return random_groups[p_group].name;
}

DVector<int> TileSet::random_group_get_tiles(int p_group) const {

	ERR_FAIL_INDEX_V(p_group, random_groups.size(), DVector<int>());
	return random_groups[p_group].tiles;
}

int TileSet::atlas_add(const Ref<Texture> &p_texture) {

	ERR_FAIL_COND_V(p_texture.is_null(), -1);
	int idx = atlas_find(p_texture);
	if (idx >= 0)
		return idx;
	Atlas a;
	a.texture = p_texture;
	atlases.push_back(a);
	_change_notify("");
	emit_changed();
	return atlases.size() - 1;
}

void TileSet::atlas_remove(int p_atlas) {

	ERR_FAIL_INDEX(p_atlas, atlases.size());
	atlases.remove(p_atlas);
	_change_notify("");
	emit_changed();
}

int TileSet::atlas_get_count() const {

	return atlases.size();
}

int TileSet::atlas_find(const Ref<Texture> &p_texture) const {

	for (int i = 0; i < atlases.size(); i++) {
		if (atlases[i].texture == p_texture)
			return i;
	}
	return -1;
}

Ref<Texture> TileSet::atlas_get_texture(int p_atlas) const {

	ERR_FAIL_INDEX_V(p_atlas, atlases.size(), Ref<Texture>());
	return atlases[p_atlas].texture;
}

void TileSet::atlas_set_tile_size(int p_atlas, const Vector2 &p_size) {

	ERR_FAIL_INDEX(p_atlas, atlases.size());
	atlases[p_atlas].tile_size = Vector2(MAX(1, p_size.x), MAX(1, p_size.y));
	emit_changed();
}

Vector2 TileSet::atlas_get_tile_size(int p_atlas) const {

	ERR_FAIL_INDEX_V(p_atlas, atlases.size(), Vector2());
	return atlases[p_atlas].tile_size;
}

void TileSet::atlas_set_margin(int p_atlas, const Vector2 &p_margin) {

	ERR_FAIL_INDEX(p_atlas, atlases.size());
	atlases[p_atlas].margin = p_margin;
	emit_changed();
}

Vector2 TileSet::atlas_get_margin(int p_atlas) const {

	ERR_FAIL_INDEX_V(p_atlas, atlases.size(), Vector2());
	return atlases[p_atlas].margin;
}

void TileSet::atlas_set_separation(int p_atlas, const Vector2 &p_separation) {

	ERR_FAIL_INDEX(p_atlas, atlases.size());
	atlases[p_atlas].separation = p_separation;
	emit_changed();
}

Vector2 TileSet::atlas_get_separation(int p_atlas) const {

	ERR_FAIL_INDEX_V(p_atlas, atlases.size(), Vector2());
	return atlases[p_atlas].separation;
}

void TileSet::clear() {

	tile_map.clear();
	_change_notify("");
	emit_changed();
}

void TileSet::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("create_tile", "id"), &TileSet::create_tile);
	ObjectTypeDB::bind_method(_MD("tile_set_name", "id", "name"), &TileSet::tile_set_name);
	ObjectTypeDB::bind_method(_MD("tile_get_name", "id"), &TileSet::tile_get_name);
	ObjectTypeDB::bind_method(_MD("tile_set_texture", "id", "texture:Texture"), &TileSet::tile_set_texture);
	ObjectTypeDB::bind_method(_MD("tile_get_texture:Texture", "id"), &TileSet::tile_get_texture);
	ObjectTypeDB::bind_method(_MD("tile_set_material", "id", "material:CanvasItemMaterial"), &TileSet::tile_set_material);
	ObjectTypeDB::bind_method(_MD("tile_get_material:CanvasItemMaterial", "id"), &TileSet::tile_get_material);
	ObjectTypeDB::bind_method(_MD("tile_set_texture_offset", "id", "texture_offset"), &TileSet::tile_set_texture_offset);
	ObjectTypeDB::bind_method(_MD("tile_get_texture_offset", "id"), &TileSet::tile_get_texture_offset);
	ObjectTypeDB::bind_method(_MD("tile_set_shape_offset", "id", "shape_offset"), &TileSet::tile_set_shape_offset);
	ObjectTypeDB::bind_method(_MD("tile_get_shape_offset", "id"), &TileSet::tile_get_shape_offset);
	ObjectTypeDB::bind_method(_MD("tile_set_region", "id", "region"), &TileSet::tile_set_region);
	ObjectTypeDB::bind_method(_MD("tile_get_region", "id"), &TileSet::tile_get_region);
	ObjectTypeDB::bind_method(_MD("tile_set_shape", "id", "shape:Shape2D"), &TileSet::tile_set_shape);
	ObjectTypeDB::bind_method(_MD("tile_get_shape:Shape2D", "id"), &TileSet::tile_get_shape);
	ObjectTypeDB::bind_method(_MD("tile_set_shapes", "id", "shapes"), &TileSet::_tile_set_shapes);
	ObjectTypeDB::bind_method(_MD("tile_get_shapes", "id"), &TileSet::_tile_get_shapes);
	ObjectTypeDB::bind_method(_MD("tile_set_navigation_polygon", "id", "navigation_polygon:NavigationPolygon"), &TileSet::tile_set_navigation_polygon);
	ObjectTypeDB::bind_method(_MD("tile_get_navigation_polygon:NavigationPolygon", "id"), &TileSet::tile_get_navigation_polygon);
	ObjectTypeDB::bind_method(_MD("tile_set_navigation_polygon_offset", "id", "navigation_polygon_offset"), &TileSet::tile_set_navigation_polygon_offset);
	ObjectTypeDB::bind_method(_MD("tile_get_navigation_polygon_offset", "id"), &TileSet::tile_get_navigation_polygon_offset);
	ObjectTypeDB::bind_method(_MD("tile_set_light_occluder", "id", "light_occluder:OccluderPolygon2D"), &TileSet::tile_set_light_occluder);
	ObjectTypeDB::bind_method(_MD("tile_get_light_occluder:OccluderPolygon2D", "id"), &TileSet::tile_get_light_occluder);
	ObjectTypeDB::bind_method(_MD("tile_set_occluder_offset", "id", "occluder_offset"), &TileSet::tile_set_occluder_offset);
	ObjectTypeDB::bind_method(_MD("tile_get_occluder_offset", "id"), &TileSet::tile_get_occluder_offset);

	ObjectTypeDB::bind_method(_MD("tile_set_probability", "id", "probability"), &TileSet::tile_set_probability);
	ObjectTypeDB::bind_method(_MD("tile_get_probability", "id"), &TileSet::tile_get_probability);
	ObjectTypeDB::bind_method(_MD("remove_tile", "id"), &TileSet::remove_tile);
	ObjectTypeDB::bind_method(_MD("clear"), &TileSet::clear);
	ObjectTypeDB::bind_method(_MD("get_last_unused_tile_id"), &TileSet::get_last_unused_tile_id);
	ObjectTypeDB::bind_method(_MD("find_tile_by_name", "name"), &TileSet::find_tile_by_name);
	ObjectTypeDB::bind_method(_MD("get_tiles_ids", "name"), &TileSet::_get_tiles_ids);

	ObjectTypeDB::bind_method(_MD("random_group_add", "name", "tiles"), &TileSet::random_group_add);
	ObjectTypeDB::bind_method(_MD("random_group_remove", "group"), &TileSet::random_group_remove);
	ObjectTypeDB::bind_method(_MD("random_group_get_count"), &TileSet::random_group_get_count);
	ObjectTypeDB::bind_method(_MD("random_group_get_name", "group"), &TileSet::random_group_get_name);
	ObjectTypeDB::bind_method(_MD("random_group_get_tiles", "group"), &TileSet::random_group_get_tiles);
	ObjectTypeDB::bind_method(_MD("atlas_add", "texture:Texture"), &TileSet::atlas_add);
	ObjectTypeDB::bind_method(_MD("atlas_remove", "atlas"), &TileSet::atlas_remove);
	ObjectTypeDB::bind_method(_MD("atlas_get_count"), &TileSet::atlas_get_count);
	ObjectTypeDB::bind_method(_MD("atlas_find", "texture:Texture"), &TileSet::atlas_find);
	ObjectTypeDB::bind_method(_MD("atlas_get_texture:Texture", "atlas"), &TileSet::atlas_get_texture);
	ObjectTypeDB::bind_method(_MD("atlas_set_tile_size", "atlas", "size"), &TileSet::atlas_set_tile_size);
	ObjectTypeDB::bind_method(_MD("atlas_get_tile_size", "atlas"), &TileSet::atlas_get_tile_size);
	ObjectTypeDB::bind_method(_MD("atlas_set_margin", "atlas", "margin"), &TileSet::atlas_set_margin);
	ObjectTypeDB::bind_method(_MD("atlas_get_margin", "atlas"), &TileSet::atlas_get_margin);
	ObjectTypeDB::bind_method(_MD("atlas_set_separation", "atlas", "separation"), &TileSet::atlas_set_separation);
	ObjectTypeDB::bind_method(_MD("atlas_get_separation", "atlas"), &TileSet::atlas_get_separation);
}

TileSet::TileSet() {
}
