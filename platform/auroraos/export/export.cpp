/*************************************************************************/
/*  export.cpp                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Author: 2025 sashikknox <sashikkxno@gmail.com>                             */
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-present Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "export.h"

#include "core/variant/dictionary.h"
#include "core/io/marshalls.h"
#include "core/io/xml_parser.h"
#include "core/io/zip_io.h"
#include "drivers/unix/os_unix.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/config/project_settings.h"
#include "core/version.h"
#include "editor/export/editor_export.h"
#include "editor/themes/editor_scale.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_paths.h"
#include "main/main.h"
#include "modules/regex/regex.h"
#include "modules/svg/image_loader_svg.h"
#include "platform/auroraos/export/logo_svg.gen.h"
#include "scene/resources/texture.h"

#define prop_editor_sdk_path "export/auroraos/aurora_sdk/sdk_path"
#define prop_editor_tool "export/auroraos/aurora_sdk/tool"
#define prop_editor_ssh_tool_path "export/auroraos/aurora_sdk/ssh_tool_path"
#define prop_editor_ssh_port "export/auroraos/aurora_sdk/ssh_port"
#define prop_editor_binary_arm "export/auroraos/export_tempaltes/arm32"
#define prop_editor_binary_arm64 "export/auroraos/export_tempaltes/arm64"
#define prop_editor_binary_x86_64 "export/auroraos/export_tempaltes/x86_64"

#define prop_project_launcher_name "application/config/name"
#define prop_project_package_version "application/config/version"

#define prop_export_application_launcher_name "application/launcher_name"
#define prop_export_application_organization "application/organization"
#define prop_export_application_name "application/name"
#define prop_export_package_version "application/version"
#define prop_export_release_version "application/release_version"
#define prop_export_icon_config_path "icons/"
#define prop_export_icon_86 "icons/86x86"
#define prop_export_icon_108 "icons/108x108"
#define prop_export_icon_128 "icons/128x128"
#define prop_export_icon_172 "icons/172x172"
#define prop_export_sign_key "sign/key"
#define prop_export_sign_cert "sign/cert"
#define prop_export_sign_password "sign/password"

#define prop_export_binary_arch "architecture"
#define prop_export_binary_arch_armv7hl "architecture/armv7hl"
#define prop_export_binary_arch_aarch64 "architecture/aarch64"
#define prop_export_binary_arch_x86_64 "architecture/x86_64"
// firejail / sailjail properties
#define prop_sailjail_permissions "permissions/"
#define prop_validator_enable "rpm_validator/enable"

#define mersdk_rsa_key "/vmshare/ssh/private_keys/engine/mersdk"

// Add missing TTR macro
#ifndef TTR
#define TTR(m_text) (m_text)
#endif

const String sailjail_minimum_required_permissions[] = {
	 // required in any case
	// "Internet", // looks like its too required FIXME: debug godot and made it useable without internet permission  on device
	String()
};

const String sailjail_permissions[] = {
	"Audio",
	"Bluetooth",
	"Camera",
	"Internet", 
	"Location",
	"MediaIndexing",
	"Microphone",
	"NFC",
	"RemovableMedia",
	"UserDirs",
	"WebView",
	"Documents",
	"Downloads",
	"Music",
	"Pictures",
	"PublicDir",
	"Videos",
	"Compatibility",
	String()
};

// #ifdef WINDOWS_ENABLED
// const String separator("\\");
// #else
const String separator("/");
// #endif

const String spec_file_tempalte =
		"Name:       %{_gd_application_name}\n"
		"# >> macros\n"
		"%define __requires_exclude ^libfreetype\\.so.*|.*libxkbcommon\\.so.*$\n"
		"%define __provides_exclude_from ^%{_datadir}/%{name}/lib/.*\\.so.*$\n"
		"%define debug_package %{nil}\n"
		"# << macros\n"
		"Summary:    %{_gd_launcher_name}\n"
		"Version:    %{_gd_version}\n"
		"Release:    %{_gd_release}\n"
		"Group:      Game\n"
		"License:    Proprietary\n"
		"BuildArch:  %{_gd_architecture}\n"
		"BuildRequires: patchelf\n"
		"\n"
		"%define _topdir %{_gd_shared_path}%{_gd_export_path}\n"
		"\n"
		"%description\n"
		"%{_gd_description}\n"
		"\n"
		"%prep\n"
		"echo \"Nothing to do here. Skip this step\"\n"
		"\n"
		"%build\n"
		"echo \"Nothing to do here. Skip this step\"\n"
		"\n"
		"%install\n"
		"rm -rf %{buildroot}\n"
		"mkdir -p \"%{buildroot}\"\n"
		"mkdir -p \"%{buildroot}%{_bindir}\"\n"
		"rm -fr \"%{buildroot}%{_bindir}\"\n"
		"mv \"%{_topdir}/BUILD%{_bindir}\" \"%{buildroot}%{_bindir}\"\n"
		// "cp -r \"%{_topdir}/%{_datadir}/%{name}/\"* \"%{buildroot}%{_datadir}/%{name}/\"\n"
		"mv  \"%{_topdir}/BUILD%{_datadir}\" \"%{buildroot}%{_datadir}\"\n"
		"mkdir -p \"%{buildroot}/usr/share/applications\"\n"
		"[ -f \"%{_topdir}/BUILD/usr/share/applications/%{name}.desktop\" ] && mv -f \"%{_topdir}/BUILD/usr/share/applications/%{name}.desktop\" \"%{buildroot}/usr/share/applications/%{name}.desktop\"||echo \"File moved already\"\n"
		"chmod 755 %{buildroot}/usr/share/icons/hicolor/*\n"
		"chmod 755 %{buildroot}/usr/share/icons/hicolor/*/apps\n"
		"chmod -R 755 %{buildroot}%{_datadir}/%{name}\n"
		// TODO: add use patchelf
		"patchelf --force-rpath --set-rpath /usr/share/%{name}/lib %{buildroot}%{_bindir}/%{name}\n" 
		"# dependencies\n"
		"install -D %{_libdir}/libfreetype.so.* -t %{buildroot}%{_datadir}/%{name}/lib/\n"
		"install -D %{_libdir}/libxkbcommon.so.* -t %{buildroot}%{_datadir}/%{name}/lib/\n"
		//libudev.so.1 - not allowed dependency..
		"\n"
		"%files\n"
		"%defattr(644,root,root,-)\n"
		"%attr(755,root,root) %{_bindir}/%{name}\n"
		// "%attr(644,root,root) %{_datadir}/%{name}/%{name}.png\n" // FIXME: add icons for all resolutions, as AuroraOS specification needed
		"%{_datadir}/icons/hicolor/86x86/apps/%{name}.png\n"
		"%{_datadir}/icons/hicolor/108x108/apps/%{name}.png\n"
		"%{_datadir}/icons/hicolor/128x128/apps/%{name}.png\n"
		"%{_datadir}/icons/hicolor/172x172/apps/%{name}.png\n"
		"%attr(644,root,root) %{_datadir}/%{name}/%{name}.pck\n"
		"%{_datadir}/%{name}/lib\n"
		// "%attr(755,root,root) %{_datadir}/%{name}/lib/*\n" // FIXME: add this as optional string, if we use native extensions
		"%attr(644,root,root) %{_datadir}/applications/%{name}.desktop\n"
		"%changelog\n"
		"* %{_gd_date} Godot Game Engine\n"
		"- application %{name} packed to RPM\n"
		"#$changelog$";

// --define "_datadir  /home/nemo/.local/share" if need install to user folder
const String desktop_file_template =
		"[Desktop Entry]\n"
		"Type=Application\n"
		"X-Nemo-Application-Type=no-invoker\n"
		"Icon=%{name}\n"
		"Exec=%{name} --main-pack %{_datadir}/%{name}/%{name}.pck\n"
		"Name=%{_gd_launcher_name}\n"
		"Name[en]=%{_gd_launcher_name}\n"
		"Categories=Game";

const String desktop_file_sailjail =
		"\n"
		"[X-Application]\n"
		"Permissions=%{permissions}\n"
		"OrganizationName=%{organization}\n"
		"ApplicationName=%{appname}\n";

static void _execute_thread(void *p_ud) {
	EditorNode::ExecuteThreadArgs *eta = (EditorNode::ExecuteThreadArgs *)p_ud;
	// Error err = OS_Unix::execute(eta->path, eta->args, true, NULL, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex);
	Error err = OS::get_singleton()->execute(eta->path, eta->args, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex, false);
	// Error err = OS_Unix::execute(eta->path, eta->args, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex, false);
	print_verbose("Thread exit status: " + itos(eta->exitcode));
	if (err != OK) {
		eta->exitcode = err;
	}

	eta->done.set();
}

class EditorExportPlatformAuroraOS : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformAuroraOS, EditorExportPlatform)

	/** On Windows, the sfdk tool works worst
	 * with the @godot implementation of the
	 * execution command in windows port, it does
	 * not work at all.
	 * Ok, try use just ssh
	 */
	enum SDKConnectType {
		tool_sfdk,
		tool_ssh
	};

	enum TargetArch {
		arch_armv7hl,
		arch_aarch64,
		arch_x86_64,
		arch_MAX,
		arch_MIN = arch_armv7hl,
		arch_x86 = arch_x86_64,
		arch_unkown
	};

	struct Device {
		String address;
		String name;
		TargetArch arch;
	};

	struct MerTarget {
		MerTarget() {
			arch = arch_unkown;
			name = "AuroraOS";
			target_template = "";
			addversion = "";
			version[0] = 4;
			version[1] = 3;
			version[2] = 0;
			version[3] = 12;
		}

		String target_template;
		String name;
		String addversion; // in AuroraOS we have additions name "-base" after version
		int version[4]; // array of 4 integers
		TargetArch arch;
	};

	struct NativePackage {
		MerTarget target; // AuroraOS build target
		String name; // package rpm name (lowercase, without special symbols)
		String launcher_name; // button name in launcher menu
		String version; // game/application version
		String release; // build number/release version
		String description; // package desciption
	};

