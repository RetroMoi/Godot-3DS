/*************************************************************************/
/*  tile_set_editor_plugin.h                                             */
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

#ifndef TILE_SET_EDITOR_PLUGIN_H
#define TILE_SET_EDITOR_PLUGIN_H

#include "editor/editor_file_dialog.h"
#include "editor/editor_name_dialog.h"
#include "editor/editor_node.h"
#include "editor/plugins/tile_atlas_view.h"
#include "editor/property_editor.h"
#include "scene/gui/item_list.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/shape_2d.h"
#include "scene/resources/tile_set.h"

// One tile of a TileSet seen as an object of its own ("name", "texture", "region"...), so the
// regular property editor can edit it. Changes go straight to the TileSet ("<id>/<property>").
class TileSetEditorTileProxy : public Object {

	OBJ_TYPE(TileSetEditorTileProxy, Object);

	Ref<TileSet> tileset;
	int id;
	bool setting;

	Ref<Shape2D> _get_shape() const;
	void _notify_list_changed() { _change_notify(""); }

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	void setup(const Ref<TileSet> &p_tileset, int p_id);
	int get_id() const { return id; }
	void notify_changed() { _change_notify(""); }
	bool is_setting() const { return setting; }

	TileSetEditorTileProxy() {
		id = -1;
		setting = false;
	}
};

// The selected tile as the TileMap draws it: its cell (and the neighbours), the cell's origin
// for the TileMap's Tile Origin, the texture moved by Tex Offset and the collision shape moved by
// Shape Offset, zoomed to fit.
class TileSetTilePreview : public Control {

	OBJ_TYPE(TileSetTilePreview, Control);

	Ref<TileSet> tileset;
	int tile;
	Size2 cell_size;
	int origin; // TileMap::TILE_ORIGIN_*

	UndoRedo *undo_redo;

	// Last drawn mapping: screen = view_ofs + tile space * view_zoom.
	Vector2 view_ofs;
	float view_zoom;

	// Zoom and pan, as in Godot 4's tile polygon editor: the view fits the tile until the mouse
	// wheel zooms (towards the cursor) or the middle button pans it; the reset button (or another
	// tile) fits it again.
	float manual_zoom; // 0: fit the tile
	Vector2 manual_ofs;
	bool panning;
	Vector2 pan_last;
	HBoxContainer *zoom_buttons;
	ToolButton *zoom_less;
	ToolButton *zoom_reset;
	ToolButton *zoom_more;
	void _zoom_at(float p_zoom, const Vector2 &p_screen_pos);
	void _zoom_step(int p_dir, const Vector2 &p_screen_pos);
	void _zoom_less();
	void _zoom_more();
	void _zoom_fit();

	// Dragging the collision shape: move it (Shape Offset) or resize it (a rectangle's extents
	// from its corner handle, a circle's radius from its right handle).
	enum Drag {
		DRAG_NONE,
		DRAG_MOVE,
		DRAG_EXTENTS,
		DRAG_RADIUS,
	};
	Drag drag;
	Vector2 drag_from; // tile space
	Vector2 drag_start_offset;
	Variant drag_start_value;

	Vector2 _cell_anchor() const; // in cell coordinates (0,0 = the cell's top-left)
	Vector2 _texture_pos(const Size2 &p_size, const Vector2 &p_ofs) const;
	Size2 _tile_size() const;
	Vector2 _shape_pos() const; // where the TileMap puts the shape's origin, in cell coordinates
	Vector2 _handle_pos() const; // the resize handle, in the shape's coordinates (Vector2() if none)

protected:
	void _notification(int p_what);
	void _input_event(const InputEvent &p_event);
	static void _bind_methods();

public:
	void set_undo_redo(UndoRedo *p_undo_redo) { undo_redo = p_undo_redo; }
	void set_tile(const Ref<TileSet> &p_tileset, int p_tile);
	void set_cell(const Size2 &p_cell_size, int p_origin);
	void add_rect_shape(); // a RectangleShape2D covering the cell
	Size2 get_cell_size() const { return cell_size; }
	int get_origin() const { return origin; }

	TileSetTilePreview();
};

// The TileSet editor (bottom panel "TileSet"), in the spirit of Godot 4's: the atlas textures on
// the left, the selected atlas as a grid in the middle (click an empty cell to create a tile,
// drag over empty cells for a tile of several cells, right click to remove), the selected tile's
// properties on the right. Tiles stay plain TileSet entries (texture + region): the game, the
// TileMap and the 3DS renderer see nothing new.
class TileSetEditor : public VBoxContainer {

	OBJ_TYPE(TileSetEditor, VBoxContainer);

