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
#include "scene/gui/spin_box.h"
#include "scene/resources/tile_set.h"

// One tile of a TileSet seen as an object of its own ("name", "texture", "region"...), so the
// regular property editor can edit it. Changes go straight to the TileSet ("<id>/<property>").
class TileSetEditorTileProxy : public Object {

	OBJ_TYPE(TileSetEditorTileProxy, Object);

	Ref<TileSet> tileset;
	int id;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void setup(const Ref<TileSet> &p_tileset, int p_id);
	int get_id() const { return id; }
	void notify_changed() { _change_notify(""); }

	TileSetEditorTileProxy() { id = -1; }
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
	SpinBox *size_x, *size_y, *margin_x, *margin_y, *sep_x, *sep_y;
	CheckBox *show_ids;
	EditorFileDialog *file_dialog;
	MenuButton *menu;
	ConfirmationDialog *cd;
	EditorNameDialog *nd;
	AcceptDialog *err_dialog;

	enum {

		MENU_OPTION_ADD_ITEM,
		MENU_OPTION_REMOVE_ITEM,
		MENU_OPTION_CREATE_FROM_SCENE,
		MENU_OPTION_MERGE_FROM_SCENE,
		MENU_OPTION_CREATE_TILES_IN_ATLAS,
		MENU_OPTION_REMOVE_ATLAS,
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
	void _cells_selected(const Rect2 &p_cells, bool p_shift, bool p_ctrl);
	void _cells_rmb(const Rect2 &p_cells);
	void _remove_tile_pressed();
	void _show_ids_toggled(bool p_on);
	void _mode_pressed(bool p_create);
	void _select_tile(int p_id);
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