protected:
	String get_current_date() const {
		// TODO implement get date function
		return String("Thu Dec 19 2019");
	}

	String mertarget_to_text(const MerTarget &target) const {
		Array args;
		for (int i = 0; i < 4; i++)
			args.push_back(target.version[i]);
		bool error;
		String addversion = "-";
		if( !target.addversion.is_empty() )
			addversion = target.addversion + addversion;
		return target.name + String("-%d.%d.%d.%d").sprintf(args, &error) + addversion + arch_to_text(target.arch) + String(".default");
	}

	String arch_to_text(TargetArch arch) const {
		switch (arch) {
			case arch_armv7hl:
				return "armv7hl";
				break;
			case arch_aarch64:
				return "aarch64";
				break;
			case arch_x86_64:
				return "x86_64";
				break;
			default:
				print_error("Cant use this architecture");
				return "noarch";
				break;
		}
		return "noarch";
	}

	String get_sdk_config_path(const Ref<EditorExportPreset> &p_preset) const {
		String sdk_configs_path = OS::get_singleton()->get_config_path();
#ifdef OSX_ENABLED
		sdk_configs_path = OS::get_singleton()->get_environment("HOME") + String("/.config");
#elif WINDOWS_ENABLED
		sdk_configs_path = String(EDITOR_GET(prop_editor_sdk_path)) + separator + String("settings");
#endif
		sdk_configs_path += separator + sdk_config_dir;
		return sdk_configs_path;
	}

	String get_absolute_export_path(const String &realitive_export_path) const {
		String export_path = realitive_export_path;
		String project_path = ProjectSettings::get_singleton()->get_resource_path();

		// if (project_path.rfind(separator) == project_path.length() - 1)
		if (project_path.right(1) == separator)
			project_path = project_path.left(project_path.length() - 1);
			// project_path = project_path.left(project_path.rfind(separator));
		// make from realitive path an absolute path
		if (export_path.find(String(".") + separator) == 0) {
			export_path = project_path + separator + export_path.substr(2, export_path.length() - 2);
		} else {
			int count_out_dir = 0;
			while (export_path.find(String("..") + separator) == 0) {
				count_out_dir++;
				export_path = export_path.substr(3, export_path.length() - 3);
			}
			for (int i = 0; i < count_out_dir; i++) {
				int pos = project_path.rfind(separator);
				if (pos >= 0) {
					project_path = project_path.left(pos);
				}
			}
			export_path = project_path + separator + export_path;
		}
		return export_path;
	}

	String get_sfdk_path(const Ref<EditorExportPreset> &p_preset) const {
		String sfdk_path = String(EDITOR_GET(prop_editor_sdk_path));
#ifdef WINDOWS_ENABLED
		sfdk_path += String("\\bin\\sfdk.exe");
#else
		sfdk_path += String("/bin/sfdk");
#endif
		return sfdk_path;
	}

	int execute_task(const String &p_path, const List<String> &p_arguments, List<String> &r_output) {
		EditorNode::ExecuteThreadArgs eta;
		eta.path = p_path;
		eta.args = p_arguments;
		eta.exitcode = 255;
		eta.done.set_to(false);

		int prev_len = 0;
		eta.execute_output_thread.start(_execute_thread, &eta);

		while (!eta.done.is_set()) {
			eta.execute_output_mutex.lock();
			if (prev_len != eta.output.length()) {
				String to_add = eta.output.substr(prev_len, eta.output.length());
				prev_len = eta.output.length();
				r_output.push_back(to_add);
				//print_verbose(to_add);
				Main::iteration();
			}
			eta.execute_output_mutex.unlock();
			OS::get_singleton()->delay_usec(1000);
		}

		eta.execute_output_thread.wait_to_finish();

		return eta.exitcode;
	}

	Error build_package(const NativePackage &package, const Ref<EditorExportPreset> &p_preset, const bool &p_debug, const String &sfdk_tool, EditorProgress &ep, int progress_from, int progress_full) {
		int steps = 9; // if add some step to build process, need change it
		const bool validate_rpm = p_preset->get(prop_validator_enable);
		const bool sign_rpm = true; //p_preset->get(prop_aurora_sign_enable);
		const bool sailjail_enabled = true;
		int current_step = 0;
		int progress_step = progress_full / steps;

		if (validate_rpm) {
			steps++; //another step to validate PRM
		}

		if (sign_rpm) {
			steps++; //another step to sign PRM
		}

		// Check if template file exists and is readable
		if (!FileAccess::exists(package.target.target_template)) {
			print_error("AuroraOS Export: Template file does not exist: " + package.target.target_template);
			return ERR_FILE_NOT_FOUND;
		}

		SDKConnectType sdk_tool = SDKConnectType::tool_sfdk;
		String tool = EDITOR_GET(prop_editor_tool);

		if (tool == String("ssh"))
			sdk_tool = SDKConnectType::tool_ssh;

		List<String> args;
		List<String> pre_args;
		String export_template;

		if (sdk_tool == SDKConnectType::tool_ssh) {
			// here we neet to know where is RSA keys for buildengine
			String rsa_key_path = EDITOR_GET(prop_editor_sdk_path);
			rsa_key_path += String(mersdk_rsa_key);
			String ssh_port = EDITOR_GET(prop_editor_ssh_port);
			pre_args.push_back("-o");
			pre_args.push_back("\"IdentitiesOnly=yes\"");
			pre_args.push_back("-i");
			pre_args.push_back(String("\"") + rsa_key_path + String("\""));
			pre_args.push_back("-p");
			pre_args.push_back(ssh_port); // default is 2222 port
			pre_args.push_back("mersdk@localhost");
		} else { // SFDK tool
			pre_args.push_back("engine");
			pre_args.push_back("exec");
		}

		switch (package.target.arch) {
			case arch_armv7hl:
				export_template = EDITOR_GET(prop_editor_binary_arm);
				break;
			case arch_aarch64:
				export_template = EDITOR_GET(prop_editor_binary_arm64);
				break;
			case arch_x86:
				export_template = EDITOR_GET(prop_editor_binary_x86_64);
				break;
			default:
				print_error("Wrong architecture of package");
				return ERR_CANT_CREATE;
		}

		if (export_template.is_empty()) {
			print_error(String("No ") + arch_to_text(package.target.arch) + String(" template setuped"));
			return ERR_CANT_CREATE;
		}

		// Validate template architecture compatibility
		print_verbose("AuroraOS Export: Validating template architecture...");
		Error template_check = validate_template_architecture(export_template, package.target.arch);
		if (template_check != OK) {
			print_error("AuroraOS Export: Template architecture validation failed");
			return template_check;
		}

		String target_string = mertarget_to_text(package.target);
		String export_path = get_absolute_export_path(p_preset->get_export_path());
		String broot_path = export_path + String("_buildroot");
		String build_folder = separator + ("BUILD") + separator;
		String rpm_prefix_path = broot_path.left(broot_path.rfind(separator));
		String export_path_part;
		String sdk_shared_path;
		String rpm_dir_path = broot_path + separator + String("rpm");
		String pck_path = broot_path + separator + package.name + String(".pck");
		String template_path = broot_path + separator;
		// String spec_file_path = rpm_dir_path + separator+ package.name + String(".spec");
		String spec_file_path = broot_path + separator + String("SPECS") + separator + package.name + String(".spec");
		String desktop_file_path = broot_path + build_folder + String("/usr/share/applications/").replace("/", separator) + package.name + String(".desktop");
		String package_prefix = "/usr"; //p_preset->get(prop_package_prefix);
		String data_dir = package_prefix + String("/share");
		String bin_dir = package_prefix + String("/bin");
#ifdef WINDOWS_ENABLED
		String data_dir_native = data_dir.replace("/", "\\");
		String bin_dir_native = bin_dir.replace("/", "\\");
#else
		String &data_dir_native = data_dir;
		String &bin_dir_native = bin_dir;
#endif
		// String sailjail_organization_name = GLOBAL_GET(prop_project_organization);
		String sailjail_organization_name = p_preset->get(prop_export_application_organization);

		Error err;
		bool is_export_path_exits_in_sdk = false;
		{
			// check avaliable folders in build engine ( from some AuroraSDK version,
			// in buildengine has no /home/metsdk/share folder, just /home/username
			// folder (in unix systems, with docker) )
			// that mean, expart path in build engine should be same as on host system
			// List<String> args;
			args.clear();
			String pipe;
			int exitcode = 0;
			for (List<String>::Element *a = pre_args.front(); a != nullptr; a = a->next()) {
				args.push_back(a->get());
			}
			args.push_back("bash");
			args.push_back("-c");
			if (export_path.find(shared_home) == 0)
				args.push_back(String("if [ -d \"") + shared_home + String("\" ]; then echo returncode_true; exit 0; else echo returncode_false; exit 1; fi"));
			else if (export_path.find(shared_src) == 0)
				args.push_back(String("if [ -d \"") + shared_src + String("\" ]; then echo returncode_true; exit 0; else echo returncode_false; exit 1; fi"));
			else
				args.push_back(String("if [ -d \"") + export_path + String("\" ]; then echo returncode_true; exit 0; else echo returncode_false; exit 1; fi"));

			// err = OS::get_singleton()->execute(sfdk_tool, args, true, nullptr, &pipe, &exitcode, false /*read stderr*/);
			err = OS::get_singleton()->execute(sfdk_tool, args, &pipe, &exitcode, false);

			if (err == OK) {
				if (exitcode == 0 || String("returncode_true") == pipe) {
					print_line("Use export path without any changes, its same in host and in build engine");
					is_export_path_exits_in_sdk = true;
				}
			} else {
				print_line(pipe);
			}
		}
		if (!shared_home.is_empty() && export_path.find(shared_home) == 0) {
			// print_verbose(String("sfdk") + result_string);
			// String execute_binary = sfdk_tool;
			export_path_part = export_path.substr(shared_home.length(), export_path.length() - shared_home.length()).replace(separator, "/") + String("_buildroot");
			if (is_export_path_exits_in_sdk)
				sdk_shared_path = shared_home;
			else
				sdk_shared_path = String("/home/mersdk/share");
		} else if (!shared_src.is_empty() && export_path.find(shared_src) == 0) {
			export_path_part = export_path.substr(shared_src.length(), export_path.length() - shared_src.length()).replace(separator, "/") + String("_buildroot");
			if (is_export_path_exits_in_sdk)
				sdk_shared_path = shared_src;
			else
				sdk_shared_path = String("/home/src1");
		} else {
			print_error(String("Export path outside of SharedHome and SharedSrc:\nSharedHome: ") + shared_home + String("\nSharedSrc: ") + shared_src);
			return ERR_CANT_CREATE;
		}

		Ref<DirAccess> broot = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		const String directories[] = {
			separator + String("SPECS"),
			build_folder + data_dir_native + separator + package.name + separator + String("lib"),
			build_folder + bin_dir_native,
			build_folder + String("/usr/share/applications/").replace("/", separator),
			build_folder + String("/usr/share/icons/hicolor/86x86/").replace("/", separator) + String("apps"),
			build_folder + String("/usr/share/icons/hicolor/108x108/").replace("/", separator) + String("apps"),
			build_folder + String("/usr/share/icons/hicolor/128x128/").replace("/", separator) + String("apps"),
			build_folder + String("/usr/share/icons/hicolor/172x172/").replace("/", separator) + String("apps"),
			String("")
		};

		int path_num = 0;
		while (directories[path_num].length() > 0) {
			err = broot->make_dir_recursive(broot_path + directories[path_num]);
			if (err != Error::OK) {
				print_error(String("Cant create directory: ") + broot_path + directories[path_num]);
				return ERR_CANT_CREATE;
			}
			path_num++;
		}

		ep.step("copy export tempalte to buildroot", progress_from + (++current_step) * progress_step);
		String binary_result_path = broot_path + build_folder + bin_dir_native + separator + package.name;
		if (broot->copy(export_template, binary_result_path) != Error::OK) {
			print_verbose(String("Template path: ") + export_template);
			print_error(String("Cant copy ") + arch_to_text(package.target.arch) + String(" template binary to: ") + binary_result_path);
			return ERR_CANT_CREATE;
		}

		ep.step(String("create name.pck file.").replace("name", package.name), progress_from + (++current_step) * progress_step);
		pck_path = broot_path + build_folder + data_dir_native + separator + package.name + separator + package.name + String(".pck");
		
		// Modify GDExtension files for AuroraOS before packing
		Error gdext_err = modify_gdextension_files(package.name, p_preset, p_debug);
		if (gdext_err != OK) {
			print_error("AuroraOS Export: Failed to modify GDExtension files");
			return gdext_err;
		}
		
		// use custom data path
		bool use_custom_dir = ProjectSettings::get_singleton()->get("application/config/use_custom_user_dir");
		String custom_path = String(ProjectSettings::get_singleton()->get("application/config/custom_user_dir_name"));
		ProjectSettings::get_singleton()->set("application/config/use_custom_user_dir",true);
		ProjectSettings::get_singleton()->set(
				"application/config/custom_user_dir_name", 
				package.name
					.replace("harbour-", sailjail_organization_name + String("/"))
					.replace(sailjail_organization_name + String("."), sailjail_organization_name + String("/")) 
			);
		err = export_pack(p_preset, p_debug, pck_path);
		
		// Restore GDExtension files from backup
		Error restore_err = restore_gdextension_files(p_preset);
		if (restore_err != OK) {
			print_error("AuroraOS Export: Failed to restore GDExtension files from backup");
		}
		
		ProjectSettings::get_singleton()->set("application/config/use_custom_user_dir",use_custom_dir);
		ProjectSettings::get_singleton()->set("application/config/custom_user_dir_name", custom_path);

		if (err != Error::OK) {
			print_error(String("Cant create *.pck: ") + pck_path);
			return err;
		}

		ep.step(String("generate ") + package.name + String(".spec file"), progress_from + (++current_step) * progress_step);
		{
			Ref<FileAccess> spec_file = FileAccess::open(spec_file_path, FileAccess::WRITE, &err);
			if (err != Error::OK) {
				print_error(String("Cant create *.spec: ") + spec_file_path);
				return ERR_CANT_CREATE;
			}
			String spec_text = spec_file_tempalte.replace("%{_gd_application_name}", package.name);
			spec_text = spec_text.replace("%{_gd_launcher_name}", package.launcher_name);
			spec_text = spec_text.replace("%{_gd_version}", package.version);
			spec_text = spec_text.replace("%{_gd_release}", package.release);
			spec_text = spec_text.replace("%{_gd_architecture}", arch_to_text(package.target.arch));
			spec_text = spec_text.replace("%{_gd_description}", package.description);
			spec_text = spec_text.replace("%{_gd_shared_path}", sdk_shared_path);
			spec_text = spec_text.replace("%{_gd_export_path}", export_path_part);
			spec_text = spec_text.replace("%{_gd_date}", get_current_date());
			spec_text = spec_text.replace("%{_datadir}", data_dir);
			spec_text = spec_text.replace("%{_bindir}", bin_dir);
			// spec_text = spec_text.replace("%{_set_topdir}", sdk_shared_path + export_path_part);

			spec_file->store_string(spec_text);
			spec_file->flush();
			spec_file->close();
		}

		ep.step(String("generate ") + package.name + String(".desktop file"), progress_from + (++current_step) * progress_step);
		{
			//desktop_file_path = broot_path + desktop_file_path;
			Ref<FileAccess> desktop_file = FileAccess::open(desktop_file_path, FileAccess::WRITE, &err);
			if (err != Error::OK) {
				print_error(String("Cant create *.desktop: ") + desktop_file_path);
				return ERR_CANT_CREATE;
			}
			String file_text = desktop_file_template.replace("%{_gd_launcher_name}", package.launcher_name);
			file_text = file_text.replace("%{name}", package.name);
			file_text = file_text.replace("%{_datadir}", data_dir);
			file_text = file_text.replace("%{_bindir}", bin_dir);
			if (sailjail_enabled) {
				file_text = file_text + desktop_file_sailjail.replace("%{name}", package.name);
				file_text = file_text.replace("%{organization}", sailjail_organization_name);
				file_text = file_text.replace("%{appname}", package.name.replace("harbour-","").replace(sailjail_organization_name + String("."),""));
				// collect sailjail permissions
				int permission_num = 0;
				String permissions;
				// minimum required permission
				while (!sailjail_minimum_required_permissions[permission_num].is_empty()) {
					if (permissions.is_empty())
						permissions = sailjail_minimum_required_permissions[permission_num];
					else
						permissions = permissions + String(";") + sailjail_minimum_required_permissions[permission_num];
					permission_num++;
				}
				// all other permissions
				permission_num = 0;
				while (!sailjail_permissions[permission_num].is_empty()) {
					if (!p_preset->get(String(prop_sailjail_permissions) + sailjail_permissions[permission_num++]))
						continue;
					if (permissions.is_empty())
						permissions = sailjail_permissions[permission_num - 1];
					else
						permissions = permissions + String(";") + sailjail_permissions[permission_num - 1];
				}
				file_text = file_text.replace("%{permissions}", permissions);
			}
			desktop_file->store_string(file_text);
			desktop_file->flush();
			desktop_file->close();
		}

		ep.step(String("copy project icons"), progress_from + (++current_step) * progress_step);
		int icon_number = 0;
		const String icons[] = {
			String(p_preset->get(prop_export_icon_86)), "86x86",
			String(p_preset->get(prop_export_icon_108)), "108x108",
			String(p_preset->get(prop_export_icon_128)), "128x128",
			String(p_preset->get(prop_export_icon_172)), "172x172",
			// String(GLOBAL_GET(prop_project_icon_86)), "86x86",
			// String(GLOBAL_GET(prop_project_icon_108)), "108x108",
			// String(GLOBAL_GET(prop_project_icon_128)), "128x128",
			// String(GLOBAL_GET(prop_project_icon_172)), "172x172",
			String()
		};

		while (icons[icon_number].length() > 0) {
			String icon = icons[icon_number];
			// //usr/share/icons/hicolor/128x128/apps/
			String icon_file_path = broot_path + build_folder + String("/usr/share/icons/hicolor/").replace("/", separator) + icons[icon_number + 1] + separator + String("apps") + separator + package.name + String(".png");
			if (broot->copy(icon, icon_file_path) != Error::OK) {
				print_error(String("Cant copy icon file \"") + icon + String("\" to \"") + icon_file_path + String("\""));
				return ERR_CANT_CREATE;
			}
			icon_number += 2;
		}

		// Copy GDExtension libraries
		ep.step(String("copy GDExtension libraries"), progress_from + (++current_step) * progress_step);
		
		// Add diagnostic logging before copying libraries
		log_gdextension_diagnostics(package.name, p_preset);
		
		err = copy_gdextension_libraries(broot_path, build_folder, data_dir_native, package.name, p_preset, p_debug);
		if (err != Error::OK) {
			print_error(String("Failed to copy GDExtension libraries"));
			return err;
		}

		ep.step(String("setup SDK tool for ") + arch_to_text(package.target.arch) + String(" package build"), progress_from + (++current_step) * progress_step);
		{
			String build_script;
			String buid_script_path = broot_path + separator + String("buildscript.sh");
			{ //
				Ref<FileAccess> script_file = FileAccess::open(buid_script_path, FileAccess::WRITE, &err);
				if (err != Error::OK) {
					print_error(String("Cant create : ") + buid_script_path);
					return ERR_CANT_CREATE;
				}
				String file_text;
				file_text += String("rpmbuild ");
				file_text += String("--define ");
				file_text += String(String("'_topdir ") + sdk_shared_path + export_path_part + String("'"));
				file_text += String(" -ba ");
				file_text += String("--without debug --without debuginfo ");
				file_text += String("\"") + sdk_shared_path + export_path_part + String("/SPECS/") + package.name + String(".spec\"");
				build_script = file_text;
				script_file->store_string(file_text);
				script_file->flush();
				script_file->close();
			}

			// try use shell -------------------------------

			// ---------------------------------------------
			String result_string;
			for (List<String>::Element *a = args.front(); a != nullptr; a = a->next()) {
				result_string += String(" ") + a->get();
			}
			print_verbose(String("sfdk") + result_string);
			String execute_binary = sfdk_tool;
			String long_parameter;

			// install deps
			args.clear();
			for (List<String>::Element *a = pre_args.front(); a != nullptr; a = a->next()) {
				args.push_back(a->get());
			}
			args.push_back("sb2");
			args.push_back("-t");
			args.push_back(target_string);
			args.push_back("-m");
			args.push_back("sdk-install");
			args.push_back("-R");
			args.push_back("zypper");
			args.push_back("in");
			args.push_back("-y");
			args.push_back("patchelf");

			int result = EditorNode::get_singleton()->execute_and_show_output(TTR("Install dependencies for ") + target_string, execute_binary, args, true, false);
			if (result != 0) {
				return ERR_CANT_CREATE;
			}

			args.clear();
			for (List<String>::Element *a = pre_args.front(); a != nullptr; a = a->next()) {
				args.push_back(a->get());
			}

			if (sdk_tool == SDKConnectType::tool_ssh) {
				buid_script_path = sdk_shared_path + export_path_part + separator + String("buildscript.sh");
			}

			args.push_back("sb2");
			args.push_back("-t");
			args.push_back(target_string);
			args.push_back("bash");
			args.push_back("-c");
			args.push_back(build_script);
			// args.push_back(buid_script_path);

			EditorNode::get_singleton()->execute_and_show_output(TTR("Build rpm package for ") + target_string, execute_binary, args, true, false);
			if (result != 0) {
				return ERR_CANT_CREATE;
			}
			print_verbose("Move result RPMS to outside folder");
			{
				String build_result_dir = broot_path + separator + String("RPMS") + separator + arch_to_text(package.target.arch);
				Ref<DirAccess> move_result = DirAccess::create_for_path(build_result_dir);
				String result_rpm_name = package.name + String("-") + package.version + String("-") + package.release + String(".") + arch_to_text(package.target.arch) + String(".rpm");
				if (move_result.is_null())
					return ERR_CANT_CREATE;
				if (move_result->exists(rpm_prefix_path + separator + result_rpm_name))
					move_result->remove(rpm_prefix_path + separator + result_rpm_name);
				err = move_result->rename(build_result_dir + separator + result_rpm_name, rpm_prefix_path + separator + result_rpm_name);

				if (err != Error::OK) {
					print_error(TTR("Cant move result of build, please check it by yourself: ") + broot_path + separator + String("RPMS"));
					return ERR_CANT_CREATE;
				}

				if (sign_rpm) {
					String key_path = p_preset->get(prop_export_sign_key);
					String cert_path = p_preset->get(prop_export_sign_cert);
					String cert_password = p_preset->get(prop_export_sign_password);
					ep.step(String("Signing RPM file: ") + result_rpm_name, progress_from + (++current_step) * progress_step);
					String rpm_path = rpm_prefix_path + separator + result_rpm_name;

					args.clear();
					for(List<String>::Element *a = pre_args.front(); a != nullptr; a = a->next()) {
						args.push_back( a->get() );
					}
					args.push_back("sb2");
					args.push_back("-t");
					args.push_back(target_string);
					if (cert_password.is_empty())
						cert_password = "nopassword"; // to prevent wait stdin if password not set by user
					args.push_back("env");
					args.push_back(String("KEY_PASSPHRASE=") + cert_password);
					args.push_back("rpmsign-external");
					args.push_back("sign");
					args.push_back("--key");
					args.push_back(key_path);
					args.push_back("--cert");
					args.push_back(cert_path);
					args.push_back(rpm_path);

					result = EditorNode::get_singleton()->execute_and_show_output(TTR("Signing RPM package ") + result_rpm_name, execute_binary, args, true, false);
					if (result != 0) {
						return ERR_CANT_CREATE;
					}
				}

				if (validate_rpm) {
					if (sdk_tool == SDKConnectType::tool_sfdk) {
						ep.step(String("Validate RPM file: ") + result_rpm_name, progress_from + (++current_step) * progress_step);
						String rpm_path = rpm_prefix_path + separator + result_rpm_name;

						args.clear();
						args.push_back("-c");
						args.push_back(String("target=") + target_string.replace(".default", ""));
						args.push_back("check");
						args.push_back(rpm_path);

						result = EditorNode::get_singleton()->execute_and_show_output(TTR("Validate RPM package ") + result_rpm_name, execute_binary, args, true, false);
						if (result != 0) {
							return ERR_CANT_CREATE;
						}
					} else {
						ep.step(String("Cant Validate RPM file: ") + result_rpm_name, progress_from + (++current_step) * progress_step);
					}
				}
			}
		}
		ep.step(String("remove temp directory"), progress_from + (++current_step) * progress_step);
		{
			Ref<DirAccess> rmdir = DirAccess::open(broot_path, &err);
			if (err != Error::OK) {
				print_error("cant open dir");
			}
			rmdir->erase_contents_recursive(); //rmdir->remove(<#String p_name#>)
			rmdir->remove(broot_path);
		}

		ep.step(String("build ") + arch_to_text(package.target.arch) + String(" target success"), progress_from + (++current_step) * progress_step);
		return Error::OK;
	}

	// Copy GDExtension libraries to buildroot
	Error copy_gdextension_libraries(const String &broot_path, const String &build_folder, const String &data_dir_native, const String &package_name, const Ref<EditorExportPreset> &p_preset, bool p_debug) {
		Vector<SharedObject> so_files;
		print_verbose("AuroraOS Export: Copying GDExtension libraries");
		
		// Use save_pack to collect shared objects without writing to file
		print_verbose("AuroraOS Export: Collecting GDExtension libraries via save_pack");
		
		// Create a temporary file path for the pack (we won't actually use it)
		String temp_pack_path = EditorPaths::get_singleton()->get_temp_dir().path_join("temp_collect_libs.pck");
		
		// Call save_pack to collect shared objects
		Error err = save_pack(p_preset, p_debug, temp_pack_path, &so_files, nullptr, nullptr, false);
		
		// Clean up the temporary file if it was created
		if (FileAccess::exists(temp_pack_path)) {
			DirAccess::remove_file_or_error(temp_pack_path);
		}
		
		if (err != OK) {
			print_verbose("AuroraOS Export: Could not collect GDExtension libraries via save_pack, continuing without them");
			return OK; // Continue without libraries - they're optional
		}
		
		if (so_files.size() > 0) {
			print_verbose("AuroraOS Export: Found " + String::num_int64(so_files.size()) + " GDExtension libraries");
			
			// Copy each library to the lib directory
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			for (const SharedObject &so : so_files) {
				print_verbose("AuroraOS Export: Copying GDExtension library: " + so.path);
				String source_path = so.path;
				String lib_name = source_path.get_file();
				String dest_path = broot_path + build_folder + data_dir_native + separator + package_name + separator + String("lib") + separator + lib_name;
				
				print_verbose("AuroraOS Export: Copying GDExtension library: " + source_path + " -> " + dest_path);
				
				if (da->copy(source_path, dest_path) != Error::OK) {
					print_error("AuroraOS Export: Failed to copy GDExtension library: " + source_path);
					// Continue with other libraries even if one fails
				}
			}
		} else {
			print_verbose("AuroraOS Export: No GDExtension libraries found");
		}
		
		return OK;
	}

	// Modify GDExtension files to use correct paths for AuroraOS
	Error modify_gdextension_files(const String &package_name, const Ref<EditorExportPreset> &p_preset, bool p_debug) {
		print_verbose("AuroraOS Export: Modifying GDExtension files for AuroraOS paths");
		
		// Get project resource path
		String project_path = ProjectSettings::get_singleton()->get_resource_path();
		
		// Find all .gdextension files in the project
		Vector<String> gdextension_files;
		find_gdextension_files(project_path, gdextension_files);
		
		if (gdextension_files.size() == 0) {
			print_verbose("AuroraOS Export: No GDExtension files found");
			return OK;
		}
		
		print_verbose("AuroraOS Export: Found " + String::num_int64(gdextension_files.size()) + " GDExtension files");
		
		// Backup and modify each .gdextension file
		for (const String &gdext_path : gdextension_files) {
			print_verbose("AuroraOS Export: Processing GDExtension file: " + gdext_path);
			
			// Create backup
			String backup_path = gdext_path + ".auroraos_backup";
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			if (da->copy(gdext_path, backup_path) != OK) {
				print_error("AuroraOS Export: Failed to backup GDExtension file: " + gdext_path);
				continue;
			}
			
			// Read original file
			Ref<FileAccess> file = FileAccess::open(gdext_path, FileAccess::READ);
			if (!file.is_valid()) {
				print_error("AuroraOS Export: Failed to read GDExtension file: " + gdext_path);
				continue;
			}
			
			String content = file->get_as_text();
			file->close();
			
			// Modify library paths for AuroraOS
			String modified_content = modify_gdextension_content(content, package_name, p_preset, p_debug);
			
			// Write modified file
			file = FileAccess::open(gdext_path, FileAccess::WRITE);
			if (!file.is_valid()) {
				print_error("AuroraOS Export: Failed to write modified GDExtension file: " + gdext_path);
				continue;
			}
			
			file->store_string(modified_content);
			file->close();
			
			print_verbose("AuroraOS Export: Modified GDExtension file: " + gdext_path);
		}
		
		return OK;
	}
	
	// Restore GDExtension files from backup
	Error restore_gdextension_files(const Ref<EditorExportPreset> &p_preset) {
		print_verbose("AuroraOS Export: Restoring GDExtension files from backup");
		
		// Get project resource path
		String project_path = ProjectSettings::get_singleton()->get_resource_path();
		
		// Find all .gdextension files in the project
		Vector<String> gdextension_files;
		find_gdextension_files(project_path, gdextension_files);
		
		// Restore each .gdextension file from backup
		for (const String &gdext_path : gdextension_files) {
			String backup_path = gdext_path + ".auroraos_backup";
			
			if (FileAccess::exists(backup_path)) {
				print_verbose("AuroraOS Export: Restoring GDExtension file: " + gdext_path);
				
				Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
				if (da->copy(backup_path, gdext_path) != OK) {
					print_error("AuroraOS Export: Failed to restore GDExtension file: " + gdext_path);
					continue;
				}
				
				// Remove backup file
				da->remove(backup_path);
			}
		}
		
		return OK;
	}
	
	// Find all .gdextension files in project
	void find_gdextension_files(const String &path, Vector<String> &gdextension_files) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->change_dir(path);
		da->list_dir_begin();
		
		String file_name = da->get_next();
		while (file_name != "") {
			String full_path = path + "/" + file_name;
			
			if (da->current_is_dir() && file_name != "." && file_name != "..") {
				find_gdextension_files(full_path, gdextension_files);
			} else if (file_name.ends_with(".gdextension")) {
				gdextension_files.push_back(full_path);
			}
			
			file_name = da->get_next();
		}
		
		da->list_dir_end();
	}
	
	// Modify GDExtension content to use AuroraOS paths
	String modify_gdextension_content(const String &content, const String &package_name, const Ref<EditorExportPreset> &p_preset, bool p_debug) {
		Vector<String> lines = content.split("\n");
		String modified_content;
		bool in_libraries_section = false;
		
		for (int i = 0; i < lines.size(); i++) {
			String line = lines[i];
			String trimmed_line = line.strip_edges();
			
			// Check if we're entering libraries section
			if (trimmed_line == "[libraries]") {
				in_libraries_section = true;
				modified_content += line + "\n";
				continue;
			}
			
			// Check if we're leaving libraries section
			if (in_libraries_section && trimmed_line.begins_with("[") && trimmed_line.ends_with("]") && trimmed_line != "[libraries]") {
				in_libraries_section = false;
			}
			
			// Modify library paths if we're in libraries section
			if (in_libraries_section && trimmed_line.contains("=")) {
				String key = trimmed_line.get_slice("=", 0).strip_edges();
				String value = trimmed_line.get_slice("=", 1).strip_edges();
				
				// Remove quotes from value if present
				if (value.begins_with("\"") && value.ends_with("\"")) {
					value = value.substr(1, value.length() - 2);
				}
				
				// Check if this is a target we should modify
				bool should_modify = false;
				
				// Check for AuroraOS-specific targets
				if (key.contains("auroraos.")) {
					// Only modify if the debug/release type matches
					if (p_debug && key.contains("debug")) {
						should_modify = true;
					} else if (!p_debug && key.contains("release")) {
						should_modify = true;
					}
				}

				if (should_modify) {
					// Extract library filename from path
					String lib_name = value.get_file();
					// Replace with AuroraOS path
					String new_value = String("/usr/share/") + package_name + String("/lib/") + lib_name;
					modified_content += key + " = \"" + new_value + "\"\n";
					print_verbose("AuroraOS Export: Modified library path: " + value + " -> " + new_value);
				} else {
					modified_content += line + "\n";
				}
			} else {
				modified_content += line + "\n";
			}
		}
		
		return modified_content;
	}

	// Validate template architecture compatibility
	Error validate_template_architecture(const String &template_path, TargetArch expected_arch) {
		print_verbose("AuroraOS Export: Validating template architecture for: " + template_path);
		
		// Check if file exists
		if (!FileAccess::exists(template_path)) {
			print_error("AuroraOS Export: Template file does not exist: " + template_path);
			return ERR_FILE_NOT_FOUND;
		}
		
		// Try to run 'file' command to check architecture
		List<String> args;
		args.push_back(template_path);
		
		String output;
		int exit_code;
		Error err = OS::get_singleton()->execute("file", args, &output, &exit_code, true);
		
		if (err == OK && exit_code == 0) {
			print_verbose("AuroraOS Export: File command output: " + output);
			
			// Check architecture in output
			String arch_expected = arch_to_text(expected_arch);
			bool arch_match = false;
			
			if (expected_arch == arch_armv7hl) {
				arch_match = output.contains("ARM") && (output.contains("32-bit") || output.contains("armv7"));
			} else if (expected_arch == arch_aarch64) {
				arch_match = output.contains("ARM") && (output.contains("64-bit") || output.contains("aarch64"));
			} else if (expected_arch == arch_x86_64) {
				arch_match = output.contains("x86-64") || output.contains("x86_64");
			}
			
			if (!arch_match) {
				print_error("AuroraOS Export: Template architecture mismatch!");
				print_error("AuroraOS Export: Expected: " + arch_expected);
				print_error("AuroraOS Export: Template file info: " + output);
				return ERR_INVALID_DATA;
			} else {
				print_verbose("AuroraOS Export: Template architecture validated successfully");
			}
		} else {
			print_verbose("AuroraOS Export: Could not run 'file' command to check architecture (this is not critical)");
		}
		
		return OK;
	}

	// Add diagnostic logging for GDExtension libraries
	void log_gdextension_diagnostics(const String &package_name, const Ref<EditorExportPreset> &p_preset) {
		print_verbose("AuroraOS Export: === GDExtension Diagnostics ===");
		
		// Get project resource path
		String project_path = ProjectSettings::get_singleton()->get_resource_path();
		
		// Find all .gdextension files
		Vector<String> gdextension_files;
		find_gdextension_files(project_path, gdextension_files);
		
		print_verbose("AuroraOS Export: Found " + String::num_int64(gdextension_files.size()) + " GDExtension files");
		
		for (const String &gdext_path : gdextension_files) {
			print_verbose("AuroraOS Export: Processing: " + gdext_path);
			
			// Read and analyze the file
			Ref<FileAccess> file = FileAccess::open(gdext_path, FileAccess::READ);
			if (file.is_valid()) {
				String content = file->get_as_text();
				file->close();
				
				// Check for architecture-specific entries
				Vector<String> lines = content.split("\n");
				bool in_libraries_section = false;
				
				for (const String &line : lines) {
					String trimmed = line.strip_edges();
					
					if (trimmed == "[libraries]") {
						in_libraries_section = true;
						continue;
					}
					
					if (in_libraries_section && trimmed.begins_with("[") && trimmed.ends_with("]")) {
						in_libraries_section = false;
					}
					
					if (in_libraries_section && trimmed.contains("=")) {
						String key = trimmed.get_slice("=", 0).strip_edges();
						String value = trimmed.get_slice("=", 1).strip_edges();
						
						if (key.contains("armv7hl") || key.contains("aarch64") || key.contains("x86_64") || 
							key.contains("auroraos") || key.contains("linux")) {
							print_verbose("AuroraOS Export: Found library entry: " + key + " = " + value);
						}
					}
				}
			}
		}
	}

