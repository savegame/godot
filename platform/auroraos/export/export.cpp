/*************************************************************************/
/*  export.cpp                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Author: sashikknox <sashikkxno@gmail.ocm>                             */
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "core/dictionary.h"
#include "core/io/marshalls.h"
#include "core/io/xml_parser.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/project_settings.h"
#include "core/version.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "main/main.h"
#include "modules/regex/regex.h"
#include "platform/auroraos/logo.gen.h"
#include "scene/resources/texture.h"

#define prop_editor_sdk_path "export/auroraos/sdk_path"
#define prop_editor_tool "export/auroraos/tool"
#define prop_editor_ssh_tool_path "export/auroraos/ssh_tool_path"
#define prop_editor_ssh_port "export/auroraos/ssh_port"

#define prop_auroraos_sdk_path "auroraos_sdk/sdk_path"
#define prop_custom_binary_arm "custom_binary/arm"
#define prop_custom_binary_arm_debug "custom_binary/arm_debug"
#define prop_custom_binary_x86 "custom_binary/x86"
#define prop_custom_binary_x86_debug "custom_binary/x86_debug"
#define prop_custom_binary_aarch64 "custom_binary/arm64"
#define prop_custom_binary_aarch64_debug "custom_binary/arm64_debug"
#define prop_version_release "version/release"
#define prop_version_string "version/string"
#define prop_package_name "package/name"
#define prop_package_launcher_name "package/game_name"
// #define prop_package_prefix "package/assets_prefix"
// #define prop_package_icon "package/launcher_icon"
#define prop_launcher_icons_86 "launcher_icons/launcher_icon_86"
#define prop_launcher_icons_108 "launcher_icons/launcher_icon_108"
#define prop_launcher_icons_128 "launcher_icons/launcher_icon_128"
#define prop_launcher_icons_172 "launcher_icons/launcher_icon_172"
// firejail / sailjail properties
#define prop_sailjail_permissions "sialjail_permissions/"
#define prop_sailjail_enabled "sialjail/enabled"
#define prop_sailjail_organization "sialjail/organization"
#define prop_sailjail_application_name "sialjail/application_name"
#define prop_validator_enable "rpm_validator/enable"
// aurora os rpm signing properties
#define prop_aurora_sign_enable "AuroraOS/sign"
#define prop_aurora_sign_key "AuroraOS/key"
#define prop_aurora_sign_cert "AuroraOS/cert"

#define mersdk_rsa_key "/vmshare/ssh/private_keys/engine/mersdk"

const String sailjail_minimum_required_permissions[] = {
	"Audio", // required in any case
	"Internet", // looks like its too required FIXME: debug godot and made it useable without internet permission  on device
	String()
};

