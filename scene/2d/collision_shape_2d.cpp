/*************************************************************************/
/*  collision_shape_2d.cpp                                               */
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

#include "collision_shape_2d.h"
#include "collision_object_2d.h"
#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/segment_shape_2d.h"
#include "scene/resources/shape_line_2d.h"
#include "scene/main/scene_main_loop.h"
#include "servers/visual_server.h"

void CollisionShape2D::_add_to_collision_object(Object *p_obj) {

	if (unparenting)
		return;

	CollisionObject2D *co = p_obj->cast_to<CollisionObject2D>();
	ERR_FAIL_COND(!co);
	update_shape_index = co->get_shape_count();
	co->add_shape(shape, get_transform());
	if (trigger)
		co->set_shape_as_trigger(co->get_shape_count() - 1, true);
}

void CollisionShape2D::_shape_changed() {

	update();
	_update_parent();
}

void CollisionShape2D::_update_xform_in_parent() {

	if (update_shape_index >= 0) {

		CollisionObject2D *co = get_parent()->cast_to<CollisionObject2D>();
		if (co) {
			co->set_shape_transform(update_shape_index, get_transform());
		}
	}
}

void CollisionShape2D::_update_parent() {

	Node *parent = get_parent();
	if (!parent)
		return;
	CollisionObject2D *co = parent->cast_to<CollisionObject2D>();
	if (!co)
		return;
	co->_update_shapes_from_children();
}

// In a running game without "Visible Collision Shapes" this node draws nothing: its canvas item
// leaves the visual server's walk (the node's own visibility is untouched), unless it has
// children that draw.
static void _collision_debug_skip_draw(CanvasItem *p_ci) {

	SceneTree *st = p_ci->get_tree();
	bool skip = st && !st->is_editor_hint() && !st->is_debugging_collisions_hint();
	for (int i = 0; skip && i < p_ci->get_child_count(); i++) {
		if (p_ci->get_child(i)->cast_to<CanvasItem>())
			skip = false;
	}
	VisualServer::get_singleton()->canvas_item_set_visible(p_ci->get_canvas_item(), !skip && !p_ci->is_hidden());
}

void CollisionShape2D::add_child_notify(Node *p_child) {

	if (is_inside_tree() && p_child->cast_to<CanvasItem>())
		_collision_debug_skip_draw(this);
}

void CollisionShape2D::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE: {
			unparenting = false;
			can_update_body = get_tree()->is_editor_hint();
			if (!get_tree()->is_editor_hint()) {
				_update_xform_in_parent();
				//display above all else
				set_z_as_relative(false);
				set_z(VS::CANVAS_ITEM_Z_MAX - 1);
			}
			_collision_debug_skip_draw(this);

		} break;
		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {

			if (!is_inside_tree())
				break;
			if (can_update_body) {
				_update_parent();
			} else {
				_update_xform_in_parent();
			}

		} break;
		case NOTIFICATION_EXIT_TREE: {
			can_update_body = false;

		} break;
		/*
		case NOTIFICATION_TRANSFORM_CHANGED: {

			if (!is_inside_scene())
				break;
			_update_parent();

		} break;*/
		case NOTIFICATION_DRAW: {

			if (!get_tree()->is_editor_hint() && !get_tree()->is_debugging_collisions_hint()) {
				break;
			}

			if (!shape.is_valid()) {
				break;
			}

			rect = Rect2();

			shape->draw(get_canvas_item(), get_debug_color());

			rect = shape->get_rect();
			rect = rect.grow(3);

		} break;
		case NOTIFICATION_UNPARENTED: {
			unparenting = true;
			_update_parent();
		} break;
	}
}

void CollisionShape2D::set_shape(const Ref<Shape2D> &p_shape) {

	if (shape.is_valid())
		shape->disconnect("changed", this, "_shape_changed");
	shape = p_shape;
	update();
	if (is_inside_tree() && can_update_body)
		_update_parent();
	if (is_inside_tree() && !can_update_body && update_shape_index >= 0) {
		CollisionObject2D *co = get_parent()->cast_to<CollisionObject2D>();
		if (co) {
			co->set_shape(update_shape_index, p_shape);
		}
	}
	if (shape.is_valid())
		shape->connect("changed", this, "_shape_changed");

	update_configuration_warning();
}