public:
	EditorExportPlatformAuroraOS() {
		// Ref<Image> img = memnew(Image(_auroraos_logo));
		// logo.instance();
		// logo->create_from_image(img);
	}

	void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override {
		r_features->push_back("etc2");
		r_features->push_back("astc");
		
		// Add architecture features based on enabled architectures
		if (p_preset->get(prop_export_binary_arch_armv7hl).operator bool()) {
			r_features->push_back("arm32");
			r_features->push_back("armv7hl");
		}
		if (p_preset->get(prop_export_binary_arch_aarch64).operator bool()) {
			r_features->push_back("arm64");
			r_features->push_back("aarch64");
		}
		if (p_preset->get(prop_export_binary_arch_x86_64).operator bool()) {
			r_features->push_back("x86_64");
		}
	}

	virtual void get_platform_features(List<String> *r_features) const override {
		r_features->push_back("mobile");
		r_features->push_back(get_os_name().to_lower());
	}

	String get_os_name() const override {
		return "AuroraOS";
	}

	String get_name() const override {
		return "AuroraOS";
	}

	void set_logo(Ref<Texture> logo) {
		this->logo = logo;
	}

	Ref<Texture2D> get_logo() const override {
		return logo;
	}

	void get_export_options(List<ExportOption> *r_options) const override {
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_sdk_path, PROPERTY_HINT_GLOBAL_DIR));
		bool global_valid = false;
		String global_sdk_path = EditorSettings::get_singleton()->get(prop_editor_sdk_path, &global_valid);
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_auroraos_sdk_path, PROPERTY_HINT_GLOBAL_DIR), (global_valid) ? global_sdk_path : ""));

		// r_options->push_back(ExportOption(PropertyInfo(Variant::INT, prop_export_binary_arch, PROPERTY_HINT_ENUM, "arm32:0,arm64:1,x86_64:2"), TargetArch::arch_armv7hl));
		for (int  i = TargetArch::arch_MIN; i < TargetArch::arch_MAX; i++) {
			r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, String(prop_export_binary_arch) + String("/") + arch_to_text((TargetArch)i)), false));
		}

		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_application_launcher_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use name from project application/config/name"), GLOBAL_GET(prop_project_launcher_name)));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_application_organization, PROPERTY_HINT_PLACEHOLDER_TEXT, "org.godot"), ""/*GLOBAL_GET(prop_project_organization)*/));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_application_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "example"), ""/*GLOBAL_GET(prop_project_application_name)*/));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_package_version, PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use version from project config: application/config/version"), GLOBAL_GET(prop_project_package_version)));
		r_options->push_back(ExportOption(PropertyInfo(Variant::INT, prop_export_release_version, PROPERTY_HINT_RANGE, "1,40096,1,or_greater"), 1));
		
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_icon_86, PROPERTY_HINT_GLOBAL_FILE, "*.png"),  "res://icons/86.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_icon_108, PROPERTY_HINT_GLOBAL_FILE, "*.png"), "res://icons/108.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_icon_128, PROPERTY_HINT_GLOBAL_FILE, "*.png"), "res://icons/128.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_icon_172, PROPERTY_HINT_GLOBAL_FILE, "*.png"), "res://icons/172.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_sign_key, PROPERTY_HINT_GLOBAL_FILE, "*.pem"), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_sign_cert, PROPERTY_HINT_GLOBAL_FILE, "*.pem"), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_export_sign_password, PROPERTY_HINT_PASSWORD, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));

		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_release, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_debug, PROPERTY_HINT_GLOBAL_FILE), ""));

		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_arm, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_arm_debug, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_aarch64, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_aarch64_debug, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_x86, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_x86_debug, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_prefix, PROPERTY_HINT_ENUM, "/usr,/home/nemo/.local"), "/usr"));

		// r_options->push_back(ExportOption(PropertyInfo(Variant::INT, prop_version_release, PROPERTY_HINT_RANGE, "1,40096,1,or_greater"), 1));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_version_string, PROPERTY_HINT_PLACEHOLDER_TEXT, "1.0.0"), "1.0.0"));

		// String gename =
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "harbour-$gename", PROPERTY_USAGE_READ_ONLY), "harbour-$gename"));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_launcher_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name [default if blank]"), ""));

		// String global_icon_path = ProjectSettings::get_singleton()->get("application/config/icon");
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_86, PROPERTY_HINT_GLOBAL_FILE), "res://icons/86.png"));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_108, PROPERTY_HINT_GLOBAL_FILE), "res://icons/108.png"));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_128, PROPERTY_HINT_GLOBAL_FILE), "res://icons/128.png"));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_172, PROPERTY_HINT_GLOBAL_FILE), "res://icons/172.png"));

		// r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, prop_sailjail_enabled), true));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_organization, PROPERTY_HINT_PLACEHOLDER_TEXT, "org.godot"), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_application_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "mygame"), ""));
		int permission_num = 0;
		while (!sailjail_permissions[permission_num].is_empty()) {
			r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, String(prop_sailjail_permissions) + sailjail_permissions[permission_num++]), false));
		}

		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, prop_validator_enable), false));

		// r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, prop_aurora_sign_enable), false));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_aurora_sign_key, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_aurora_sign_cert, PROPERTY_HINT_GLOBAL_FILE), ""));
	}

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override
	{
		// String binary_template;
		String binary_arch;

		Error err;

		String driver = ProjectSettings::get_singleton()->get("rendering/renderer/rendering_method");
		SDKConnectType sdk_tool = SDKConnectType::tool_sfdk;
		String tool = EDITOR_GET(prop_editor_tool);

		if (tool == String("ssh"))
			sdk_tool = SDKConnectType::tool_ssh;

		if (driver != "gl_compability") {
			print_line(TTR("Vulkan render not finished in AuroraOS port! Driver is:") + driver);
			//return false;
		}

		r_missing_templates = false;
		int package_count = 0;
		if (p_preset->get(prop_export_binary_arch_armv7hl).operator bool()) {
			String template_path = EDITOR_GET(prop_editor_binary_arm);
			r_missing_templates = r_missing_templates || template_path.is_empty();
			if (r_missing_templates) {
				r_error = TTR(String("Can`t export without AuroraOS export templates: ") + arch_to_text(TargetArch::arch_armv7hl) + template_path);
				return false;
			}
			FileAccess::open(template_path, FileAccess::READ, &err);
			if (err != Error::OK) {
				r_error = vformat(TTR("Template file not found: \"%s\"."), template_path);
				return false;
			}
			package_count++;
		}
		if (p_preset->get(prop_export_binary_arch_aarch64).operator bool()) {
			String template_path = EDITOR_GET(prop_editor_binary_arm64);
			r_missing_templates = r_missing_templates || template_path.is_empty();
			if (r_missing_templates) {
				r_error = TTR(String("Can`t export without AuroraOS export templates: ") + arch_to_text(TargetArch::arch_aarch64) );
				return false;
			}
			FileAccess::open(template_path, FileAccess::READ, &err);
			if (err != Error::OK) {
				r_error = vformat(TTR("Template file not found: \"%s\"."), template_path);
				return false;
			}
			package_count++;
		}
		if (p_preset->get(prop_export_binary_arch_x86_64).operator bool()) {
			String template_path = EDITOR_GET(prop_editor_binary_x86_64);
			r_missing_templates = r_missing_templates || template_path.is_empty();
			if (r_missing_templates) {
				r_error = TTR(String("Can`t export without AuroraOS export templates: ") + arch_to_text(TargetArch::arch_x86_64));
				return false;
			}
			FileAccess::open(template_path, FileAccess::READ, &err);
			if (err != Error::OK) {
				r_error = vformat(TTR("Template file not found: \"%s\"."), template_path);
				return false;
			}
			package_count++;
		}

		if (package_count == 0) {
			r_error = TTR("Please select one or more export architectures.");
			return false;
		} 

		if (r_missing_templates) {
			r_error = TTR("Can`t export without AuroraOS export templates: ");
			r_missing_templates = true;
			return false;
		} 

		// print_verbose(binary_arch + String(" binary: ") + binary_template);

		// Ref<FileAccess> template_file = FileAccess::open(binary_template, FileAccess::READ, &err);
		// if (err != Error::OK) {
		// 	binary_template.clear();
		// 	r_error = TTR("Template file not exists!\n");
		// 	return false;
		// }

		// here need check if SDK is exists
		// String sfdk_path = String(EDITOR_GET(prop_editor_sdk_path));
		sdk_path = EDITOR_GET(prop_editor_sdk_path);
		if (!DirAccess::exists(sdk_path)) {
			sdk_path = String(EDITOR_GET(prop_editor_sdk_path));
			if (!DirAccess::exists(sdk_path)) {
				r_error = TTR("Wrong AuroraSDK path. Setup it in \nEditor->Settings->Export->AuroraOS->SDK Path,\nor setup it for current project\n");
				return false;
			}
		}
 
		// ---- if we use ssh, we need RSA keys for build engine
		if (sdk_tool == SDKConnectType::tool_ssh) {
			String rsa_key_path = sdk_path;
			rsa_key_path += String("/vmshare/ssh/private_keys/engine/mersdk");
			Ref<DirAccess> da = DirAccess::open(sdk_path, &err);
			if (da.is_null() || !da->file_exists(rsa_key_path)) {
				r_error = TTR("Cant find RSA key for access to build engine:\n") + rsa_key_path;
				return false;
			}
		}

		// check SDK version, minimum is 3.0.7
		Ref<FileAccess> sdk_release_file = FileAccess::open(sdk_path + separator + String("sdk-release"), FileAccess::READ, &err);

		if (err != Error::OK) {
			r_error = TTR("Wrong AuroraSDK path: cant find \"sdk-release\" file\n");
			return false;
		}
		bool wrong_sdk_version = false;
		String current_line;
		Vector<int> sdk_version;
		sdk_version.resize(3);
		while (!sdk_release_file->eof_reached()) {
			current_line = sdk_release_file->get_line();
			Vector<String> splitted = current_line.split("=");
			if (splitted.size() < 2)
				continue;

			if (splitted[0] == String("SDK_RELEASE")) {
				RegEx regex("([0-9]+)\\.([0-9]+)\\.([0-9]+)");
				Array matches = regex.search_all(splitted[1]);
				// print_verbose( String("Matches size: ") + Variant(matches.size()) );
				if (matches.size() == 1) {
					Ref<RegExMatch> rem = ((Ref<RegExMatch>)matches[0]);
					PackedStringArray names = rem->get_strings();
					if (names.size() >= 4) {
						if (names[1].to_int() > 3) {
							wrong_sdk_version = false;
						} else if (names[1].to_int() < 3) {
							r_error = TTR("Minimum AuroraSDK version is 3.0.7, current is ") + current_line.split("=")[1];
							wrong_sdk_version = true;
						} else if (names[2].to_int() > 0) {
							wrong_sdk_version = false;
						} else if (names[3].to_int() < 7) {
							r_error = TTR("Minimum AuroraSDK version is 3.0.7, current is ") + current_line.split("=")[1];
							wrong_sdk_version = true;
						}
						sdk_version.set(0, names[1].to_int());
						sdk_version.set(1, names[2].to_int());
						sdk_version.set(2, names[3].to_int());
					}
				} else {
					r_error = TTR("Cant parse \"sdk-release\" file in AuroraSDK directory");
					wrong_sdk_version = true;
				}
			} else if (splitted[0] == String("SDK_CONFIG_DIR")) {
				sdk_config_dir = splitted[1];
			}
		}
		sdk_release_file->close();
		if (wrong_sdk_version) {
			//r_error = TTR("Wrong AuroraSDK path: cant find \"sdk-release\" file");
			return false;
		}

		String sfdk_path;

		//  Chek if tool exists --------------------------------
		Ref<DirAccess> da = DirAccess::open(sdk_path, &err);

		if (sdk_tool == SDKConnectType::tool_sfdk) {
			sfdk_path = sdk_path + String("/bin/sfdk");

			if (err != Error::OK || da.is_null()  || !da->file_exists(sfdk_path)) {
				r_error = TTR("Wrong AuroraSDK path or sfdk tool not exists");
				return false;
			}
		} else {
			sfdk_path = EDITOR_GET(prop_editor_ssh_tool_path);

			if (err != Error::OK || da.is_null()  || !da->file_exists(sfdk_path)) {
				r_error = TTR("Wrong SSH tool path. Setup it in Editor->Settings->Export->AuroraOS");
				return false;
			}

			String rsa_key = sdk_path + String(mersdk_rsa_key);
			if (!da->file_exists(rsa_key)) {
				r_error = TTR("Cant find RSA key for acces to build engine. Try use AuroraOSIDE for generate keys.");
				return false;
			}
		}

		/// PARSE XML ------------------------------------------
		String xml_path;
		String sdk_configs_path = get_sdk_config_path(p_preset);
		xml_path = sdk_configs_path + String("/libsfdk/");
		xml_path += String("buildengines.xml");

		XMLParser *xml_parser = memnew(XMLParser);
		if (xml_parser->open(xml_path) != Error::OK) {
			memdelete(xml_parser);
			r_error = TTR("Cant open XML file: ") + xml_path;
			return false;
		}

		while (xml_parser->read() == Error::OK) {
			if (xml_parser->get_node_type() == XMLParser::NodeType::NODE_ELEMENT) {
				if (xml_parser->get_node_name() != String("value")) {
					String debug_string = xml_parser->get_node_name();
					print_verbose(String("Node skipping is: ") + debug_string);
					//xml_parser->skip_section();
					continue;
				}
				if (xml_parser->has_attribute("key")) {
					if (xml_parser->get_named_attribute_value("key") == String("SharedHome")) {
						xml_parser->read();
						shared_home = xml_parser->get_node_data();
					} else if (xml_parser->get_named_attribute_value("key") == String("SharedSrc")) {
						xml_parser->read();
						shared_src = xml_parser->get_node_data();
					}
				}
			}
		}
		xml_parser->close();
		memdelete(xml_parser);

		bool result = true;
		//---- checkl App Icon ------------------------------------------------
		String sizes[] = {
			String("86x86"),
			String("108x108"),
			String("128x128"),
			String("172x172")
		};
		{ // DirAccess 
			Ref<FileAccess> icon_path_check = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);
			for (int i = 0; i < 4; i++) {
				String current_icon_path = p_preset->get(String(prop_export_icon_config_path) + sizes[i]);
				print_verbose(String("Check if icon exists") + current_icon_path);
				if (!icon_path_check->exists(current_icon_path)) {
					r_error = String("Icon path not exists: ") + current_icon_path;
					return false;
				}
			}
		}
		// --- Package name ------------------------------------------
		String packname = p_preset->get(prop_export_application_organization).operator String() + String(".") + p_preset->get(prop_export_application_name).operator String();
		print_verbose(vformat("Genrated package name: %s", packname));
		// check packagename (if its set by user)
		if (packname.find("$genname") >= 0) {
			packname = packname.replace("$genname", "");
		}
		// check name by regex
		{
			String name; // = packname;
			RegEx regex("([a-z_\\-0-9\\.]+)");
			Array matches = regex.search_all(packname);
			// name.clear();
			for (int mi = 0; mi < matches.size(); mi++) {
				Ref<RegExMatch> rem = ((Ref<RegExMatch>)matches[mi]);
				PackedStringArray names = rem->get_strings();
				for (int n = 1; n < names.size(); n++) {
					name += names[n];
				}
			}
			if (packname != name) {
				r_error += TTR("Package name should be in lowercase, only 'a-z,_,-,0-9' symbols");
				return false;
			}
		}

		String orgname = p_preset->get(prop_export_application_organization);
		if (orgname.is_empty()) {
			r_error += TTR("Organization name should be reverse domain name, like: org.godot.");
			return false;
		}

		String appname = p_preset->get(prop_export_application_name);
		if (appname.is_empty()) {
			r_error += TTR("Application name is empty");
			return false;
		}

		String export_path = get_absolute_export_path(p_preset->get_export_path());
		print_verbose(String("Export path: ") + export_path);
		if ( shared_home.is_empty() || shared_src.is_empty() || 
			 (export_path.find(shared_home) < 0 && export_path.find(shared_src) < 0) ) {
			result = false;
			r_error += TTR("Export path is outside of Shared Home in AuroraSDK (choose export path inside shared home):\nSharedHome: ") + shared_home + String("\nShareedSource: ") + shared_src;
		}

		// if (p_preset->get(prop_aurora_sign_enable)) {
		if (true) {
			String key_path = p_preset->get(prop_export_sign_key);
			String cert_path =p_preset->get(prop_export_sign_cert);
			if (key_path.is_empty() || cert_path.is_empty()) {
				r_error += TTR("Signing enabled, but has empty paths for signing key and cert.");
				result = false;
			} else if (!da->file_exists(key_path) || !da->file_exists(cert_path)) {
				r_error += TTR("Key or cert file not exists. Check path to key and cert files.");
				result = false;
			}
		}

		return result;
	}

	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override
	{
		// TODO: FIXME
		return true;
	}

	List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override {
		List<String> ext;
		ext.push_back("rpm");
		return ext;
	}

	Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override {
		ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

		EditorProgress ep("export", "Exporting for AuroraOS", 100, true);
		ep.step("=== Starting AuroraOS export process ===", 1);

		String sdk_path = EDITOR_GET(prop_editor_sdk_path);
		String sdk_configs_path = OS::get_singleton()->get_config_path();
		String sfdk_tool = get_sfdk_path(p_preset);
		
		print_verbose("AuroraOS Export: SDK path: " + sdk_path);
		print_verbose("AuroraOS Export: SFDK tool path: " + sfdk_tool);

		SDKConnectType sdk_tool = SDKConnectType::tool_sfdk;
		String tool = EDITOR_GET(prop_editor_tool);

		if (tool == String("ssh")) {
			sdk_tool = SDKConnectType::tool_ssh;
			print_verbose("AuroraOS Export: Using SSH connection to SDK");
		} else {
			print_verbose("AuroraOS Export: Using SFDK tool connection");
		}

		String accept_templates;

		ep.step("checking export template binaries.", 5);
		print_verbose("AuroraOS Export: === Phase 1: Checking export template binaries ===");

		for (int i = TargetArch::arch_MIN; i < TargetArch::arch_MAX; i++) {
			String binary_template;
			String arch_name = arch_to_text((TargetArch)i);
			
			print_verbose("AuroraOS Export: Checking architecture: " + arch_name);
			
			bool arch_enabled = p_preset->get(String(prop_export_binary_arch) + String("/") + arch_name);
			print_verbose("AuroraOS Export: Architecture " + arch_name + " enabled in preset: " + (arch_enabled ? "YES" : "NO"));
			
			if (arch_enabled) {
				print_verbose("AuroraOS Export: Architecture " + arch_name + " is enabled in preset");
				switch(i) {
				case TargetArch::arch_armv7hl:
					binary_template = EDITOR_GET(prop_editor_binary_arm);
					print_verbose("AuroraOS Export: ARM32 template path from settings: " + binary_template);
					break;
				case TargetArch::arch_aarch64:
					binary_template = EDITOR_GET(prop_editor_binary_arm64);
					print_verbose("AuroraOS Export: ARM64 template path from settings: " + binary_template);
					break;
				case TargetArch::arch_x86_64:
					binary_template = EDITOR_GET(prop_editor_binary_x86_64);
					print_verbose("AuroraOS Export: x86_64 template path from settings: " + binary_template);
					break;
				}
				
				if (binary_template.is_empty()) {
					print_verbose("AuroraOS Export: No " + arch_name + " template setuped - skipping architecture");
					// binary_template = String(p_preset->get(prop_custom_binary_release));
				} else {
					if (accept_templates.is_empty()) {
						accept_templates = arch_name;
					} else {
						accept_templates += String(" ") + arch_name;
					}
					print_verbose("AuroraOS Export: Added " + arch_name + " to accept_templates");
				}
			} else {
				print_verbose("AuroraOS Export: Architecture " + arch_name + " is disabled in preset");
			}
		}

		print_verbose("AuroraOS Export: Final accept_templates string: '" + accept_templates + "'");

		if (accept_templates.is_empty() /*&& aarch64_template.is_empty() && x86_template.is_empty()*/) {
			print_error("AuroraOS Export: No export templates found");
			return ERR_CANT_CREATE;
		}

		ep.step("found export template binaries.", 10);
		print_verbose("AuroraOS Export: === Phase 2: Getting SDK targets ===");
		List<String> args;
		List<String> output_list;

		if (sdk_tool == SDKConnectType::tool_sfdk) {
			args.push_back("engine");
			args.push_back("exec");
		} else { // use SSH
			String rsa_key_path = sdk_path;
			rsa_key_path += String("/vmshare/ssh/private_keys/engine/mersdk");
			sfdk_tool = EDITOR_GET(prop_editor_ssh_tool_path);
			String ssh_port = EDITOR_GET(prop_editor_ssh_port);
			args.push_back("-o");
			args.push_back("\"IdentitiesOnly=yes\"");
			args.push_back("-i");
			args.push_back(String("\"") + rsa_key_path + String("\""));
			args.push_back("-p");
			args.push_back(ssh_port); // default is 2222 port
			args.push_back("mersdk@localhost");
		}
		args.push_back("sb2-config");
		args.push_back("-l");

		ep.step("check build targets", 20);
		List<MerTarget> targets;
		{ // echo verbose
			String result_cmd = sfdk_tool;
			for (int i = 0; i < args.size(); i++)
				result_cmd += String(" ") + args.get(i);
			print_verbose("AuroraOS Export: Executing command: " + result_cmd);
		}
		//int result = EditorNode::get_singleton()->execute_and_show_output(TTR("Run sfdk tool"), sfdk_tool, args, true, false);
		int result = execute_task(sfdk_tool, args, output_list);
		if (result != 0) {
			String result_cmd;
			List<String>::Element *e = output_list.front();
			while (e) {
				result_cmd += String("\n") + e->get();
				e = e->next();
			}
			EditorNode::get_singleton()->show_warning(TTR("Building of AuroraOS RPM failed, check output for the error.\nAlternatively visit docs.godotengine.org for AuroraOS build documentation.\n Output: ") + result_cmd);
			return ERR_CANT_CREATE;
		} else {
			print_verbose("AuroraOS Export: === Phase 3: Parsing SDK targets ===");
			// parse export targets, and choose two latest targets
			List<String>::Element *e = output_list.front();
			while (e) {
				String entry = e->get();
				print_verbose("AuroraOS Export: Processing SDK target line: " + entry);
				RegEx regex(".*AuroraOS-([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)(-base|-MB2)?-(armv7hl|x86_64|aarch64).*");
				Array matches = regex.search_all(entry);
				// print_verbose( String("Matches size: ") + Variant(matches.size()) );
				for (int mi = 0; mi < matches.size(); mi++) {
					MerTarget target;
					Ref<RegExMatch> rem = ((Ref<RegExMatch>)matches[mi]);
					PackedStringArray names = rem->get_strings();
					Dictionary formats;
					formats["size"] = names.size();
					print_verbose( String("AuroraOS Export: Regex match strings size: {size}").format(formats));
					if (names.size() < 7) {
						print_verbose("AuroraOS Export: Wrong match - insufficient groups");
						for (int d = 0; d < names.size(); d++) {
							print_verbose(String("AuroraOS Export: match[0].strings[") + String::num_int64(d) + String("]: ") + String(names[d]));
						}
						target.arch = arch_unkown;
					} else {
						target.name = "AuroraOS"; // no more SailfishOS support
						target.version[0] = names[1].to_int();
						target.version[1] = names[2].to_int();
						target.version[2] = names[3].to_int();
						target.version[3] = names[4].to_int();
						target.addversion = names[5];
						String target_arch = names[6];

						print_verbose("AuroraOS Export: Parsed target: " + target.name + "-" + names[1] + "." + names[2] + "." + names[3] + "." + names[4] + target.addversion + "-" + target_arch);

						// First, determine the architecture from the string
						if (target_arch == String("armv7hl")) {
							target.arch = arch_armv7hl;
						} else if (target_arch == String("x86_64")) {
							target.arch = arch_x86_64;
						} else if (target_arch == String("aarch64")) {
							target.arch = arch_aarch64;
						} else {
							target.arch = arch_unkown;
						}

						// Then check if this architecture is enabled
						bool arch_is_enabled = accept_templates.contains(target_arch);
						print_verbose("AuroraOS Export: Target architecture '" + target_arch + "' enabled: " + (arch_is_enabled ? "YES" : "NO"));
						print_verbose("AuroraOS Export: accept_templates contains: '" + accept_templates + "'");

						// Only set template path if architecture is enabled
						if (arch_is_enabled) {
							if (target.arch == arch_armv7hl) {
								target.target_template = EDITOR_GET(prop_editor_binary_arm);
								print_verbose("AuroraOS Export: Set ARM32 template: " + target.target_template);
							} else if (target.arch == arch_x86_64) {
								target.target_template = EDITOR_GET(prop_editor_binary_x86_64);
								print_verbose("AuroraOS Export: Set x86_64 template: " + target.target_template);
							} else if (target.arch == arch_aarch64) {
								target.target_template = EDITOR_GET(prop_editor_binary_arm64);
								print_verbose("AuroraOS Export: Set ARM64 template: " + target.target_template);
							}
						} else {
							continue;
						}

						print_verbose(String("AuroraOS Export: Target template path: ") + target.target_template);
						print_verbose(String("AuroraOS Export: Found target ") + mertarget_to_text(target) + String(" with arch: ") + target_arch);

						bool need_add_to_list = true;
						List<MerTarget>::Element *it = targets.front();
						for (; it != nullptr; it = it->next()) {
							MerTarget current = it->get();
							if (current.arch != target.arch)
								continue;
							int is_equal = 0;
							// check if target is more than added to list
							for (int v = 0; v < 4; v++) {
								if (current.version[v] > target.version[v]) {
									need_add_to_list = false;
									it->set(current);
									break;
								} else if (current.version[v] == target.version[v])
									is_equal++;
							}
							if (is_equal == 4)
								need_add_to_list = false;
							if (!need_add_to_list)
								continue;
						}
						if (need_add_to_list) {
							print_verbose(String("AuroraOS Export: Target added ") + mertarget_to_text(target) + String(" to export list"));
							targets.push_back(target);
						} else {
							print_verbose(String("AuroraOS Export: Target skipped ") + mertarget_to_text(target) + String(" (duplicate or older version)"));
						}
					}
				}

				e = e->next();
			}

			if (targets.size() == 0) {
				print_error("AuroraOS Export: No targets found.");
				return ERR_CANT_CREATE;
			}

			print_verbose("AuroraOS Export: === Phase 4: Building packages ===");
			print_verbose("AuroraOS Export: Total targets to build: " + String::num_int64(targets.size()));
			int one_target_progress_length = (90 - 20) / targets.size();
			int targets_succes = 0, target_num = 0;
			for (List<MerTarget>::Element *it = targets.front(); it != nullptr; it = it->next(), target_num++) {
				print_verbose("AuroraOS Export: === Building target " + String::num_int64(target_num + 1) + " of " + String::num_int64(targets.size()) + " ===");
				NativePackage pack;

				pack.release = vformat("%d", p_preset->get(prop_export_release_version).operator signed int());
				print_verbose(String("AuroraOS Export: Release version tag: ") + pack.release);
				pack.description = ProjectSettings::get_singleton()->get("application/config/description");
				pack.launcher_name = p_preset->get(prop_export_application_launcher_name);
				if (pack.launcher_name.is_empty()) {
					print_verbose("AuroraOS Export: Launcher name is empty, getting from project settings");
					pack.launcher_name = GLOBAL_GET(prop_project_launcher_name);
				}
				if (pack.launcher_name.is_empty())
					pack.launcher_name = ProjectSettings::get_singleton()->get("application/config/name");
				pack.name = p_preset->get(prop_export_application_organization).operator String() + String(".") + p_preset->get(prop_export_application_name).operator String();
				print_verbose(vformat("AuroraOS Export: Generated package name: %s", pack.name));
				print_verbose(vformat("AuroraOS Export: Launcher name: %s", pack.launcher_name));

				pack.version = p_preset->get(prop_export_package_version);
				if (pack.version.is_empty())
					pack.version = GLOBAL_GET(prop_project_package_version);
				print_verbose(vformat("AuroraOS Export: Package version: %s", pack.version));
				// TODO arch should be generated from current MerTarget
				pack.target = it->get();
				print_verbose("AuroraOS Export: Target: " + mertarget_to_text(pack.target));
				print_verbose("AuroraOS Export: Target template path: " + pack.target.target_template);

				if (pack.target.target_template.is_empty()) {
					print_error(String("AuroraOS Export: Target ") + mertarget_to_text(it->get()) + String(" template path is empty. Skip"));
					print_verbose("AuroraOS Export: REASON: Template path is empty - this means the architecture was not properly enabled or template not configured");
					continue;
				}

				print_verbose("AuroraOS Export: Starting build_package for target: " + mertarget_to_text(pack.target));
				if (build_package(pack, p_preset, p_debug, sfdk_tool, ep, 20 + one_target_progress_length * target_num, one_target_progress_length) != Error::OK) {
					// TODO Warning mesasgebox
					print_error(String("AuroraOS Export: Target ") + mertarget_to_text(it->get()) + String(" not exported succesfully"));
				} else {
					targets_succes++;
					print_verbose("AuroraOS Export: Target " + mertarget_to_text(it->get()) + " exported successfully");
				}
			}
			if (targets_succes == targets.size()) {
				ep.step("all targets build succes", 100);
				print_verbose("AuroraOS Export: === All targets built successfully ===");
			} else {
				// TODO add Warning messagebox
				ep.step("Not all targets builded", 100);
				print_verbose("AuroraOS Export: === Not all targets built successfully ===");
				print_verbose("AuroraOS Export: Successful targets: " + String::num_int64(targets_succes) + " / " + String::num_int64(targets.size()));
			}
		}

		print_verbose("AuroraOS Export: === AuroraOS export process completed ===");
		return Error::OK;
	}

	void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, HashSet<String> &p_features) override {
	}

