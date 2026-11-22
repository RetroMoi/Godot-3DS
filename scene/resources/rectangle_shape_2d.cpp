/*************************************************************************/
/*  rectangle_shape_2d.cpp                                               */
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

#include "rectangle_shape_2d.h"

#include "servers/physics_2d_server.h"
#include "servers/visual_server.h"
void RectangleShape2D::_update_shape() {

	Physics2DServer::get_singleton()->shape_set_data(get_rid(), extents);
	emit_changed();
}

void RectangleShape2D::set_extents(const Vector2 &p_extents) {

	extents = p_extents;
	_update_shape();
}

Vector2 RectangleShape2D::get_extents() const {

	return extents;
}

void RectangleShape2D::draw(const RID &p_to_rid, const Color &p_color) {

	draw_fill(p_to_rid, p_color);
	if (is_debug_outline_enabled())
		draw_outline(p_to_rid, p_color);
}

void RectangleShape2D::draw_fill(const RID &p_to_rid, const Color &p_color) {

	VisualServer::get_singleton()->canvas_item_add_rect(p_to_rid, Rect2(-extents, extents * 2.0), p_color);
}

void RectangleShape2D::draw_outline(const RID &p_to_rid, const Color &p_color) {

	Rect2 r(-extents, extents * 2.0);
	if (r.size.x <= 2 || r.size.y <= 2)
		return;
#ifdef TOOLS_ENABLED
	// Editor: lines stay one screen pixel at any zoom.
	Vector<Vector2> pts;
	pts.push_back(r.pos);
	pts.push_back(Vector2(r.pos.x + r.size.x, r.pos.y));
	pts.push_back(r.pos + r.size);
	pts.push_back(Vector2(r.pos.x, r.pos.y + r.size.y));
	draw_debug_outline(p_to_rid, pts, p_color);
#else
	// Export templates (the 3DS): thin rects just inside the edges. Rects of one color batch into a
	// single draw; lines are one draw each.
	VisualServer *vs = VisualServer::get_singleton();
	Color c = p_color;
	c.a = 1.0;
	vs->canvas_item_add_rect(p_to_rid, Rect2(r.pos, Size2(r.size.x, 1)), c);
	vs->canvas_item_add_rect(p_to_rid, Rect2(r.pos.x, r.pos.y + r.size.y - 1, r.size.x, 1), c);
	vs->canvas_item_add_rect(p_to_rid, Rect2(r.pos.x, r.pos.y + 1, 1, r.size.y - 2), c);
	vs->canvas_item_add_rect(p_to_rid, Rect2(r.pos.x + r.size.x - 1, r.pos.y + 1, 1, r.size.y - 2), c);
#endif
}

Rect2 RectangleShape2D::get_rect() const {

	return Rect2(-extents, extents * 2.0);
}

void RectangleShape2D::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_extents", "extents"), &RectangleShape2D::set_extents);
	ObjectTypeDB::bind_method(_MD("get_extents"), &RectangleShape2D::get_extents);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "extents"), _SCS("set_extents"), _SCS("get_extents"));
}

RectangleShape2D::RectangleShape2D() :
		Shape2D(Physics2DServer::get_singleton()->shape_create(Physics2DServer::SHAPE_RECTANGLE)) {

	extents = Vector2(10, 10);
	_update_shape();
}
