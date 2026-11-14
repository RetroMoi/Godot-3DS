/*************************************************************************/
/*  collision_grid_2d.cpp                                                */
/*************************************************************************/
#include "collision_grid_2d.h"

#include "scene/2d/tile_map.h"
#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/segment_shape_2d.h"

#define GRID_EPSILON 0.001f
#define MAX_CANDIDATES 256

void CollisionGrid2D::set_cell_size(float p_size) {

	cell_size = MAX(p_size, 1.0f);
	dirty = true;
}

float CollisionGrid2D::get_cell_size() const {

	return cell_size;
}

static bool _shape_local_rect(const Ref<Shape2D> &p_shape, Rect2 &r_rect) {

	if (Ref<RectangleShape2D>(p_shape).is_valid()) {
		Vector2 e = Ref<RectangleShape2D>(p_shape)->get_extents();
		r_rect = Rect2(-e, e * 2);
		return true;
	}
	if (Ref<ConvexPolygonShape2D>(p_shape).is_valid()) {
		Vector<Vector2> points = Ref<ConvexPolygonShape2D>(p_shape)->get_points();
		if (points.empty())
			return false;
		r_rect = Rect2(points[0], Size2());
		for (int i = 1; i < points.size(); i++)
			r_rect.expand_to(points[i]);
		return true;
	}
	if (Ref<CircleShape2D>(p_shape).is_valid()) {
		float r = Ref<CircleShape2D>(p_shape)->get_radius();
		r_rect = Rect2(-r, -r, r * 2, r * 2);
		return true;
	}
	if (Ref<CapsuleShape2D>(p_shape).is_valid()) {
		Ref<CapsuleShape2D> c = p_shape;
		float r = c->get_radius(), h = c->get_height();
		r_rect = Rect2(-r, -h * 0.5f - r, r * 2, h + r * 2);
		return true;
	}
	if (Ref<SegmentShape2D>(p_shape).is_valid()) {
		Ref<SegmentShape2D> s = p_shape;
		r_rect = Rect2(s->get_a(), Size2());
		r_rect.expand_to(s->get_b());
		return true;
	}
	return false;
}

void CollisionGrid2D::add_tilemap(Object *p_tilemap) {

	TileMap *tm = p_tilemap ? p_tilemap->cast_to<TileMap>() : NULL;
	ERR_FAIL_COND(!tm);

	if (rects.empty() && cell_size <= 0)
		cell_size = MAX(tm->get_cell_size().x, tm->get_cell_size().y);

	List<TileMap::CollisionShape> shapes;
	tm->get_collision_shapes(&shapes);
	Matrix32 global = tm->is_inside_tree() ? tm->get_global_transform() : tm->get_transform();
	int skipped = 0;
	for (List<TileMap::CollisionShape>::Element *E = shapes.front(); E; E = E->next()) {
		Rect2 local;
		if (!_shape_local_rect(E->get().shape, local)) {
			skipped++;
			continue;
		}
		rects.push_back((global * E->get().xform).xform(local));
	}
	if (skipped)
		WARN_PRINT(("CollisionGrid2D: " + itos(skipped) + " tile shapes of an unsupported type were skipped.").utf8().get_data());
	dirty = true;
}

void CollisionGrid2D::add_rect(const Rect2 &p_rect) {

	rects.push_back(Rect2(p_rect.pos, p_rect.size.abs()));
	dirty = true;
}

void CollisionGrid2D::build() {

	if (dirty)
		_rebuild();
}

void CollisionGrid2D::clear() {

	rects.clear();
	dirty = true;
}

int CollisionGrid2D::get_rect_count() const {

	return rects.size();
}