protected:
	Ref<ImageTexture> logo;
	mutable String shared_home;
	mutable String shared_src;
	mutable String sdk_config_dir;
	mutable String sdk_path;
	mutable SDKConnectType sdk_connection_type;
};

void register_auroraos_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformAuroraOS);
}

void register_auroraos_exporter() {
	String exe_ext;
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = "*.exe";
	}

	// Ref<EditorExportPlatformAuroraOS> platform;
	// platform.instance();
	Ref<EditorExportPlatformAuroraOS> platform = Ref<EditorExportPlatformAuroraOS>(memnew(EditorExportPlatformAuroraOS));

	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);
		ImageLoaderSVG::create_image_from_string(img, _auroraos_logo_svg, EDSCALE, upsample, false);
		Ref<ImageTexture> logo = ImageTexture::create_from_image(img);
		platform->set_logo(logo);
	}

	EDITOR_DEF_BASIC(prop_editor_sdk_path, "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_sdk_path, PROPERTY_HINT_GLOBAL_DIR));

// #ifdef WINDOWS_ENABLED
// 	EDITOR_DEF(prop_editor_tool, "ssh");
// #else
	EDITOR_DEF_BASIC(prop_editor_tool, "sfdk");
// #endif
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_tool, PROPERTY_HINT_ENUM, "sfdk,ssh"));

