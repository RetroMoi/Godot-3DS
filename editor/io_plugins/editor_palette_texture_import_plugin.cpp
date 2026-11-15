/*************************************************************************/
/*  editor_palette_texture_import_plugin.cpp                             */
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

#include "editor_palette_texture_import_plugin.h"

#include "editor/editor_file_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "io/resource_saver.h"
#include "os/file_access.h"
#include "scene/resources/palette_texture.h"

class EditorPaletteTextureImportDialog : public ConfirmationDialog {

	OBJ_TYPE(EditorPaletteTextureImportDialog, ConfirmationDialog);

	EditorPaletteTextureImportPlugin *plugin;

	LineEdit *image_path;
	LineEdit *palette_paths;
	LineEdit *save_path;
	EditorFileDialog *image_select;
	EditorFileDialog *palette_select;
	EditorFileDialog *save_select;
	AcceptDialog *error_dialog;

	void _error(const String &p_text) {

		error_dialog->set_text(p_text);
		error_dialog->popup_centered_minsize();
	}

public:
	void _choose_image(const String &p_path) {

		image_path->set_text(p_path);
		if (save_path->get_text().strip_edges() == "") {
			// Next to the image when it is in the project.
			String local = Globals::get_singleton()->localize_path(p_path);
			if (local.begins_with("res://"))
				save_path->set_text(local.get_base_dir().plus_file(local.get_file().basename() + "_palettes.res"));
		}
	}

	void _choose_palettes(const Vector<String> &p_paths) {

		String files;
		for (int i = 0; i < p_paths.size(); i++) {
			if (i > 0)
				files += ",";
			files += p_paths[i];
		}
		palette_paths->set_text(files);
	}

	void _choose_save(const String &p_path) {

		save_path->set_text(p_path);
	}

	void _browse_image() {

		image_select->popup_centered_ratio();
	}

	void _browse_palettes() {

		palette_select->popup_centered_ratio();
	}

	void _browse_save() {

		save_select->popup_centered_ratio();
	}

	void popup_import(const String &p_from) {

		popup_centered(Size2(520, 190) * EDSCALE);
		if (p_from == "")
			return;
		// Reimport: the sources from the resource's metadata.
		Ref<ResourceImportMetadata> rimd = ResourceLoader::load_import_metadata(p_from);
		ERR_FAIL_COND(!rimd.is_valid());
		save_path->set_text(p_from);
		if (rimd->get_source_count() > 0)
			image_path->set_text(EditorImportPlugin::expand_source_path(rimd->get_source_path(0)));
		String pals;
		for (int i = 1; i < rimd->get_source_count(); i++) {
			if (i > 1)
				pals += ",";
			pals += EditorImportPlugin::expand_source_path(rimd->get_source_path(i));
		}
		palette_paths->set_text(pals);
	}

	void _import() {

		String image = image_path->get_text().strip_edges();
		String dst = save_path->get_text().strip_edges();
		if (image == "") {
			_error(TTR("Choose the indexed PNG."));
			return;
		}
		if (!dst.begins_with("res://") || dst.extension() != "res") {
			// Binary: only .res keeps the import metadata (sources), needed to reimport.
			_error(TTR("The target must be a .res file inside the project (res://)."));
			return;
		}
		Ref<ResourceImportMetadata> imd = memnew(ResourceImportMetadata);
		imd->add_source(EditorImportPlugin::validate_source_path(image));
		Vector<String> pals = palette_paths->get_text().split(",", false);
		for (int i = 0; i < pals.size(); i++) {
			if (pals[i].strip_edges() != "")
				imd->add_source(EditorImportPlugin::validate_source_path(pals[i].strip_edges()));
		}
		Error err = plugin->import(dst, imd);
		if (err != OK) {
			_error(TTR("Couldn't import:") + "\n" + plugin->get_last_error());
			return;
		}
		hide();
	}

	static void _bind_methods() {

		ObjectTypeDB::bind_method("_choose_image", &EditorPaletteTextureImportDialog::_choose_image);
		ObjectTypeDB::bind_method("_choose_palettes", &EditorPaletteTextureImportDialog::_choose_palettes);
		ObjectTypeDB::bind_method("_choose_save", &EditorPaletteTextureImportDialog::_choose_save);
		ObjectTypeDB::bind_method("_browse_image", &EditorPaletteTextureImportDialog::_browse_image);
		ObjectTypeDB::bind_method("_browse_palettes", &EditorPaletteTextureImportDialog::_browse_palettes);
		ObjectTypeDB::bind_method("_browse_save", &EditorPaletteTextureImportDialog::_browse_save);
		ObjectTypeDB::bind_method("_import", &EditorPaletteTextureImportDialog::_import);
	}

