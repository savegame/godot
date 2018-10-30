/**
 * Export options for sailfish OS 
 */

#include "export.h"
#include "editor/editor_import_export.h"
#include "platform/sailfish/logo.gen.h"
#include "scene/resources/texture.h"

void register_sailfish_exporter() {

	Image img(_sailfish_logo);
	Ref<ImageTexture> logo = memnew(ImageTexture);
	logo->create_from_image(img);

	{
		Ref<EditorExportPlatformPC> exporter = Ref<EditorExportPlatformPC>(memnew(EditorExportPlatformPC));
		exporter->set_binary_extension("");
		exporter->set_release_binary32("sailfish_sdl_32_release");
		exporter->set_debug_binary32("sailfish_sdl_32_debug");
		exporter->set_release_binary64("sailfish_sdl_arm_release");
		exporter->set_debug_binary64("sailfish_sdl_arm_debug");
		exporter->set_name("SailfishOS SDL");
		exporter->set_logo(logo);
		exporter->set_chmod_flags(0755);
		EditorImportExport::get_singleton()->add_export_platform(exporter);
	}
}