const String sailjail_permissions[] = {
	"Bluetooth",
	"Camera",
	// "Internet", // looks like its too required
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
		"%define __requires_exclude ^lib(freetype|SDL2).*$\n"
		"%define __provides_exclude_from ^%{_datadir}/%{name}/lib/$\n"
		"# << macros\n"
		"Summary:    %{_gd_launcher_name}\n"
		"Version:    %{_gd_version}\n"
		"Release:    %{_gd_release}\n"
		"Group:      Game\n"
		"License:    Proprietary\n"
		"BuildArch:  %{_gd_architecture}\n"
		"URL:        http://example.org/\n"
		// "Requires:   SDL2 >= 2.0.9\n"
		// "Requires:   freetype\n"
		// "Requires:   libpng\n"
		// "Requires:   openssl\n"
		// "Requires:   zlib\n"
		// "Requires:   glib2\n"
		// "Requires:   libaudioresource\n"
		"\n"
		"%define _topdir %{_gd_shared_path}%{_gd_export_path}\n"
		"\n"
		"%description\n"
		"%{_gd_description}\n"
		"%prep\n"
		"echo \"Nothing to do here. Skip this step\"\n"
		"%build\n"
		"echo \"Nothing to do here. Skip this step\"\n"
		"%install\n"
		"rm -rf %{buildroot}\n"
		"mkdir -p \"%{buildroot}\"\n"
		"mkdir -p \"%{buildroot}%{_bindir}\"\n"
		"rm -fr \"%{buildroot}%{_bindir}\"\n"
		// "mkdir -p \"%{buildroot}%{_datadir}/%{name}\"\n"
		// "mkdir -p \"%{buildroot}/usr/share/applications\"\n"
		// "cp -r \"%{_topdir}/usr/share/applications/\"* \"%{buildroot}/usr/share/applications/\"\n"
		// "cp -r \"%{_topdir}/%{_bindir}/\"* \"%{buildroot}%{_bindir}/\"\n"
		"mv \"%{_topdir}/BUILD%{_bindir}\" \"%{buildroot}%{_bindir}\"\n"
		// "cp -r \"%{_topdir}/%{_datadir}/%{name}/\"* \"%{buildroot}%{_datadir}/%{name}/\"\n"
		"mv  \"%{_topdir}/BUILD%{_datadir}\" \"%{buildroot}%{_datadir}\"\n"
		"mkdir -p \"%{buildroot}/usr/share/applications\"\n"
		"[ -f \"%{_topdir}/BUILD/usr/share/applications/%{name}.desktop\" ] && mv -f \"%{_topdir}/BUILD/usr/share/applications/%{name}.desktop\" \"%{buildroot}/usr/share/applications/%{name}.desktop\"||echo \"File moved already\"\n"
		"chmod 755 %{buildroot}/usr/share/icons/hicolor/*\n"
		"chmod 755 %{buildroot}/usr/share/icons/hicolor/*/apps\n"
		//libudev.so.1 - not allowed dependency..
		"\n"
		"%files\n"
		"%defattr(644,root,root,-)\n"
		"%attr(755,root,root) %{_bindir}/%{name}\n"
		// "%attr(644,root,root) %{_datadir}/%{name}/%{name}.png\n" // FIXME: add icons for all resolutions, as AuroraOS specification needed
		"%{_datadir}/icons/hicolor/*\n"
		"%attr(644,root,root) %{_datadir}/%{name}/%{name}.pck\n"
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
		"X-Nemo-Application-Type=silica-qt5\n"
		"Icon=%{name}\n"
		"Exec=%{name} --main-pack %{_datadir}/%{name}/%{name}.pck\n" // FIXME: AURORAOS use invoker!
		"Name=%{_gd_launcher_name}\n"
		"Name[en]=%{_gd_launcher_name}\n";

const String desktop_file_sailjail =
		"\n"
		"[X-Sailjail]\n"
		"Permissions=%{permissions}\n"
		"OrganizationName=%{organization}\n"
		"ApplicationName=%{appname}\n";

static void _execute_thread(void *p_ud) {
	EditorNode::ExecuteThreadArgs *eta = (EditorNode::ExecuteThreadArgs *)p_ud;
	Error err = OS::get_singleton()->execute(eta->path, eta->args, true, NULL, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex);
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
		arch_i486,
		arch_x86 = arch_i486,
		arch_aarch64,
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
		if( !target.addversion.empty() )
			addversion = target.addversion + addversion;
		return target.name + String("-%d.%d.%d.%d").sprintf(args, &error) + addversion + arch_to_text(target.arch) + String(".default");
	}

	String arch_to_text(TargetArch arch) const {
		switch (arch) {
			case arch_armv7hl:
				return "armv7hl";
				break;
			case arch_i486:
				return "i486";
				break;
			case arch_aarch64:
				return "aarch64";
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
		sdk_configs_path = String(p_preset->get(prop_auroraos_sdk_path)) + separator + String("settings");
#endif
		sdk_configs_path += separator + sdk_config_dir;
		return sdk_configs_path;
	}

	String get_absolute_export_path(const String &realitive_export_path) const {
		String export_path = realitive_export_path;
		String project_path = ProjectSettings::get_singleton()->get_resource_path();

		if (project_path.find_last(separator) == project_path.length() - 1)
			project_path = project_path.left(project_path.find_last(separator));
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
				int pos = project_path.find_last(separator);
				if (pos >= 0) {
					project_path = project_path.left(pos);
				}
			}
			export_path = project_path + separator + export_path;
		}
		return export_path;
	}

	String get_sfdk_path(const Ref<EditorExportPreset> &p_preset) const {
		String sfdk_path = String(p_preset->get(prop_auroraos_sdk_path));
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
		int steps = 8; // if add some step to build process, need change it
		const bool validate_rpm = p_preset->get(prop_validator_enable);
		const bool sign_rpm = p_preset->get(prop_aurora_sign_enable);
		const bool sailjail_enabled = p_preset->get(prop_sailjail_enabled);
		int current_step = 0;
		int progress_step = progress_full / steps;

		if (validate_rpm) {
			steps++; //another step to validate PRM
		}

		if (sign_rpm) {
			steps++; //another step to sign PRM
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
			String rsa_key_path = p_preset->get(prop_auroraos_sdk_path);
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
				if (p_debug)
					export_template = String(p_preset->get(prop_custom_binary_arm_debug));
				if (export_template.empty())
					export_template = String(p_preset->get(prop_custom_binary_arm));
				break;
			case arch_aarch64:
				if (p_debug)
					export_template = String(p_preset->get(prop_custom_binary_aarch64_debug));
				if (export_template.empty())
					export_template = String(p_preset->get(prop_custom_binary_aarch64));
				break;
			case arch_x86:
				if (p_debug)
					export_template = String(p_preset->get(prop_custom_binary_x86_debug));
				if (export_template.empty())
					export_template = String(p_preset->get(prop_custom_binary_x86));
				break;
			default:
				print_error("Wrong architecture of package");
				return ERR_CANT_CREATE;
		}

		if (export_template.empty()) {
			print_error(String("No ") + arch_to_text(package.target.arch) + String(" template setuped"));
			return ERR_CANT_CREATE;
		}

		String target_string = mertarget_to_text(package.target);
		String export_path = get_absolute_export_path(p_preset->get_export_path());
		String broot_path = export_path + String("_buildroot");
		String build_folder = separator + ("BUILD") + separator;
		String rpm_prefix_path = broot_path.left(broot_path.find_last(separator));
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
		String sailjail_organization_name = String(p_preset->get(prop_sailjail_organization));

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

			err = OS::get_singleton()->execute(sfdk_tool, args, true, nullptr, &pipe, &exitcode, false /*read stderr*/);

			if (err == OK) {
				if (exitcode == 0 || String("returncode_true") == pipe) {
					print_line("Use export path without any changes, its same in host and in build engine");
					is_export_path_exits_in_sdk = true;
				}
			} else {
				print_line(pipe);
			}
		}
		if (!shared_home.empty() && export_path.find(shared_home) == 0) {
			// print_verbose(String("sfdk") + result_string);
			// String execute_binary = sfdk_tool;
			export_path_part = export_path.substr(shared_home.length(), export_path.length() - shared_home.length()).replace(separator, "/") + String("_buildroot");
			if (is_export_path_exits_in_sdk)
				sdk_shared_path = shared_home;
			else
				sdk_shared_path = String("/home/mersdk/share");
		} else if (!shared_src.empty() && export_path.find(shared_src) == 0) {
			export_path_part = export_path.substr(shared_src.length(), export_path.length() - shared_src.length()).replace(separator, "/") + String("_buildroot");
			if (is_export_path_exits_in_sdk)
				sdk_shared_path = shared_src;
			else
				sdk_shared_path = String("/home/src1");
		} else {
			print_error(String("Export path outside of SharedHome and SharedSrc:\nSharedHome: ") + shared_home + String("\nSharedSrc: ") + shared_src);
			return ERR_CANT_CREATE;
		}

		DirAccessRef broot = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
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
			print_error(String("Cant copy ") + arch_to_text(package.target.arch) + String(" template binary to: ") + binary_result_path);
			return ERR_CANT_CREATE;
		}

		ep.step(String("create name.pck file.").replace("name", package.name), progress_from + (++current_step) * progress_step);
		pck_path = broot_path + build_folder + data_dir_native + separator + package.name + separator + package.name + String(".pck");
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
		ProjectSettings::get_singleton()->set("application/config/use_custom_user_dir",use_custom_dir);
		ProjectSettings::get_singleton()->set("application/config/custom_user_dir_name", custom_path);

		if (err != Error::OK) {
			print_error(String("Cant create *.pck: ") + pck_path);
			return err;
		}

		ep.step(String("generate ") + package.name + String(".spec file"), progress_from + (++current_step) * progress_step);
		{
			FileAccessRef spec_file = FileAccess::open(spec_file_path, FileAccess::WRITE, &err);
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
			FileAccessRef desktop_file = FileAccess::open(desktop_file_path, FileAccess::WRITE, &err);
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
				while (!sailjail_minimum_required_permissions[permission_num].empty()) {
					if (permissions.empty())
						permissions = sailjail_minimum_required_permissions[permission_num];
					else
						permissions = permissions + String(";") + sailjail_minimum_required_permissions[permission_num];
					permission_num++;
				}
				// all other permissions
				permission_num = 0;
				while (!sailjail_permissions[permission_num].empty()) {
					if (!p_preset->get(String(prop_sailjail_permissions) + sailjail_permissions[permission_num++]))
						continue;
					if (permissions.empty())
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
			String(p_preset->get(prop_launcher_icons_86)), "86x86",
			String(p_preset->get(prop_launcher_icons_108)), "108x108",
			String(p_preset->get(prop_launcher_icons_128)), "128x128",
			String(p_preset->get(prop_launcher_icons_172)), "172x172",
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

		ep.step(String("setup SDK tool for ") + arch_to_text(package.target.arch) + String(" package build"), progress_from + (++current_step) * progress_step);
		{
			String build_script;
			String buid_script_path = broot_path + separator + String("buildscript.sh");
			{ //
				FileAccessRef script_file = FileAccess::open(buid_script_path, FileAccess::WRITE, &err);
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

			int result = EditorNode::get_singleton()->execute_and_show_output(TTR("Build rpm package for ") + target_string, execute_binary, args, true, false);
			if (result != 0) {
				return ERR_CANT_CREATE;
			}
			print_verbose("Move result RPMS to outside folder");
			{
				String build_result_dir = broot_path + separator + String("RPMS") + separator + arch_to_text(package.target.arch);
				DirAccessRef move_result = DirAccess::create_for_path(build_result_dir);
				String result_rpm_name = package.name + String("-") + package.version + String("-") + package.release + String(".") + arch_to_text(package.target.arch) + String(".rpm");
				if (!move_result)
					return ERR_CANT_CREATE;
				if (move_result->exists(rpm_prefix_path + separator + result_rpm_name))
					move_result->remove(rpm_prefix_path + separator + result_rpm_name);
				err = move_result->rename(build_result_dir + separator + result_rpm_name, rpm_prefix_path + separator + result_rpm_name);

				if (err != Error::OK) {
					print_error(TTR("Cant move result of build, please check it by yourself: ") + broot_path + separator + String("RPMS"));
					return ERR_CANT_CREATE;
				}

				if (sign_rpm) {
					String key_path = p_preset->get(prop_aurora_sign_key);
					String cert_path = p_preset->get(prop_aurora_sign_cert);
					ep.step(String("Signing RPM file: ") + result_rpm_name, progress_from + (++current_step) * progress_step);
					String rpm_path = rpm_prefix_path + separator + result_rpm_name;

					args.clear();
					for(List<String>::Element *a = pre_args.front(); a != nullptr; a = a->next()) {
						args.push_back( a->get() );
					}
					args.push_back("sb2");
					args.push_back("-t");
					args.push_back(target_string);
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
						args.push_back("config");
						args.push_back(String("target=") + target_string.replace(".default", ""));

						result = EditorNode::get_singleton()->execute_and_show_output(TTR("Set target for sfdk validator tool ") + target_string.replace(".default", ""), execute_binary, args, true, false);
						if (result != 0) {
							return ERR_CANT_CREATE;
						}

						args.clear();
						args.push_back("check");
						args.push_back(rpm_path);

						result = EditorNode::get_singleton()->execute_and_show_output(TTR("Validate RPM package ") + result_rpm_name, execute_binary, args, true, false);
						if (result != 0) {
							return ERR_CANT_CREATE;
						}
					} else {
						ep.step(String("Cant Validate RPM file: ") + result_rpm_name, progress_from + (++current_step) * progress_step);
						// for(List<String>::Element *a = pre_args.front(); a != nullptr; a = a->next()) {
						// 	args.push_back( a->get() );
						// }
						// args.push_back("sb2");
						// args.push_back("-t");
						// args.push_back(target_string);
						// args.push_back("")
					}
				}
			}
		}
		ep.step(String("remove temp directory"), progress_from + (++current_step) * progress_step);
		{
			DirAccessRef rmdir = DirAccess::open(broot_path, &err);
			if (err != Error::OK) {
				print_error("cant open dir");
			}
			rmdir->erase_contents_recursive(); //rmdir->remove(<#String p_name#>)
			rmdir->remove(broot_path);
		}

		ep.step(String("build ") + arch_to_text(package.target.arch) + String(" target success"), progress_from + (++current_step) * progress_step);
		return Error::OK;
	}

public:
	EditorExportPlatformAuroraOS() {
		// Ref<Image> img = memnew(Image(_auroraos_logo));
		// logo.instance();
		// logo->create_from_image(img);
	}

	void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) override {
		//        print_verbose("get_preset_features, path " + p_preset->get_export_path() );
		String driver = ProjectSettings::get_singleton()->get("rendering/quality/driver/driver_name");
		if (driver == "GLES2") {
			r_features->push_back("etc");
		} else if (driver == "GLES3") {
			r_features->push_back("etc2");
			r_features->push_back("s3tc");
			if (ProjectSettings::get_singleton()->get("rendering/quality/driver/fallback_to_gles2")) {
				r_features->push_back("etc");
			}
		}
	}

	virtual void get_platform_features(List<String> *r_features) override {
		r_features->push_back("mobile");
		r_features->push_back(get_os_name());
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

	Ref<Texture> get_logo() const override {
		return logo;
	}

	void get_export_options(List<ExportOption> *r_options) override {
		EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_sdk_path, PROPERTY_HINT_GLOBAL_DIR));
		bool global_valid = false;
		String global_sdk_path = EditorSettings::get_singleton()->get(prop_editor_sdk_path, &global_valid);
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_auroraos_sdk_path, PROPERTY_HINT_GLOBAL_DIR), (global_valid) ? global_sdk_path : ""));

		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_arm, PROPERTY_HINT_GLOBAL_FILE), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_arm_debug, PROPERTY_HINT_GLOBAL_FILE), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_aarch64, PROPERTY_HINT_GLOBAL_FILE), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_aarch64_debug, PROPERTY_HINT_GLOBAL_FILE), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_x86, PROPERTY_HINT_GLOBAL_FILE), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_custom_binary_x86_debug, PROPERTY_HINT_GLOBAL_FILE), ""));
		// r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_prefix, PROPERTY_HINT_ENUM, "/usr,/home/nemo/.local"), "/usr"));

		r_options->push_back(ExportOption(PropertyInfo(Variant::INT, prop_version_release, PROPERTY_HINT_RANGE, "1,40096,1,or_greater"), 1));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_version_string, PROPERTY_HINT_PLACEHOLDER_TEXT, "1.0.0"), "1.0.0"));

		// String gename =
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "harbour-$genname"), "harbour-$genname"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_package_launcher_name, PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name [default if blank]"), ""));

		// String global_icon_path = ProjectSettings::get_singleton()->get("application/config/icon");
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_86, PROPERTY_HINT_GLOBAL_FILE), "res://icons/86.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_108, PROPERTY_HINT_GLOBAL_FILE), "res://icons/108.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_128, PROPERTY_HINT_GLOBAL_FILE), "res://icons/128.png"));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_launcher_icons_172, PROPERTY_HINT_GLOBAL_FILE), "res://icons/172.png"));

		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, prop_sailjail_enabled), true));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_sailjail_organization, PROPERTY_HINT_PLACEHOLDER_TEXT, "org.godot"), ""));
		int permission_num = 0;
		while (!sailjail_permissions[permission_num].empty()) {
			r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, String(prop_sailjail_permissions) + sailjail_permissions[permission_num++]), false));
		}

		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, prop_validator_enable), false));

		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, prop_aurora_sign_enable), false));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_aurora_sign_key, PROPERTY_HINT_GLOBAL_FILE), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, prop_aurora_sign_cert, PROPERTY_HINT_GLOBAL_FILE), ""));
	}

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const override
	{
		String arm_template;
		String x86_template;
		String aarch64_template;
		Error err;
		bool p_debug = false;

		String driver = ProjectSettings::get_singleton()->get("rendering/quality/driver/driver_name");
		SDKConnectType sdk_tool = SDKConnectType::tool_sfdk;
		String tool = EDITOR_GET(prop_editor_tool);

		if (tool == String("ssh"))
			sdk_tool = SDKConnectType::tool_ssh;

		if (driver == "GLES3") {
			print_line(TTR("GLES3 render not finished in AuroraOS port! Sorry, but it work properly in Portrait orientation only."));
			//return false;
		}

		if (p_debug)
			arm_template = String(p_preset->get(prop_custom_binary_arm_debug));
		else
			arm_template = String(p_preset->get(prop_custom_binary_arm));

		if (p_debug)
			x86_template = String(p_preset->get(prop_custom_binary_x86_debug));
		else
			x86_template = String(p_preset->get(prop_custom_binary_x86));

		if (p_debug)
			aarch64_template = String(p_preset->get(prop_custom_binary_aarch64_debug));
		else
			aarch64_template = String(p_preset->get(prop_custom_binary_aarch64));

		print_verbose(String("arm_binary: ") + arm_template);
		print_verbose(String("x86_binary: ") + x86_template);
		print_verbose(String("aarch64_binary: ") + aarch64_template);

		if (arm_template.empty() && x86_template.empty() && aarch64_template.empty()) {
			r_error = TTR("Cant export without AuroraOS export templates");
			r_missing_templates = true;
			return false;
		} else {
			bool has_tamplate = false;
			if (!arm_template.empty()) {
				FileAccessRef template_file = FileAccess::open(arm_template, FileAccess::READ, &err);
				if (err != Error::OK) {
					arm_template.clear();
				} else {
					//                    template_file->get_
					has_tamplate = true;
				}
			}

			if (!x86_template.empty()) {
				FileAccessRef template_file = FileAccess::open(x86_template, FileAccess::READ, &err);
				if (err != Error::OK) {
					x86_template.clear();
				} else
					has_tamplate = true;
			}

			if (!aarch64_template.empty()) {
				FileAccessRef template_file = FileAccess::open(aarch64_template, FileAccess::READ, &err);
				if (err != Error::OK) {
					aarch64_template.clear();
				} else
					has_tamplate = true;
			}

			if (!has_tamplate) {
				r_error = TTR("Template files not exists\n");
				return false;
			}
		}

		// here need check if SDK is exists
		// String sfdk_path = String(p_preset->get(prop_auroraos_sdk_path));
		sdk_path = EDITOR_GET(prop_editor_sdk_path);
		if (!DirAccess::exists(sdk_path)) {
			sdk_path = String(p_preset->get(prop_auroraos_sdk_path));
			if (!DirAccess::exists(sdk_path)) {
				r_error = TTR("Wrong AuroraSDK path. Setup it in \nEditor->Settings->Export->AuroraOS->SDK Path,\nor setup it for current project\n");
				return false;
			}
		}

		// ---- if we use ssh, we need RSA keys for build engine
		if (sdk_tool == SDKConnectType::tool_ssh) {
			String rsa_key_path = sdk_path;
			rsa_key_path += String("/vmshare/ssh/private_keys/engine/mersdk");
			DirAccessRef da = DirAccess::open(sdk_path, &err);
			if (!da || !da->file_exists(rsa_key_path)) {
				r_error = TTR("Cant find RSA key for access to build engine:\n") + rsa_key_path;
				return false;
			}
		}

		// check SDK version, minimum is 3.0.7
		FileAccessRef sdk_release_file = FileAccess::open(sdk_path + separator + String("sdk-release"), FileAccess::READ, &err);

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
					Array names = rem->get_strings();
					if (names.size() >= 4) {
						if (int(names[1]) > 3) {
							wrong_sdk_version = false;
						} else if (int(names[1]) < 3) {
							r_error = TTR("Minimum AuroraSDK version is 3.0.7, current is ") + current_line.split("=")[1];
							wrong_sdk_version = true;
						} else if (int(names[2]) > 0) {
							wrong_sdk_version = false;
						} else if (int(names[3]) < 7) {
							r_error = TTR("Minimum AuroraSDK version is 3.0.7, current is ") + current_line.split("=")[1];
							wrong_sdk_version = true;
						}
						sdk_version.set(0, int(names[1]));
						sdk_version.set(1, int(names[2]));
						sdk_version.set(2, int(names[3]));
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
		;

		//  Chek if tool exists --------------------------------

		DirAccessRef da = DirAccess::open(sdk_path, &err);

		if (sdk_tool == SDKConnectType::tool_sfdk) {
			sfdk_path = sdk_path + String("/bin/sfdk");
#ifdef WINDOWS_ENABLED
			sfdk_path += String(".exe");
#endif
			if (err != Error::OK || !da || !da->file_exists(sfdk_path)) {
				r_error = TTR("Wrong AuroraSDK path or sfdk tool not exists");
				return false;
			}
		} else {
			sfdk_path = EDITOR_GET(prop_editor_ssh_tool_path);

			if (err != Error::OK || !da || !da->file_exists(sfdk_path)) {
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
					if (xml_parser->get_attribute_value("key") == String("SharedHome")) {
						xml_parser->read();
						shared_home = xml_parser->get_node_data();
					} else if (xml_parser->get_attribute_value("key") == String("SharedSrc")) {
						xml_parser->read();
						shared_src = xml_parser->get_node_data();
					}
				}
			}
		}
		xml_parser->close();
		memdelete(xml_parser);
		//----App Icon------------------------------------------------
		// TODO: check all app icons
		String icon = ProjectSettings::get_singleton()->get("application/config/icon");
		bool result = true;
		String suffix = icon.right(icon.length() - 3).to_lower();
		if (suffix != String("png")) {
			r_error += TTR("Icon file should be PNG. Set up custom icon for AuroraOS, or change icon of project");
			result = false;
		}
		// --- Package name ------------------------------------------
		String packname = p_preset->get(prop_package_name);
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
				Array names = rem->get_strings();
				for (int n = 1; n < names.size(); n++) {
					name += String(names[n]);
				}
			}
			if (packname != name) {
				r_error += TTR("Package name should be in lowercase, only 'a-z,_,-,0-9' symbols");
				return false;
			}
		}

		String orgname = String(p_preset->get(prop_sailjail_organization));
		if (orgname.empty()) {
			r_error += TTR("Organization name should be reverse domain name, like: org.godot");
			return false;
		}

		String export_path = get_absolute_export_path(p_preset->get_export_path());
		print_verbose(String("Export path: ") + export_path);
		if ( shared_home.empty() || shared_src.empty() || 
			 (export_path.find(shared_home) < 0 && export_path.find(shared_src) < 0) ) {
			result = false;
			r_error += TTR("Export path is outside of Shared Home in AuroraSDK (choose export path inside shared home):\nSharedHome: ") + shared_home + String("\nShareedSource: ") + shared_src;
		}

		if (p_preset->get(prop_aurora_sign_enable)) {
			String key_path = p_preset->get(prop_aurora_sign_key);
			String cert_path = p_preset->get(prop_aurora_sign_cert);
			if (key_path.empty() || cert_path.empty()) {
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

	Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags = 0) override {
		ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

		EditorProgress ep("export", "Exporting for AuroraOS", 100, true);

		String sdk_path = p_preset->get(prop_auroraos_sdk_path);
		String sdk_configs_path = OS::get_singleton()->get_config_path();
		String sfdk_tool = get_sfdk_path(p_preset);

		SDKConnectType sdk_tool = SDKConnectType::tool_sfdk;
		String tool = EDITOR_GET(prop_editor_tool);

		if (tool == String("ssh")) {
			sdk_tool = SDKConnectType::tool_ssh;
		}

		//        String mer_sdk_tools;
		List<MerTarget> mer_target; // Mer targets list
		String arm_template;
		String aarch64_template;
		String x86_template;

		ep.step("checking export template binaries.", 5);

		if (p_debug) {
			arm_template = String(p_preset->get(prop_custom_binary_arm_debug));
			if (arm_template.empty()) {
				print_error("Debug armv7hl template path is empty. Try use release template.");
			}
		}
		if (arm_template.empty()) {
			arm_template = String(p_preset->get(prop_custom_binary_arm));
			if (arm_template.empty()) {
				print_error("No armv7hl template setuped");
			}
		}

		if (p_debug) {
			aarch64_template = String(p_preset->get(prop_custom_binary_aarch64_debug));
			if (aarch64_template.empty()) {
				print_error("Debug aarch64 template path is empty. Try use release template.");
			}
		}
		if (aarch64_template.empty()) {
			aarch64_template = String(p_preset->get(prop_custom_binary_aarch64));
			if (aarch64_template.empty()) {
				print_error("No aarch64 template setuped");
			}
		}

		if (p_debug) {
			x86_template = String(p_preset->get(prop_custom_binary_x86_debug));
			if (x86_template.empty()) {
				print_error("Debug i486 template path is empty. Try use release template.");
			}
		}
		if (x86_template.empty()) {
			x86_template = String(p_preset->get(prop_custom_binary_x86));
			if (x86_template.empty()) {
				print_error("No i486 template setuped");
			}
		}
		if (arm_template.empty() && aarch64_template.empty() && x86_template.empty()) {
			return ERR_CANT_CREATE;
		}

		ep.step("found export template binaries.", 10);
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
				result_cmd += String(" ") + args[i];
			print_verbose(result_cmd);
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
			// parse export targets, and choose two latest targets
			List<String>::Element *e = output_list.front();
			while (e) {
				String entry = e->get();
				print_verbose(entry);
				RegEx regex(".*(AuroraOS|AuroraOS)-([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)(-base|-MB2)?-(armv7hl|i486|aarch64).*");
				Array matches = regex.search_all(entry);
				// print_verbose( String("Matches size: ") + Variant(matches.size()) );
				for (int mi = 0; mi < matches.size(); mi++) {
					MerTarget target;
					Ref<RegExMatch> rem = ((Ref<RegExMatch>)matches[mi]);
					Array names = rem->get_strings();
					// print_verbose( String("match[0] strings size: ") + Variant(names.size()) );
					if (names.size() < 8) {
						print_verbose("Wrong match");
						for (int d = 0; d < names.size(); d++) {
							print_verbose(String("match[0].strings[") + Variant(d) + String("]: ") + String(names[d]));
						}
						target.arch = arch_unkown;
					} else {
						target.name = names[1];
						target.version[0] = int(names[2]);
						target.version[1] = int(names[3]);
						target.version[2] = int(names[4]);
						target.version[3] = int(names[5]);
						target.addversion = names[6];
						String target_arch = names[7];

						if (target_arch == String("armv7hl")) {
							target.arch = arch_armv7hl;
							target.target_template = arm_template;
						} else if (target_arch == String("i486")) {
							target.arch = arch_i486;
							target.target_template = x86_template;
						} else if (target_arch == String("aarch64")) {
							target.arch = arch_aarch64;
							target.target_template = aarch64_template;
						} else
							target.arch = arch_unkown;

						print_verbose(String("Found target ") + mertarget_to_text(target));

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
							print_verbose(String("Target added ") + mertarget_to_text(target) + String(" to export list"));
							targets.push_back(target);
						}
					}
				}

				e = e->next();
			}

			if (targets.size() == 0) {
				print_error(String("No targets found."));
				return ERR_CANT_CREATE;
			}

			int one_target_progress_length = (90 - 20) / targets.size();
			int targets_succes = 0, target_num = 0;
			for (List<MerTarget>::Element *it = targets.front(); it != nullptr; it = it->next(), target_num++) {
				NativePackage pack;
				pack.release = p_preset->get(prop_version_release);
				pack.description = ProjectSettings::get_singleton()->get("application/config/description");
				pack.launcher_name = p_preset->get(prop_package_launcher_name);
				if (pack.launcher_name.empty())
					pack.launcher_name = ProjectSettings::get_singleton()->get("application/config/name");
				pack.name = p_preset->get(prop_package_name);
				if (pack.name.find("$genname") >= 0) {
					String name = ProjectSettings::get_singleton()->get("application/config/name");
					name = name.to_lower();
					RegEx regex("([a-z_\\-0-9\\.]+)");
					Array matches = regex.search_all(name);
					// print_verbose( String("Matches size: ") + Variant(matches.size()) );
					name.clear();
					for (int mi = 0; mi < matches.size(); mi++) {
						Ref<RegExMatch> rem = ((Ref<RegExMatch>)matches[mi]);
						Array names = rem->get_strings();
						for (int n = 1; n < names.size(); n++) {
							name += String(names[n]);
						}
					}
					pack.name = pack.name.replace("$genname", name);
				}
				pack.version = p_preset->get(prop_version_string);
				// TODO arch should be generated from current MerTarget
				pack.target = it->get();

				if (pack.target.target_template.empty()) {
					print_error(String("Target ") + mertarget_to_text(it->get()) + String(" template path is empty. Skip"));
					continue;
				}

				if (build_package(pack, p_preset, p_debug, sfdk_tool, ep, 20 + one_target_progress_length * target_num, one_target_progress_length) != Error::OK) {
					// TODO Warning mesasgebox
					print_error(String("Target ") + mertarget_to_text(it->get()) + String(" not exported succesfully"));
				} else
					targets_succes++;
			}
			if (targets_succes == targets.size()) {
				ep.step("all targets build succes", 100);
			} else {
				// TODO add Warning messagebox
				ep.step("Not all targets builded", 100);
			}
		}

		return Error::OK;
	}

	void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<String> &p_features) override {
	}

