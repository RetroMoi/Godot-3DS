/*************************************************************************/
/*  tile_atlas_view.h                                                    */
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

#ifndef TILE_ATLAS_VIEW_H
#define TILE_ATLAS_VIEW_H

#include "scene/gui/control.h"
#include "scene/resources/tile_set.h"

// An atlas texture of a TileSet shown as a grid, with the tiles that use it outlined, selection
// and hover. Zoom: Ctrl + wheel. Scroll: wheel (Shift + wheel sideways). Pan: middle button, or
// Space + left button. Shared by the TileSet editor (create, select, remove tiles) and the
// TileMap palette (pick tiles).
//
// Signals:
//   cells_selected(Rect2 cells, bool shift, bool ctrl): left click or drag released; cells in
//     grid coordinates (a click is a 1x1 rect).
//   cells_rmb(Rect2 cells): right click.
class TileAtlasView : public Control {

	OBJ_TYPE(TileAtlasView, Control);

public:
	// A texture to show as an atlas: one of the TileSet's registered atlases (atlas >= 0) or a
	// texture its tiles use that has no atlas settings yet (atlas == -1, grid guessed).
	struct Source {
		Ref<Texture> texture;
		int atlas;
		Vector2 tile_size;
		Vector2 margin;
		Vector2 separation;
	};
	static void get_sources(const Ref<TileSet> &p_tileset, Vector<Source> *r_sources);

private:
	Ref<TileSet> tileset;
	Source source;
	bool has_source;

	float zoom;
	Vector2 ofs; // screen position of the texture's top-left corner
	bool fit_pending;
	bool show_ids;

	Set<int> selected;

	enum Drag {
		DRAG_NONE,
		DRAG_PAN,
		DRAG_SELECT,
	};
	Drag drag;
	Vector2 drag_from;
	Vector2 drag_to;
	Vector2 pan_mouse;
	Vector2 pan_ofs;
	Vector2 hover;
	bool hover_valid;

	Vector2 _texel_at(const Point2 &p_pos) const;
	void _draw_rect_outline(const Rect2 &p_rect, const Color &p_color, float p_width);
	void _zoom_at(float p_factor, const Point2 &p_pos);

protected:
	void _notification(int p_what);
	void _input_event(const InputEvent &p_event);
	static void _bind_methods();

public:
	void set_tileset(const Ref<TileSet> &p_tileset);
	void set_source(const Source &p_source);
	void clear_source();
	bool get_has_source() const { return has_source; }
	const Source &get_source() const { return source; }

	void set_selected(const Vector<int> &p_ids);
	void set_show_ids(bool p_show);
	void fit();

	// Grid helpers (texture pixels).
	Vector2 get_grid_size() const; // columns, rows
	Vector2 cell_at_texel(const Vector2 &p_texel) const;
	Rect2 cells_region(const Rect2 &p_cells) const;
	Rect2 tile_cells(int p_id) const; // the cells a tile covers (rounded to the grid)
	int tile_at_cell(const Vector2 &p_cell) const; // -1 if none
	void get_tiles_in(const Rect2 &p_cells, Vector<int> *r_ids) const;
	bool cell_is_empty(const Image &p_image, const Vector2 &p_cell) const; // fully transparent

	TileAtlasView();
};

#endif // TILE_ATLAS_VIEW_H