Ref<Shape2D> CollisionShape2D::get_shape() const {

	return shape;
}

// A shape whose size the editor box edits directly (see edit_set_rect).
static bool _is_box_resizable(const Ref<Shape2D> &p_shape) {

	return p_shape.is_valid() && (p_shape->cast_to<RectangleShape2D>() || p_shape->cast_to<CircleShape2D>() || p_shape->cast_to<CapsuleShape2D>());
}

Rect2 CollisionShape2D::get_item_rect() const {

	// From the shape itself, not from the last draw (which only happens in the editor or with
	// visible collisions). Resizable shapes: the exact bounds, so the editor box and its handles
	// sit on the shape at any zoom (a margin in canvas units grows with the zoom).
	if (_is_box_resizable(shape))
		return shape->get_rect();
	if (shape.is_valid())
		return shape->get_rect().grow(3); // thin shapes (segment, ray): still clickable
	return rect;
}

Variant CollisionShape2D::edit_get_state() const {

	Array state = Node2D::edit_get_state();
	if (shape.is_valid()) {
		if (shape->cast_to<RectangleShape2D>())
			state.push_back(shape->cast_to<RectangleShape2D>()->get_extents());
		else if (shape->cast_to<CircleShape2D>())
			state.push_back(shape->cast_to<CircleShape2D>()->get_radius());
		else if (shape->cast_to<CapsuleShape2D>())
			state.push_back(Vector2(shape->cast_to<CapsuleShape2D>()->get_radius(), shape->cast_to<CapsuleShape2D>()->get_height()));
	}
	return state;
}

void CollisionShape2D::edit_set_state(const Variant &p_state) {

	Array state = p_state;
	ERR_FAIL_COND(state.size() < 3);
	Array node_state;
	for (int i = 0; i < 3; i++)
		node_state.push_back(state[i]);
	Node2D::edit_set_state(node_state);
	if (state.size() < 4 || shape.is_null())
		return;
	if (shape->cast_to<RectangleShape2D>() && state[3].get_type() == Variant::VECTOR2)
		shape->cast_to<RectangleShape2D>()->set_extents(state[3]);
	else if (shape->cast_to<CircleShape2D>() && (state[3].get_type() == Variant::REAL || state[3].get_type() == Variant::INT))
		shape->cast_to<CircleShape2D>()->set_radius(state[3]);
	else if (shape->cast_to<CapsuleShape2D>() && state[3].get_type() == Variant::VECTOR2) {
		Vector2 rh = state[3];
		shape->cast_to<CapsuleShape2D>()->set_radius(rh.x);
		shape->cast_to<CapsuleShape2D>()->set_height(rh.y);
	}
}

void CollisionShape2D::edit_set_rect(const Rect2 &p_edit_rect) {

	if (!_is_box_resizable(shape)) {
		Node2D::edit_set_rect(p_edit_rect); // scales the node, as before
		return;
	}

	// p_edit_rect is the new box in the node's local space (the shape is centered on the
	// origin): give the shape that size and move the node to its center, so the edge opposite
	// the dragged one stays put.
	Rect2 r = p_edit_rect;
	Size2 size(Math::abs(r.size.x), Math::abs(r.size.y));
	Vector2 center = r.pos + r.size / 2;
	Size2 old = shape->get_rect().size;

	if (shape->cast_to<RectangleShape2D>()) {
		shape->cast_to<RectangleShape2D>()->set_extents(Vector2(MAX(size.x / 2, 0.5), MAX(size.y / 2, 0.5)));
	} else if (shape->cast_to<CircleShape2D>()) {
		// A circle has one size: follow the side that changed the most.
		float d = Math::abs(size.x - old.x) >= Math::abs(size.y - old.y) ? size.x : size.y;
		shape->cast_to<CircleShape2D>()->set_radius(MAX(d / 2, 0.5));
	} else {
		// Capsule (vertical): the width is the diameter, the rest of the height is the middle
		// (at least 1: with a zero middle its outline repeats points and can't be drawn).
		CapsuleShape2D *capsule = shape->cast_to<CapsuleShape2D>();
		float radius = MAX(size.x / 2, 0.5);
		capsule->set_radius(radius);
		capsule->set_height(MAX(size.y - radius * 2, 1));
	}

	set_pos(get_pos() + get_transform().basis_xform(center));
}