void CollisionGrid2D::_rebuild() {

	dirty = false;
	cell_start.clear();
	cell_items.clear();
	width = height = 0;
	if (cell_size <= 0)
		cell_size = 16;
	int count = rects.size();
	visit_mark.resize(count);
	uint32_t *marks = visit_mark.ptr();
	for (int i = 0; i < count; i++)
		marks[i] = 0;
	query_id = 0;
	if (!count)
		return;

	// Cell range of each rect, and the bounds of them all. (Raw pointers: Vector's operator[]
	// checks copy-on-write on every write.)
	Vector<int> ranges_v;
	ranges_v.resize(count * 4);
	int *ranges = ranges_v.ptr();
	const Rect2 *rs = rects.ptr();
	int min_x = 0x7FFFFFFF, min_y = 0x7FFFFFFF, max_x = -0x7FFFFFFF, max_y = -0x7FFFFFFF;
	for (int i = 0; i < count; i++) {
		const Rect2 &r = rs[i];
		int *rg = ranges + i * 4;
		rg[0] = Math::floor(r.pos.x / cell_size);
		rg[1] = Math::floor(r.pos.y / cell_size);
		rg[2] = Math::floor((r.pos.x + r.size.x) / cell_size);
		rg[3] = Math::floor((r.pos.y + r.size.y) / cell_size);
		min_x = MIN(min_x, rg[0]);
		min_y = MIN(min_y, rg[1]);
		max_x = MAX(max_x, rg[2]);
		max_y = MAX(max_y, rg[3]);
	}
	origin_x = min_x;
	origin_y = min_y;
	width = max_x - min_x + 1;
	height = max_y - min_y + 1;
	if ((int64_t)width * height > 4 * 1024 * 1024) { // a 2048x2048-cell world at most
		width = height = 0;
		ERR_FAIL();
	}

	// Count, prefix sum, fill.
	int cells = width * height;
	cell_start.resize(cells + 1);
	int *start = cell_start.ptr();
	for (int i = 0; i <= cells; i++)
		start[i] = 0;
	for (int i = 0; i < count; i++) {
		const int *rg = ranges + i * 4;
		for (int y = rg[1]; y <= rg[3]; y++)
			for (int x = rg[0]; x <= rg[2]; x++)
				start[(y - origin_y) * width + (x - origin_x) + 1]++;
	}
	for (int i = 0; i < cells; i++)
		start[i + 1] += start[i];
	cell_items.resize(start[cells]);
	int *items = cell_items.ptr();
	Vector<int> fill_v;
	fill_v.resize(cells);
	int *fill = fill_v.ptr();
	for (int i = 0; i < cells; i++)
		fill[i] = start[i];
	for (int i = 0; i < count; i++) {
		const int *rg = ranges + i * 4;
		for (int y = rg[1]; y <= rg[3]; y++)
			for (int x = rg[0]; x <= rg[2]; x++)
				items[fill[(y - origin_y) * width + (x - origin_x)]++] = i;
	}
}

// Indices of the rects in the cells overlapping p_area (each once).
static int _gather(const CollisionGrid2D *p_grid, const Rect2 &p_area, int *r_out, float p_cell, int p_ox, int p_oy, int p_w, int p_h,
		const int *p_start, const int *p_items, uint32_t *p_mark, uint32_t p_query) {

	int x0 = MAX((int)Math::floor(p_area.pos.x / p_cell) - p_ox, 0);
	int y0 = MAX((int)Math::floor(p_area.pos.y / p_cell) - p_oy, 0);
	int x1 = MIN((int)Math::floor((p_area.pos.x + p_area.size.x) / p_cell) - p_ox, p_w - 1);
	int y1 = MIN((int)Math::floor((p_area.pos.y + p_area.size.y) / p_cell) - p_oy, p_h - 1);
	int n = 0;
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			int c = y * p_w + x;
			for (int k = p_start[c]; k < p_start[c + 1]; k++) {
				int idx = p_items[k];
				if (p_mark[idx] == p_query)
					continue;
				p_mark[idx] = p_query;
				if (n < MAX_CANDIDATES)
					r_out[n++] = idx;
			}
		}
	}
	return n;
}

#define GATHER(m_area, m_out)                                                                                          \
	(++query_id == 0 ? (void)(query_id = 1) : (void)0,                                                                   \
			_gather(this, m_area, m_out, cell_size, origin_x, origin_y, width, height, cell_start.ptr(), cell_items.ptr(), \
					visit_mark.ptr(), query_id))

float CollisionGrid2D::_move_axis(Rect2 &r_box, int p_axis, float p_amount, int *r_blocker) const {

	if (p_amount == 0)
		return 0;
	int o = 1 - p_axis;
	Rect2 sweep = r_box;
	if (p_amount > 0) {
		sweep.size[p_axis] += p_amount;
	} else {
		sweep.pos[p_axis] += p_amount;
		sweep.size[p_axis] -= p_amount;
	}

	int candidates[MAX_CANDIDATES];
	int n = GATHER(sweep, candidates);
	float allowed = p_amount;
	const Rect2 *rs = rects.ptr();
	float b_lo = r_box.pos[o], b_hi = r_box.pos[o] + r_box.size[o];
	for (int i = 0; i < n; i++) {
		const Rect2 &r = rs[candidates[i]];
		// Only obstacles beside the box on the other axis can block it.
		if (!(r.pos[o] < b_hi - GRID_EPSILON && r.pos[o] + r.size[o] > b_lo + GRID_EPSILON))
			continue;
		if (p_amount > 0) {
			float edge = r_box.pos[p_axis] + r_box.size[p_axis];
			if (r.pos[p_axis] >= edge - GRID_EPSILON) { // ahead (not already overlapping)
				float d = MAX(r.pos[p_axis] - edge, 0.0f);
				if (d < allowed) {
					allowed = d;
					if (r_blocker)
						*r_blocker = candidates[i];
				}
			}
		} else {
			float edge = r_box.pos[p_axis];
			float r_end = r.pos[p_axis] + r.size[p_axis];
			if (r_end <= edge + GRID_EPSILON) {
				float d = MIN(r_end - edge, 0.0f);
				if (d > allowed) {
					allowed = d;
					if (r_blocker)
						*r_blocker = candidates[i];
				}
			}
		}
	}
	r_box.pos[p_axis] += allowed;
	return allowed;
}