	static TileSetEditor *singleton;

	Ref<TileSet> tileset;
	EditorNode *editor;

	Vector<TileAtlasView::Source> sources;
	int current_source;
	int selected_tile;
	Map<int, TileSetEditorTileProxy *> proxies;
	bool updating;

	ItemList *source_list;
	Button *mode_select;
	Button *mode_create;
	bool create_mode;
	TileAtlasView *atlas_view;
	Label *tile_label;
	Button *remove_tile_button;
	PropertyEditor *tile_properties;
	TileSetTilePreview *tile_preview;
	OptionButton *preview_origin;
	Label *preview_info;
	Button *preview_add_shape;
	SpinBox *size_x, *size_y, *margin_x, *margin_y, *sep_x, *sep_y;
	CheckBox *show_ids;
	EditorFileDialog *file_dialog;
	EditorFileDialog *palette_dialog;
	MenuButton *menu;
	ConfirmationDialog *cd;
	EditorNameDialog *nd;
	AcceptDialog *err_dialog;
	AcceptDialog *info_dialog;

	enum {

		MENU_OPTION_ADD_ITEM,
		MENU_OPTION_REMOVE_ITEM,
		MENU_OPTION_CREATE_FROM_SCENE,
		MENU_OPTION_MERGE_FROM_SCENE,
		MENU_OPTION_CREATE_TILES_IN_ATLAS,
		MENU_OPTION_REMOVE_ATLAS,
		MENU_OPTION_CUSTOM_DATA_LAYERS,
		MENU_OPTION_USE_PALETTE_TEXTURE,
	};

	int option;
	void _menu_cbk(int p_option);
	void _menu_confirm();
	void _name_dialog_confirm(const String &name);

	void _tileset_changed();
	void _update_sources();
	void _source_selected(int p_index);
	void _update_grid_fields();
	void _grid_changed(double);
	void _add_atlas_pressed();
	void _atlas_file_selected(const String &p_path);
	void _palette_texture_selected(const String &p_path);
	void _cells_selected(const Rect2 &p_cells, bool p_shift, bool p_ctrl);
	void _cells_rmb(const Rect2 &p_cells);
	void _remove_tile_pressed();
	void _show_ids_toggled(bool p_on);
	void _mode_pressed(bool p_create);
	void _select_tile(int p_id);
	void _update_preview();
	void _preview_origin_selected(int p_origin);
	void _preview_shape_edited();
	void _preview_add_shape_pressed();
	void _property_edited(const Variant &p_prop); // a String, or an Array when it comes through UndoRedo
	void _resource_selected(Object *p_res, const String &p_prop);
	void _create_tile(const Rect2 &p_cells);
	void _remove_tile(int p_id);
	void _create_tiles_in_atlas();
	void _register_source_atlas(UndoRedo *p_ur); // adds do/undo to give an implicit source atlas settings
	TileSetEditorTileProxy *_get_proxy(int p_id);

	static void _import_node(Node *p_node, Ref<TileSet> p_library);
	static void _import_scene(Node *p_scene, Ref<TileSet> p_library, bool p_merge);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static TileSetEditor *get_singleton() { return singleton; }

	void edit(const Ref<TileSet> &p_tileset);
	Ref<TileSet> get_edited() const { return tileset; }
	// The TileMap being edited (its cell size and Tile Origin, for the preview).
	void set_cell_context(const Size2 &p_cell_size, int p_origin);
	static Error update_library_file(Node *p_base_scene, Ref<TileSet> ml, bool p_merge = true);

	TileSetEditor(EditorNode *p_editor);
	~TileSetEditor();
};

class TileSetEditorPlugin : public EditorPlugin {

	OBJ_TYPE(TileSetEditorPlugin, EditorPlugin);

	static TileSetEditorPlugin *singleton;

	TileSetEditor *tileset_editor;
	ToolButton *button;
	EditorNode *editor;
	bool editing_resource; // a TileSet is the edited object (not just shown for a TileMap)
	bool shown_for_tilemap;

	void _update_button();

public:
	static TileSetEditorPlugin *get_singleton() { return singleton; }

	// The TileMap editor shows its TileMap's TileSet here too, as Godot 4 does.
	void show_for_tilemap(const Ref<TileSet> &p_tileset);
	void hide_for_tilemap();

	virtual String get_name() const { return "TileSet"; }
	bool has_main_screen() const { return false; }
	virtual void edit(Object *p_node);
	virtual bool handles(Object *p_node) const;
	virtual void make_visible(bool p_visible);

	TileSetEditorPlugin(EditorNode *p_node);
};

#endif // TILE_SET_EDITOR_PLUGIN_H