void CollisionShape2D::set_trigger(bool p_trigger) {

	trigger = p_trigger;
	if (can_update_body) {
		_update_parent();
	} else if (is_inside_tree() && update_shape_index >= 0) {
		CollisionObject2D *co = get_parent()->cast_to<CollisionObject2D>();
		if (co) {
			co->set_shape_as_trigger(update_shape_index, p_trigger);
		}
	}
}

bool CollisionShape2D::is_trigger() const {

	return trigger;
}

void CollisionShape2D::_set_update_shape_index(int p_index) {

	update_shape_index = p_index;
}

int CollisionShape2D::_get_update_shape_index() const {

	return update_shape_index;
}

String CollisionShape2D::get_configuration_warning() const {

	if (!get_parent()->cast_to<CollisionObject2D>()) {
		return TTR("CollisionShape2D only serves to provide a collision shape to a CollisionObject2D derived node. Please only use it as a child of Area2D, StaticBody2D, RigidBody2D, KinematicBody2D, etc. to give them a shape.");
	}

	if (!shape.is_valid()) {
		return TTR("A shape must be provided for CollisionShape2D to function. Please create a shape resource for it!");
	}

	return String();
}

void CollisionShape2D::set_debug_color(const Color &p_color) {

	// A color equal to the project's counts as "no color of its own": it keeps following the project.
	debug_color_custom = p_color != Shape2D::get_default_debug_color();
	debug_color = p_color;
	update();
}

Color CollisionShape2D::get_debug_color() const {

	return debug_color_custom ? debug_color : Shape2D::get_default_debug_color();
}

void CollisionShape2D::_validate_property(PropertyInfo &property) const {

	// Saved only when the shape has a color of its own.
	if (property.name == "debug_color" && !debug_color_custom)
		property.usage &= ~PROPERTY_USAGE_STORAGE;
}

Variant CollisionShape2D::_property_get_revert(const String &p_name) const {

	// Inspector revert button: back to the project's color.
	if (p_name == "debug_color")
		return Shape2D::get_default_debug_color();
	return Variant();
}

void CollisionShape2D::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("set_debug_color", "color"), &CollisionShape2D::set_debug_color);
	ObjectTypeDB::bind_method(_MD("get_debug_color"), &CollisionShape2D::get_debug_color);
	ObjectTypeDB::bind_method(_MD("_property_get_revert", "property"), &CollisionShape2D::_property_get_revert);

	ObjectTypeDB::bind_method(_MD("set_shape", "shape"), &CollisionShape2D::set_shape);
	ObjectTypeDB::bind_method(_MD("get_shape"), &CollisionShape2D::get_shape);
	ObjectTypeDB::bind_method(_MD("_shape_changed"), &CollisionShape2D::_shape_changed);
	ObjectTypeDB::bind_method(_MD("_add_to_collision_object"), &CollisionShape2D::_add_to_collision_object);
	ObjectTypeDB::bind_method(_MD("set_trigger", "enable"), &CollisionShape2D::set_trigger);
	ObjectTypeDB::bind_method(_MD("is_trigger"), &CollisionShape2D::is_trigger);

	ObjectTypeDB::bind_method(_MD("_set_update_shape_index", "index"), &CollisionShape2D::_set_update_shape_index);
	ObjectTypeDB::bind_method(_MD("_get_update_shape_index"), &CollisionShape2D::_get_update_shape_index);

	ObjectTypeDB::bind_method(_MD("get_collision_object_shape_index"), &CollisionShape2D::get_collision_object_shape_index);

	ADD_PROPERTYNZ(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape2D"), _SCS("set_shape"), _SCS("get_shape"));
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trigger"), _SCS("set_trigger"), _SCS("is_trigger"));
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_color"), _SCS("set_debug_color"), _SCS("get_debug_color"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "_update_shape_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), _SCS("_set_update_shape_index"), _SCS("_get_update_shape_index"));
}

CollisionShape2D::CollisionShape2D() {

	rect = Rect2(-Point2(10, 10), Point2(20, 20));
	set_notify_local_transform(true);
	trigger = false;
	unparenting = false;
	can_update_body = false;
	update_shape_index = -1;
	debug_color = Shape2D::get_default_debug_color();
	debug_color_custom = false;
}