	EditorPaletteTextureImportDialog(EditorPaletteTextureImportPlugin *p_plugin) {

		plugin = p_plugin;
		set_title(TTR("Import Palette Texture"));

		VBoxContainer *vbc = memnew(VBoxContainer);
		add_child(vbc);
		set_child_rect(vbc);

		HBoxContainer *hbc = memnew(HBoxContainer);
		vbc->add_margin_child(TTR("Indexed PNG (indices and its own palette, \"base\"):"), hbc);
		image_path = memnew(LineEdit);
		image_path->set_h_size_flags(SIZE_EXPAND_FILL);
		hbc->add_child(image_path);
		Button *b = memnew(Button);
		b->set_text(" .. ");
		b->connect("pressed", this, "_browse_image");
		hbc->add_child(b);

		hbc = memnew(HBoxContainer);
		vbc->add_margin_child(TTR("Palettes (.hex, or indexed .png variants; name = file suffix, e.g. atlas-dusk.hex -> dusk):"), hbc);
		palette_paths = memnew(LineEdit);
		palette_paths->set_h_size_flags(SIZE_EXPAND_FILL);
		hbc->add_child(palette_paths);
		b = memnew(Button);
		b->set_text(" .. ");
		b->connect("pressed", this, "_browse_palettes");
		hbc->add_child(b);

		hbc = memnew(HBoxContainer);
		vbc->add_margin_child(TTR("Save as (.res, keeps the sources for reimporting):"), hbc);
		save_path = memnew(LineEdit);
		save_path->set_h_size_flags(SIZE_EXPAND_FILL);
		hbc->add_child(save_path);
		b = memnew(Button);
		b->set_text(" .. ");
		b->connect("pressed", this, "_browse_save");
		hbc->add_child(b);

		image_select = memnew(EditorFileDialog);
		image_select->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
		image_select->set_mode(EditorFileDialog::MODE_OPEN_FILE);
		image_select->add_filter("*.png ; PNG");
		image_select->connect("file_selected", this, "_choose_image");
		add_child(image_select);

		palette_select = memnew(EditorFileDialog);
		palette_select->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
		palette_select->set_mode(EditorFileDialog::MODE_OPEN_FILES);
		palette_select->add_filter("*.hex ; Palette (one RRGGBB per line)");
		palette_select->add_filter("*.png ; Indexed PNG");
		palette_select->connect("files_selected", this, "_choose_palettes");
		add_child(palette_select);

		save_select = memnew(EditorFileDialog);
		save_select->set_access(EditorFileDialog::ACCESS_RESOURCES);
		save_select->set_mode(EditorFileDialog::MODE_SAVE_FILE);
		save_select->add_filter("*.res ; Binary Resource");
		save_select->connect("file_selected", this, "_choose_save");
		add_child(save_select);

		error_dialog = memnew(AcceptDialog);
		add_child(error_dialog);

		get_ok()->set_text(TTR("Import"));
		get_ok()->connect("pressed", this, "_import");
		set_hide_on_ok(false);
	}
};

EditorPaletteTextureImportPlugin *EditorPaletteTextureImportPlugin::singleton = NULL;

String EditorPaletteTextureImportPlugin::get_name() const {

	return "palette_texture";
}

String EditorPaletteTextureImportPlugin::get_visible_name() const {

	return TTR("Palette Texture");
}

void EditorPaletteTextureImportPlugin::import_dialog(const String &p_from) {

	dialog->popup_import(p_from);
}

Error EditorPaletteTextureImportPlugin::import(const String &p_path, const Ref<ResourceImportMetadata> &p_from) {

	last_error = "";
	ERR_FAIL_COND_V(p_from->get_source_count() < 1, ERR_INVALID_PARAMETER);
	Ref<ResourceImportMetadata> from = p_from;

	// res:// paths inside the project (the resource stores them: no machine-specific paths).
	String image = Globals::get_singleton()->localize_path(EditorImportPlugin::expand_source_path(from->get_source_path(0)));
	Vector<String> palettes;
	for (int i = 1; i < from->get_source_count(); i++)
		palettes.push_back(Globals::get_singleton()->localize_path(EditorImportPlugin::expand_source_path(from->get_source_path(i))));

	// Reimport into the resource already loaded (TileSets using it update right away).
	Ref<PaletteTexture> target;
	if (ResourceCache::has(p_path))
		target = Ref<Resource>(ResourceCache::get(p_path));
	if (target.is_null())
		target.instance();

	Error err = target->create_from_sources(image, palettes);
	if (err != OK) {
		last_error = target->get_last_error();
		return err;
	}

	for (int i = 0; i < from->get_source_count(); i++)
		from->set_source_md5(i, FileAccess::get_md5(EditorImportPlugin::expand_source_path(from->get_source_path(i))));
	from->set_editor(get_name());
	target->set_import_metadata(from);

	err = ResourceSaver::save(p_path, target);
	if (err != OK)
		last_error = TTR("Couldn't save") + " " + p_path + ".";
	else
		target->set_path(p_path);
	return err;
}

EditorPaletteTextureImportPlugin::EditorPaletteTextureImportPlugin(EditorNode *p_editor) {

	singleton = this;
	dialog = memnew(EditorPaletteTextureImportDialog(this));
	p_editor->get_gui_base()->add_child(dialog);
}