protected:
	Ref<ImageTexture> logo;
	mutable String shared_home;
	mutable String shared_src;
	mutable String sdk_config_dir;
	mutable String sdk_path;
	mutable SDKConnectType sdk_connection_type;
};

void register_auroraos_exporter() {
	String exe_ext;
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = "*.exe";
	}

	Ref<EditorExportPlatformAuroraOS> platform;
	// Ref<EditorExportPlatformPC> p;
	
	platform.instance();

	Ref<Image> img = memnew(Image(_auroraos_logo));
	Ref<ImageTexture> logo;
	logo.instance();
	logo->create_from_image(img);
	platform->set_logo(logo);

	EDITOR_DEF(prop_editor_sdk_path, "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_sdk_path, PROPERTY_HINT_GLOBAL_DIR));

#ifdef WINDOWS_ENABLED
	EDITOR_DEF(prop_editor_tool, "ssh");
#else
	EDITOR_DEF(prop_editor_tool, "sfdk");
#endif
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_tool, PROPERTY_HINT_ENUM, "sfdk,ssh"));

#ifndef WINDOWS_ENABLED
	DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da->file_exists("/usr/bin/ssh"))
		EDITOR_DEF(prop_editor_ssh_tool_path, "/usr/bin/ssh");
	else
		EDITOR_DEF(prop_editor_ssh_tool_path, "");
		// da.reset()
#else
	EDITOR_DEF(prop_editor_ssh_tool_path, "");
#endif
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, prop_editor_ssh_tool_path, PROPERTY_HINT_GLOBAL_FILE, exe_ext));

	EDITOR_DEF(prop_editor_ssh_port, "2222");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, prop_editor_ssh_port, PROPERTY_HINT_RANGE, "1,40096,1,false"));

	EditorExport::get_singleton()->add_export_platform(platform);
}