Vector2 CollisionGrid2D::move(const Rect2 &p_box, const Vector2 &p_motion, float p_corner_slide) const {

	if (dirty)
		const_cast<CollisionGrid2D *>(this)->_rebuild();
	if (!width)
		return p_motion;

	Rect2 box = p_box;
	Vector2 moved;
	for (int axis = 0; axis < 2; axis++) {
		float amount = p_motion[axis];
		if (amount == 0)
			continue;
		int blocker = -1;
		float done = _move_axis(box, axis, amount, &blocker);
		moved[axis] += done;

		// Corner slide: pushing straight into an obstacle that overlaps the box by at most
		// p_corner_slide px on the other axis, with free space past its corner.
		int o = 1 - axis;
		if (blocker < 0 || p_corner_slide <= 0 || p_motion[o] != 0 || Math::abs(done) >= Math::abs(amount))
			continue;
		const Rect2 &r = rects[blocker];
		float to_neg = (box.pos[o] + box.size[o]) - r.pos[o]; // shift toward - to clear it
		float to_pos = (r.pos[o] + r.size[o]) - box.pos[o]; // shift toward + to clear it
		float shift = 0;
		if (to_neg <= p_corner_slide && to_neg <= to_pos)
			shift = -to_neg;
		else if (to_pos <= p_corner_slide)
			shift = to_pos;
		if (shift == 0)
			continue;
		// Only if, once shifted, the way ahead is free.
		Rect2 probe = box;
		probe.pos[o] += shift;
		probe.pos[axis] += amount > 0 ? 0.01f : -0.01f;
		if (collides(probe))
			continue;
		float step = shift > 0 ? MIN(shift, Math::abs(amount)) : MAX(shift, -Math::abs(amount));
		moved[o] += _move_axis(box, o, step, NULL);
	}
	return moved;
}

bool CollisionGrid2D::collides(const Rect2 &p_box) const {

	if (dirty)
		const_cast<CollisionGrid2D *>(this)->_rebuild();
	if (!width)
		return false;
	int candidates[MAX_CANDIDATES];
	int n = GATHER(p_box, candidates);
	const Rect2 *rs = rects.ptr();
	for (int i = 0; i < n; i++) {
		const Rect2 &r = rs[candidates[i]];
		if (r.pos.x < p_box.pos.x + p_box.size.x - GRID_EPSILON && r.pos.x + r.size.x > p_box.pos.x + GRID_EPSILON &&
				r.pos.y < p_box.pos.y + p_box.size.y - GRID_EPSILON && r.pos.y + r.size.y > p_box.pos.y + GRID_EPSILON)
			return true;
	}
	return false;
}

bool CollisionGrid2D::segment_blocked(const Vector2 &p_from, const Vector2 &p_to) const {

	if (dirty)
		const_cast<CollisionGrid2D *>(this)->_rebuild();
	if (!width)
		return false;
	Rect2 area(p_from, Size2());
	area.expand_to(p_to);
	int candidates[MAX_CANDIDATES];
	int n = GATHER(area, candidates);
	const Rect2 *rs = rects.ptr();
	for (int i = 0; i < n; i++)
		if (rs[candidates[i]].intersects_segment(p_from, p_to))
			return true;
	return false;
}

Array CollisionGrid2D::get_rects_in(const Rect2 &p_area) const {

	Array ret;
	if (dirty)
		const_cast<CollisionGrid2D *>(this)->_rebuild();
	if (!width)
		return ret;
	int candidates[MAX_CANDIDATES];
	int n = GATHER(p_area, candidates);
	for (int i = 0; i < n; i++)
		if (rects[candidates[i]].intersects(p_area))
			ret.push_back(rects[candidates[i]]);
	return ret;
}

void CollisionGrid2D::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("get_rects_in", "area"), &CollisionGrid2D::get_rects_in);
	ObjectTypeDB::bind_method(_MD("set_cell_size", "size"), &CollisionGrid2D::set_cell_size);
	ObjectTypeDB::bind_method(_MD("get_cell_size"), &CollisionGrid2D::get_cell_size);
	ObjectTypeDB::bind_method(_MD("add_tilemap", "tilemap:TileMap"), &CollisionGrid2D::add_tilemap);
	ObjectTypeDB::bind_method(_MD("add_rect", "rect"), &CollisionGrid2D::add_rect);
	ObjectTypeDB::bind_method(_MD("build"), &CollisionGrid2D::build);
	ObjectTypeDB::bind_method(_MD("clear"), &CollisionGrid2D::clear);
	ObjectTypeDB::bind_method(_MD("get_rect_count"), &CollisionGrid2D::get_rect_count);
	ObjectTypeDB::bind_method(_MD("move", "box", "motion", "corner_slide"), &CollisionGrid2D::move, DEFVAL(0));
	ObjectTypeDB::bind_method(_MD("collides", "box"), &CollisionGrid2D::collides);
	ObjectTypeDB::bind_method(_MD("segment_blocked", "from", "to"), &CollisionGrid2D::segment_blocked);
}

CollisionGrid2D::CollisionGrid2D() {

	cell_size = 0; // from the first TileMap added (16 if none)
	dirty = true;
	origin_x = origin_y = width = height = 0;
	query_id = 0;
}
