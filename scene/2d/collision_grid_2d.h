/*************************************************************************/
/*  collision_grid_2d.h                                                  */
/*************************************************************************/
#ifndef COLLISION_GRID_2D_H
#define COLLISION_GRID_2D_H

#include "reference.h"
#include "math_2d.h"
#include "array.h"

class TileMap;

// World collision for tile-based games, without the physics engine.
//
// The static obstacles (a TileMap's collision shapes, as their bounding boxes, plus any extra
// rects) go into a dense grid of cells; each cell lists the few boxes touching it. Moving a
// box tests only the boxes in the cells it sweeps, one axis at a time: a fixed, tiny cost no
// matter how big the map is, and exact for box-shaped obstacles. This is how classic tile
// games move characters, and far cheaper than a KinematicBody2D's shape-vs-shape tests on a
// slow CPU (Nintendo 3DS).
//
//   var grid = CollisionGrid2D.new()
//   grid.add_tilemap(get_node("trees"))
//   ...
//   var moved = grid.move(Rect2(pos + feet_offset, feet_size), velocity * delta, 3)
//   set_pos(pos + moved)
//
// Corner slide: when a move along one axis is blocked by at most `corner_slide` pixels of an
// obstacle's corner, the box is nudged sideways around it (like in Zelda-like games).
class CollisionGrid2D : public Reference {

	OBJ_TYPE(CollisionGrid2D, Reference);

	float cell_size;
	Vector<Rect2> rects; // all obstacles, world (global) coordinates

	// Dense grid over the obstacles' bounds: cell (x, y) lists rect indices
	// cell_items[cell_start[i] .. cell_start[i + 1]).
	bool dirty;
	int origin_x, origin_y, width, height;
	Vector<int> cell_start;
	Vector<int> cell_items;
	mutable Vector<uint32_t> visit_mark; // per rect: last query that saw it (dedup)
	mutable uint32_t query_id;

	void _rebuild();
	template <class F>
	void _for_each_rect(const Rect2 &p_area, F &p_func) const;
	float _move_axis(Rect2 &r_box, int p_axis, float p_amount, int *r_blocker) const;

protected:
	static void _bind_methods();

public:
	void set_cell_size(float p_size);
	float get_cell_size() const;

	void add_tilemap(Object *p_tilemap);
	void add_rect(const Rect2 &p_rect);
	void clear();
	int get_rect_count() const;

	// Moves p_box by p_motion, stopping at obstacles; returns the motion actually made.
	Vector2 move(const Rect2 &p_box, const Vector2 &p_motion, float p_corner_slide = 0) const;
	// Does p_box overlap an obstacle?
	bool collides(const Rect2 &p_box) const;
	// Does the segment cross an obstacle?
	bool segment_blocked(const Vector2 &p_from, const Vector2 &p_to) const;
	// The obstacles overlapping p_area (debugging, editor tools).
	Array get_rects_in(const Rect2 &p_area) const;

	CollisionGrid2D();
};

#endif