#ifndef WINDOWS_ENABLED
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da->file_exists("/usr/bin/ssh"))
		EDITOR_DEF_BASIC(prop_editor_ssh_tool_path, "/usr/bin/ssh");
	else
		EDITOR_DEF_BASIC(prop_editor_ssh_tool_path, "");
		// da.reset()
#else
	EDITOR_DEF_BASIC(prop_editor_ssh_tool_path, "");
#endif
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_ssh_tool_path, PROPERTY_HINT_GLOBAL_FILE, exe_ext));

	EDITOR_DEF_BASIC(prop_editor_ssh_port, "2222");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, prop_editor_ssh_port, PROPERTY_HINT_RANGE, "1,40096,1,false"));

	EDITOR_DEF_BASIC(prop_editor_binary_arm, "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_binary_arm, PROPERTY_HINT_GLOBAL_FILE, ""));

	EDITOR_DEF_BASIC(prop_editor_binary_arm64, "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_binary_arm64, PROPERTY_HINT_GLOBAL_FILE, ""));

	EDITOR_DEF_BASIC(prop_editor_binary_x86_64, "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_binary_x86_64, PROPERTY_HINT_GLOBAL_FILE, ""));

	// GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, prop_project_application_name, PROPERTY_HINT_TYPE_STRING, ""), "");
	// GLOBAL_DEF_BASIC(prop_project_organization, "");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_organization, PROPERTY_HINT_TYPE_STRING, ""));

	// GLOBAL_DEF_BASIC(prop_project_application_name, "");
	// ProjectSettings::get_singleton()->set(prop_project_application_name, "");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_application_name, PROPERTY_HINT_TYPE_STRING, ""));
	
	// GLOBAL_DEF_BASIC(prop_project_release_version, "");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::INT, prop_project_release_version, PROPERTY_HINT_RANGE, "1,40096,1,or_greater", 1));
	// GLOBAL_DEF_BASIC(prop_project_icon_86, "res://icons/86.png");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_icon_86, PROPERTY_HINT_GLOBAL_FILE, "*.png"));
	// GLOBAL_DEF_BASIC(prop_project_icon_108, "res://icons/108.png");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_icon_108, PROPERTY_HINT_GLOBAL_FILE, "*.png"));
	// GLOBAL_DEF_BASIC(prop_project_icon_128, "res://icons/128.png");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_icon_128, PROPERTY_HINT_GLOBAL_FILE, "*.png"));
	// GLOBAL_DEF_BASIC(prop_project_icon_172, "res://icons/172.png");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_icon_172, PROPERTY_HINT_GLOBAL_FILE, "*.png"));
	// GLOBAL_DEF_BASIC(prop_project_sign_key, "");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_sign_key, PROPERTY_HINT_GLOBAL_FILE, ""));
	// GLOBAL_DEF_BASIC(prop_project_sign_cert, "");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_sign_cert, PROPERTY_HINT_GLOBAL_FILE, ""));
	// GLOBAL_DEF_BASIC(prop_project_sign_password_file, "");
	// ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, prop_project_sign_password_file, PROPERTY_HINT_GLOBAL_FILE, ""));

	EditorExport::get_singleton()->add_export_platform(platform);
}
