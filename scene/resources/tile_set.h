/*************************************************************************/
/*  tile_set.h                                                           */
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

#ifndef TILE_SET_H
#define TILE_SET_H

#include "resource.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/navigation_polygon.h"
#include "scene/resources/shape_2d.h"
#include "scene/resources/texture.h"

class TileSet : public Resource {

	OBJ_TYPE(TileSet, Resource);

	struct Data {

		String name;
		Ref<Texture> texture;
		Vector2 offset;
		Vector2 shape_offset;
		Rect2i region;
		Vector<Ref<Shape2D> > shapes;
		Vector2 one_way_collision_direction;
		float one_way_collision_max_depth;
		Vector2 occluder_offset;
		Ref<OccluderPolygon2D> occluder;
		Vector2 navigation_polygon_offset;
		Ref<NavigationPolygon> navigation_polygon;
		Ref<CanvasItemMaterial> material;
		Color modulate;
		float probability; // weight when painting at random among several tiles (editor only)
		int z_index; // draw order relative to the TileMap (like CanvasItem z); 0 = with the other tiles

		explicit Data() :
				one_way_collision_max_depth(0.0f),
				modulate(1, 1, 1),
				probability(1.0f),
				z_index(0) {}
	};

	Map<int, Data> tile_map;

	// Editor data: the atlas textures the TileSet editor shows as grids, with their cell layout.
	// The tiles themselves stay independent (texture + region); the game ignores this.
	struct Atlas {
		Ref<Texture> texture;
		Vector2 tile_size;
		Vector2 margin;
		Vector2 separation;
		Atlas() :
				tile_size(16, 16) {}
	};
	Vector<Atlas> atlases;

	// Editor data: named sets of tiles to paint at random ("grass variants"), weighted by each
	// tile's probability.
	struct RandomGroup {
		String name;
		DVector<int> tiles;
	};
	Vector<RandomGroup> random_groups;

	bool _set_atlas(const String &p_name, const Variant &p_value);
	bool _get_atlas(const String &p_name, Variant &r_ret) const;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _tile_set_shapes(int p_id, const Array &p_shapes);
	Array _tile_get_shapes(int p_id) const;
	Array _get_tiles_ids() const;

	static void _bind_methods();

public:
	void create_tile(int p_id);

	void tile_set_name(int p_id, const String &p_name);
	String tile_get_name(int p_id) const;

	void tile_set_texture(int p_id, const Ref<Texture> &p_texture);
	Ref<Texture> tile_get_texture(int p_id) const;

	void tile_set_texture_offset(int p_id, const Vector2 &p_offset);
	Vector2 tile_get_texture_offset(int p_id) const;

	void tile_set_shape_offset(int p_id, const Vector2 &p_offset);
	Vector2 tile_get_shape_offset(int p_id) const;

	void tile_set_region(int p_id, const Rect2 &p_region);
	Rect2 tile_get_region(int p_id) const;

	void tile_set_shape(int p_id, const Ref<Shape2D> &p_shape);
	Ref<Shape2D> tile_get_shape(int p_id) const;

	void tile_set_material(int p_id, const Ref<CanvasItemMaterial> &p_material);
	Ref<CanvasItemMaterial> tile_get_material(int p_id) const;

	void tile_set_modulate(int p_id, const Color &p_color);
	Color tile_get_modulate(int p_id) const;

	void tile_set_probability(int p_id, float p_probability);
	float tile_get_probability(int p_id) const;

	void tile_set_z_index(int p_id, int p_z_index);
	int tile_get_z_index(int p_id) const;

	void tile_set_occluder_offset(int p_id, const Vector2 &p_offset);
	Vector2 tile_get_occluder_offset(int p_id) const;

	void tile_set_light_occluder(int p_id, const Ref<OccluderPolygon2D> &p_light_occluder);
	Ref<OccluderPolygon2D> tile_get_light_occluder(int p_id) const;

	void tile_set_navigation_polygon_offset(int p_id, const Vector2 &p_offset);
	Vector2 tile_get_navigation_polygon_offset(int p_id) const;

	void tile_set_navigation_polygon(int p_id, const Ref<NavigationPolygon> &p_navigation_polygon);
	Ref<NavigationPolygon> tile_get_navigation_polygon(int p_id) const;

	void tile_set_shapes(int p_id, const Vector<Ref<Shape2D> > &p_shapes);
	Vector<Ref<Shape2D> > tile_get_shapes(int p_id) const;

	void tile_set_one_way_collision_direction(int p_id, Vector2 p_direction);
	Vector2 tile_get_one_way_collision_direction(int p_id) const;

	void tile_set_one_way_collision_max_depth(int p_id, float p_max_depth);
	float tile_get_one_way_collision_max_depth(int p_id) const;

	void remove_tile(int p_id);

	bool has_tile(int p_id) const;

	int find_tile_by_name(const String &p_name) const;
	void get_tile_list(List<int> *p_tiles) const;

	void clear();

	int get_last_unused_tile_id() const;

	int random_group_add(const String &p_name, const DVector<int> &p_tiles);
	void random_group_remove(int p_group);
	int random_group_get_count() const;
	String random_group_get_name(int p_group) const;
	DVector<int> random_group_get_tiles(int p_group) const;

	int atlas_add(const Ref<Texture> &p_texture);
	void atlas_remove(int p_atlas);
	int atlas_get_count() const;
	int atlas_find(const Ref<Texture> &p_texture) const;
	Ref<Texture> atlas_get_texture(int p_atlas) const;
	void atlas_set_tile_size(int p_atlas, const Vector2 &p_size);
	Vector2 atlas_get_tile_size(int p_atlas) const;
	void atlas_set_margin(int p_atlas, const Vector2 &p_margin);
	Vector2 atlas_get_margin(int p_atlas) const;
	void atlas_set_separation(int p_atlas, const Vector2 &p_separation);
	Vector2 atlas_get_separation(int p_atlas) const;

	TileSet();
};

#endif // TILE_SET_H
