/*************************************************************************/
/*  editor_palette_texture_import_plugin.h                               */
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

#ifndef EDITOR_PALETTE_TEXTURE_IMPORT_PLUGIN_H
#define EDITOR_PALETTE_TEXTURE_IMPORT_PLUGIN_H

#include "editor/editor_import_export.h"

class EditorNode;
class EditorPaletteTextureImportDialog;

// Import > Palette Texture: an indexed PNG plus palette files (.hex or indexed .png variants)
// into a PaletteTexture resource (.tres/.res). The sources are kept in the import metadata, so
// the editor offers to reimport when they change; a reimport updates the open resource in
// place (TileSets using it see the new data).
class EditorPaletteTextureImportPlugin : public EditorImportPlugin {

	OBJ_TYPE(EditorPaletteTextureImportPlugin, EditorImportPlugin);

	EditorPaletteTextureImportDialog *dialog;
	String last_error;

public:
	static EditorPaletteTextureImportPlugin *singleton;

	virtual String get_name() const;
	virtual String get_visible_name() const;
	virtual void import_dialog(const String &p_from = "");
	virtual Error import(const String &p_path, const Ref<ResourceImportMetadata> &p_from);
	String get_last_error() const { return last_error; }

	EditorPaletteTextureImportPlugin(EditorNode *p_editor);
};

#endif // EDITOR_PALETTE_TEXTURE_IMPORT_PLUGIN_H
