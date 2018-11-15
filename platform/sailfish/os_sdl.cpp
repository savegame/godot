/*************************************************************************/
/*  os_sdl.cpp                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
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

#include <glib.h>
#include "os_sdl.h"
#include "core/string_db.h"
#include "drivers/gles2/rasterizer_gles2.h"
#include "errno.h"
#include "key_mapping_sdl.h"
#include "print_string.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"
#include "servers/physics/physics_server_sw.h"
#include "servers/physics_2d/physics_2d_server_wrap_mt.h"


#ifdef HAVE_MNTENT
#include <mntent.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ICCCM
#define WM_NormalState 1L // window normal state
#define WM_IconicState 3L // window minimized
// EWMH
#define _NET_WM_STATE_REMOVE 0L // remove/unset property
#define _NET_WM_STATE_ADD 1L // add/set property
#define _NET_WM_STATE_TOGGLE 2L // toggle property

#include "main/main.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <os/os.h>

//stupid linux.h
#ifdef KEY_TAB
#undef KEY_TAB
#endif

#undef CursorShape

static void on_audio_resource_acquired(audioresource_t*, bool, void*);

int OS_SDL::get_video_driver_count() const {
	return 1;
}

const char *OS_SDL::get_video_driver_name(int p_driver) const {
	return "GLES2";
}

OS::VideoMode OS_SDL::get_default_video_mode() const {
	OS::VideoMode def;
	def.width = 540;
	def.height = 960;
	def.fullscreen = true;
	def.resizable = false;
	def.borderless_window = true;
	def.always_on_top = false;
	return def;
}

int OS_SDL::get_audio_driver_count() const {
	return AudioDriverManagerSW::get_driver_count();
}

const char *OS_SDL::get_audio_driver_name(int p_driver) const {

	AudioDriverSW *driver = AudioDriverManagerSW::get_driver(p_driver);
	ERR_FAIL_COND_V(!driver, "");
	return AudioDriverManagerSW::get_driver(p_driver)->get_name();
}

void OS_SDL::initialize_core() {

	crash_handler.initialize();

	OS_Unix::initialize_core();
}

void OS_SDL::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {
	last_button_state = 0;

	sdl_window = NULL;
	last_click_ms = 0;
	args = OS::get_singleton()->get_cmdline_args();
	current_videomode = p_desired;
	main_loop = NULL;
	last_timestamp = 0;
	last_mouse_pos_valid = false;
	last_keyrelease_time = 0;

	// ** SDL INIT ** //

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ERR_PRINT("SDL Initialization Failed!");
		// return ERR_UNAVAILABLE;
	}

	context_gl = memnew(ContextGL_SDL(sdl_display_mode, current_videomode, true));
	context_gl->initialize();
	sdl_window = context_gl->get_window_pointer();

	bool use_reload_hooks = false;
	char * gl_extensions = NULL;

	RasterizerGLES2 *rasterizer_gles22 = memnew(RasterizerGLES2(false, use_reload_hooks, false, use_reload_hooks));
	if (gl_extensions)
		rasterizer_gles22->set_extensions(gl_extensions);
	rasterizer = rasterizer_gles22;

	visual_server = memnew(VisualServerRaster(rasterizer));

	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		visual_server = memnew(VisualServerWrapMT(visual_server, get_render_thread_mode() == RENDER_SEPARATE_THREAD));
	}

	// borderless fullscreen window mode
	if (current_videomode.fullscreen) {
		current_videomode.fullscreen = false;
		set_window_fullscreen(true);
	} else if (current_videomode.borderless_window) {
		set_borderless_window(current_videomode.borderless_window);
	}

	// enable / disable resizable window
	if (current_videomode.resizable) {
		set_window_resizable(current_videomode.resizable);
	}

	// for (int i = 0; i < CURSOR_MAX; i++) {
	// 	cursors[i] = NULL;
	// }
	memset(cursors, NULL, sizeof(SDL_Cursor*) * CURSOR_MAX );

	AudioDriverManagerSW::get_driver(p_audio_driver)->set_singleton();

	audio_resource  = audioresource_init(
		AUDIO_RESOURCE_GAME,
		on_audio_resource_acquired,
		this
	);
	audioresource_acquire(audio_resource);

	OS::get_singleton()->print("Wait libaudioresource initialization ");       
	while (!this->is_audio_resource_acquired) {
		OS::get_singleton()->print(".");
		g_main_context_iteration(NULL, false);
		// process_events();
		// force_process_input();
	}
	OS::get_singleton()->print("\nlibaudioresource initialization finished.\n");

	// OS::get_singleton()->print("AudioDriver %i\n", p_audio_driver);
	// if (AudioDriverManagerSW::get_driver(p_audio_driver)->init() != OK) 
	// {
	// 	ERR_PRINT("Initializing audio failed.");
	// }

	sample_manager = memnew(SampleManagerMallocSW);
	audio_server = memnew(AudioServerSW(sample_manager));
	audio_server->set_mixer_params(AudioMixerSW::INTERPOLATION_LINEAR, true);
	audio_server->init();

	spatial_sound_server = memnew(SpatialSoundServerSW);
	spatial_sound_server->init();

	spatial_sound_2d_server = memnew(SpatialSound2DServerSW);
	spatial_sound_2d_server->init();

	ERR_FAIL_COND(!visual_server);
	ERR_FAIL_COND(sdl_window == 0);

	visual_server->init();

	physics_server = memnew(PhysicsServerSW);
	physics_server->init();
	
	physics_2d_server = Physics2DServerWrapMT::init_server<Physics2DServerSW>();
	physics_2d_server->init();

	input = memnew(InputDefault);

#ifdef JOYDEV_ENABLED
	joypad = memnew(JoypadLinux(input));
#endif
	// return OK;
}

void OS_SDL::finalize() {

	if (main_loop)
		memdelete(main_loop);
	main_loop = NULL;

	/*
	if (debugger_connection_console) {
		memdelete(debugger_connection_console);
	}
	*/
	spatial_sound_server->finish();
	memdelete(spatial_sound_server);
	spatial_sound_2d_server->finish();
	memdelete(spatial_sound_2d_server);

	for( int i = 0; i < get_audio_driver_count(); i++ )
	{
		AudioDriverManagerSW::get_driver(i)->finish();
	}

	audioresource_release(audio_resource);
	audioresource_free(audio_resource);

#ifdef JOYDEV_ENABLED
	memdelete(joypad);
#endif
#ifdef TOUCH_ENABLED
	touch.devices.clear();
	touch.state.clear();
#endif
	memdelete(input);

	memdelete(sample_manager);

	audio_server->finish();
	memdelete(audio_server);

	visual_server->finish();
	memdelete(visual_server);
	memdelete(rasterizer);

	physics_server->finish();
	memdelete(physics_server);

	physics_2d_server->finish();
	memdelete(physics_2d_server);
	// memdelete(power_manager);

#if defined(OPENGL_ENABLED) || defined(GLES2_ENABLED)
	memdelete(context_gl);
#endif
	for (int i = 0; i < CURSOR_MAX; i++) {
		if (cursors[i] != NULL)
			SDL_FreeCursor(cursors[i]);
	}

	args.clear();
}

void OS_SDL::set_ime_position(const Point2 &p_pos) {
	int x = static_cast<int>(p_pos.x);
	int y = static_cast<int>(p_pos.y);

	// I'm not sure this is a good way to handle things.
	if (x != 0 && y != 0) {
		SDL_StartTextInput();
	} else {
		SDL_StopTextInput();
	}

	SDL_Rect ime_position;
	ime_position.x = x;
	ime_position.y = y;
	// FIXME: What should these values be?
	ime_position.w = 100;
	ime_position.h = 32;

	SDL_SetTextInputRect(&ime_position);
}


void OS_SDL::set_mouse_mode(MouseMode p_mode) {
	if (p_mode == mouse_mode) return;
	mouse_mode = p_mode;

	if (mouse_mode == MOUSE_MODE_VISIBLE)
		SDL_ShowCursor(SDL_ENABLE);
	else if (mouse_mode == MOUSE_MODE_HIDDEN)
		SDL_ShowCursor(SDL_DISABLE);

	// Allow for undoing of previously made changes.
	SDL_SetRelativeMouseMode(mouse_mode == MOUSE_MODE_CAPTURED ? SDL_TRUE : SDL_FALSE);
	// SDL_SetWindowGrab(sdl_window, mouse_mode == MOUSE_MODE_CONFINED ? SDL_TRUE : SDL_FALSE);
}

void OS_SDL::warp_mouse_position(const Point2 &p_to) {

	if (mouse_mode == MOUSE_MODE_CAPTURED) {

		last_mouse_pos = p_to;
	} else {
		SDL_WarpMouseInWindow(sdl_window, (int)p_to.x, (int)p_to.y);
	}
}

Point2 OS_SDL::get_mouse_pos() const 
{
	return last_mouse_pos;
}

OS::MouseMode OS_SDL::get_mouse_mode() const {
	return mouse_mode;
}

int OS_SDL::get_mouse_button_state() const {
	return last_button_state;
}

Point2 OS_SDL::get_mouse_position() const {
	return last_mouse_pos;
}

void OS_SDL::set_window_title(const String &p_title) {
	SDL_SetWindowTitle(sdl_window, (const char *)p_title.utf8().get_data());
}

void OS_SDL::set_video_mode(const VideoMode &p_video_mode, int p_screen) {
}

OS::VideoMode OS_SDL::get_video_mode(int p_screen) const {
	return current_videomode;
}

void OS_SDL::get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen) const {
}

void OS_SDL::set_wm_fullscreen(bool p_enabled) {
	if (current_videomode.fullscreen == p_enabled)
		return;

	SDL_SetWindowFullscreen(sdl_window, p_enabled ? SDL_WINDOW_FULLSCREEN : 0);
}

int OS_SDL::get_screen_count() const {
	int num_displays = SDL_GetNumVideoDisplays();

	if (num_displays < 0) {
		fprintf(stderr, "Could not get display count from SDL, Error: %s\n", SDL_GetError());
	}

	return num_displays;
}

int OS_SDL::get_current_screen() const {
	int current_display = SDL_GetWindowDisplayIndex(sdl_window);

	if (current_display < 0) {
		fprintf(stderr, "Could not get the current display from SDL, Error: %s\n", SDL_GetError());
	}

	return current_display;
}

void OS_SDL::set_current_screen(int p_screen) {
	int screen_count = get_screen_count();
	if (p_screen >= screen_count) return;

	if (current_videomode.fullscreen) {
		Size2i size = get_screen_size(p_screen);
		Point2i position = get_screen_position(p_screen);

		SDL_SetWindowPosition(sdl_window, position.x, position.y);
	} else {
		if (p_screen != get_current_screen()) {
			Point2i position = get_screen_position(p_screen);
			SDL_SetWindowPosition(sdl_window, position.x, position.y);
		}
	}
}

Point2 OS_SDL::get_screen_position(int p_screen) const {
	SDL_Rect display_bounds;

	if (SDL_GetDisplayBounds(p_screen, &display_bounds) != 0) {
		fprintf(stderr, "Could not get the screen position for display %i from SDL, Error: %s\n", p_screen, SDL_GetError());
		return Point2i(0, 0);
	}

	return Point2i(display_bounds.x, display_bounds.y);
}

Size2 OS_SDL::get_screen_size(int p_screen) const {
	SDL_Rect display_bounds;

	if (SDL_GetDisplayBounds(p_screen, &display_bounds) != 0) {
		fprintf(stderr, "Could not get the screen position for display %i from SDL, Error: %s\n", p_screen, SDL_GetError());
		return Size2i(0, 0);
	}

	return Size2i(display_bounds.w, display_bounds.h);
}

int OS_SDL::get_screen_dpi(int p_screen) const {
	if (p_screen == -1) {
		p_screen = get_current_screen();
	}

	// Invalid screen?
	ERR_FAIL_INDEX_V(p_screen, get_screen_count(), 0);

	float diagonal_dpi = 96.0f;

	// if (SDL_GetDisplayDPI(p_screen, &diagonal_dpi, NULL, NULL) != 0) {
	// 	fprintf(stderr, "Could not get the screen DPI for display %i from SDL, Error: %s\n", p_screen, SDL_GetError());
	// }

	return static_cast<int>(diagonal_dpi);
}

Point2 OS_SDL::get_window_position() const {
	int x, y;

	SDL_GetWindowPosition(sdl_window, &x, &y);

	return Point2i(x, y);
}

void OS_SDL::set_window_position(const Point2 &p_position) {
	SDL_SetWindowPosition(sdl_window, p_position.x, p_position.y);
}

Size2 OS_SDL::get_window_size() const {
	int w, h;
	SDL_GetWindowSize(sdl_window, &w, &h);

	// FIXME: Should we use current_videomode here instead?
	// FIXME: How should HIDPI be handled?
	return Size2i(w, h);
}

void OS_SDL::set_window_size(const Size2 p_size) {
	SDL_SetWindowSize(sdl_window, p_size.x, p_size.y);

	// Update our videomode width and height
	current_videomode.width = p_size.x;
	current_videomode.height = p_size.y;
}

void OS_SDL::set_window_fullscreen(bool p_enabled) {
	set_wm_fullscreen(p_enabled);
	current_videomode.fullscreen = p_enabled;
}

bool OS_SDL::is_window_fullscreen() const {
	return current_videomode.fullscreen;
}

void OS_SDL::set_window_resizable(bool p_enabled) {
	// SDL_SetWindowResizable(sdl_window, p_enabled ? SDL_TRUE : SDL_FALSE);
	current_videomode.resizable = false;
}

bool OS_SDL::is_window_resizable() const {
	return current_videomode.resizable;
}

void OS_SDL::set_window_minimized(bool p_enabled) {
	if (is_window_minimized() == p_enabled) return;

	if (p_enabled) {
		SDL_MinimizeWindow(sdl_window);
	} else {
		SDL_RestoreWindow(sdl_window);
	}
}

bool OS_SDL::is_window_minimized() const {
	uint32_t flags = SDL_GetWindowFlags(sdl_window);

	return flags & SDL_WINDOW_MINIMIZED;
}

void OS_SDL::set_window_maximized(bool p_enabled) {
	if (is_window_maximized() == p_enabled) return;

	if (p_enabled) {
		SDL_MaximizeWindow(sdl_window);
	} else {
		SDL_RestoreWindow(sdl_window);
	}

	if (is_window_maximize_allowed()) {
		while (p_enabled && !is_window_maximized()) {
			// Wait for effective resizing (so the GLX context is too).
		}
	}

	maximized = p_enabled;
}

bool OS_SDL::is_window_maximize_allowed() {
	uint32_t flags = SDL_GetWindowFlags(sdl_window);

	// FIXME: Not quite exact, but hopefully close enough.
	return flags & SDL_WINDOW_RESIZABLE;
}

bool OS_SDL::is_window_maximized() const {
	uint32_t flags = SDL_GetWindowFlags(sdl_window);

	// FIXME: Not quite exact, but hopefully close enough.
	return flags & SDL_WINDOW_MAXIMIZED;
}

void OS_SDL::set_borderless_window(bool p_borderless) {

	if (current_videomode.borderless_window == p_borderless)
		return;

	current_videomode.borderless_window = p_borderless;

	SDL_SetWindowBordered(sdl_window, p_borderless ? SDL_FALSE : SDL_TRUE);
}

bool OS_SDL::get_borderless_window() {
	return current_videomode.borderless_window;
}

// FIXME: Unimplemented
void OS_SDL::request_attention() {
}

InputModifierState OS_SDL::get_key_modifier_state(unsigned int state) {
	SDL_Keymod mod_state = SDL_GetModState();
	InputModifierState result;
	// InputEventMouseButton mouse;
	// mouse.
	result.shift = (mod_state & KMOD_SHIFT);
	result.control = (mod_state & KMOD_CTRL);
	result.alt = (mod_state & KMOD_ALT);
	result.meta = (mod_state & KMOD_GUI);

	return result;
}

unsigned int OS_SDL::get_mouse_button_state(uint32_t button_mask, bool refresh) {
	if (refresh) button_mask = SDL_GetMouseState(NULL, NULL);

	unsigned int state = 0;

	if (button_mask & SDL_BUTTON(SDL_BUTTON_LEFT)) state |= 1 << 0;
	if (button_mask & SDL_BUTTON(SDL_BUTTON_RIGHT)) state |= 1 << 1;
	if (button_mask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) state |= 1 << 2;
	if (button_mask & SDL_BUTTON(SDL_BUTTON_X1)) state |= 1 << 3;
	if (button_mask & SDL_BUTTON(SDL_BUTTON_X2)) state |= 1 << 4;

	last_button_state = state;
	return state;
}

void OS_SDL::process_events() {
	SDL_Event event;

	do_mouse_warp = false;
	// bool mouse_mode_grab = mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED;
	Size2i window_size;
	SDL_Scancode current_scancode;
	bool current_echo = false;
	SDL_bool text_edit_mode = SDL_IsTextInputActive();
	Vector<String> dropped_files;

	while (SDL_PollEvent(&event)) 
	{
		if (event.type == SDL_QUIT ) 
		{
			if(OS::get_singleton()->is_stdout_verbose())
				print_line("SDL_QUIT");
			force_quit = true;
			main_loop->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
			break;
		}

		if (event.type == SDL_WINDOWEVENT) {
			if(OS::get_singleton()->is_stdout_verbose())
			{
				OS::get_singleton()->print("SDL_WINDOWEVENT: ");
				//print_line("SDL_WINDOWEVENT");
			}
			switch (event.window.event) {
				case SDL_WINDOWEVENT_EXPOSED:
					if(OS::get_singleton()->is_stdout_verbose())
						OS::get_singleton()->print("SDL_WINDOWEVENT_EXPOSED\n");
					if ( main_loop )
						main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
					Main::force_redraw();
					break;
				case SDL_WINDOWEVENT_MINIMIZED:
					if(OS::get_singleton()->is_stdout_verbose())
						OS::get_singleton()->print("SDL_WINDOWEVENT_MINIMIZED\n");
					if ( main_loop )
						main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
					minimized = true;
					break;
				case SDL_WINDOWEVENT_LEAVE:
					if(OS::get_singleton()->is_stdout_verbose())
						OS::get_singleton()->print("SDL_WINDOWEVENT_LEAVE\n");
					if (main_loop /*&& !mouse_mode_grab*/)
					{
						main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_EXIT);
						main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
					}
					// if (input)
						// input->set_mouse_in_window(false);
					break;
				case SDL_WINDOWEVENT_ENTER:
					if(OS::get_singleton()->is_stdout_verbose())
						OS::get_singleton()->print("SDL_WINDOWEVENT_ENTER\n");
					if (main_loop /*&& !mouse_mode_grab*/)
						main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_ENTER);
					// if (input)
						// input->set_mouse_in_window(true);
					break;
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					if(OS::get_singleton()->is_stdout_verbose())
						OS::get_singleton()->print("SDL_WINDOWEVENT_FOCUS_GAINED\n");
					minimized = false;
					window_has_focus = true;
					main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
					// FIXME: Mot sure if we should handle the mouse grabbing manually or if SDL will handle it. Test.
					break;
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					if(OS::get_singleton()->is_stdout_verbose())
						OS::get_singleton()->print("SDL_WINDOWEVENT_SIZE_CHANGED\n");
					window_size = get_window_size();
					if(OS::get_singleton()->is_stdout_verbose())
					{
						OS::get_singleton()->print("Old size: %ix%i\n", current_videomode.width, current_videomode.height);
						OS::get_singleton()->print("New size: %ix%i\n", window_size.x, window_size.y);
					}
					current_videomode.width = window_size.x;
					current_videomode.height = window_size.y;
					break;
				case SDL_WINDOWEVENT_CLOSE:
					// force_quit = true;
					main_loop->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
					break;
			}

			continue; // Probably not a good pattern, but I'm not a fan of else-if in this case.
		}

		if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) 
		{
			// if(OS::get_singleton()->is_stdout_verbose())
			// 	print_line("SDL_MOUESBUTTONDOWN/UP");
			/* exit in case of a mouse button press */
			last_timestamp = event.button.timestamp;
			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				event.button.x = last_mouse_pos.x;
				event.button.y = last_mouse_pos.y;
			}

			// InputEvent 
			InputEvent ievent;
			ievent.ID = ++event_id;
			ievent.device = 0;
			ievent.type = InputEvent::MOUSE_BUTTON;

			// Ref<InputEventMouseButton> sc;
			// sc.instance();

			ievent.mouse_button.mod = get_key_modifier_state(0);
			// sc->
			// sc->
			ievent.mouse_button.button_mask = get_mouse_button_state(0, true);
			ievent.mouse_button.x = event.button.x;
			ievent.mouse_button.y = event.button.y;
			// ievent.mouse_button.global_position = ievent.mouse_button.position;
			ievent.mouse_button.button_index = event.button.button;

			// Swapping buttons around?
			if (ievent.mouse_button.button_index == 2)
				ievent.mouse_button.button_index = 3;
			else if (ievent.mouse_button.button_index == 3)
				ievent.mouse_button.button_index = 2;

			ievent.mouse_button.pressed = (event.button.state == SDL_PRESSED);

			ievent.mouse_button.doubleclick = (event.button.clicks > 1);

			input->parse_input_event(ievent);
			continue;
		}

		// Ahh, good ol' abstractions. :3
		if (event.type == SDL_MOUSEMOTION) {
			// if(OS::get_singleton()->is_stdout_verbose())
			// 	print_line("SDL_MOUSEMOTION");
			last_timestamp = event.motion.timestamp;

			InputEvent ievent;
			ievent.ID = ++event_id;
			ievent.device = 0;
			ievent.type = InputEvent::MOUSE_MOTION;
			// Motion is also simple.
			// A little hack is in order
			// to be able to send relative motion events.
			Point2i pos(event.motion.x, event.motion.y);

			// TODO: Handle mouse warp. Is this needed in SDL?

			Point2i rel(event.motion.xrel, event.motion.yrel);

			// Ref<InputEventMouseMotion> mm;
			// mm.instance();

			ievent.mouse_motion.mod = get_key_modifier_state(0);
			ievent.mouse_motion.button_mask =get_mouse_button_state(event.motion.state, false);
			ievent.mouse_motion.x = pos.x;
			ievent.mouse_motion.y = pos.y;
			// ievent.mouse_motion.set_global_position(pos);
			input->set_mouse_pos(pos);

			// ievent.mouse_motion.speed = input->get_last_mouse_speed();
			// ievent.mouse_motion.relative = rel;

			last_mouse_pos = pos;

			// Don't propagate the motion event unless we have focus
			// this is so that the relative motion doesn't get messed up
			// after we regain focus.
			// FIXME: Not sure if we need this or not.
			if (window_has_focus /*|| !mouse_mode_grab*/)
				input->parse_input_event(ievent);

			continue;
		}
#if defined(TOUCH_ENABLED)
		if( event.type ==  SDL_FINGERDOWN || event.type == SDL_FINGERUP )
		{
			// if(OS::get_singleton()->is_stdout_verbose())
			// 	print_line("SDL_FINGERDOW | SDL_FINGERUP");
			InputEvent input_event;
			input_event.ID = ++event_id;
			input_event.device = 0;

			InputEvent mouse_event;
			mouse_event.ID = ++event_id;
			mouse_event.device = 0;

			int index = (int)event.tfinger.fingerId;
			Point2i pos = Point2i(event.tfinger.x, event.tfinger.y);

			bool is_begin = event.type ==  SDL_FINGERDOWN;

			bool translate = false;
			if (is_begin) {
				++num_touches;
				if (num_touches == 1) {
					touch_mouse_index = index;
					translate = true;
				}
			} else {
				--num_touches;
				if (num_touches == 0) {
					translate = true;
				} else if (num_touches < 0) { // Defensive
					num_touches = 0;
				}
				touch_mouse_index = -1;
			}

			input_event.type = InputEvent::SCREEN_TOUCH;
			input_event.screen_touch.index = index;
			input_event.screen_touch.x = pos.x;
			input_event.screen_touch.y = pos.y;
			input_event.screen_touch.pressed = is_begin;

			if (translate) {
				mouse_event.type = InputEvent::MOUSE_BUTTON;
				mouse_event.mouse_button.x = pos.x;
				mouse_event.mouse_button.y = pos.y;
				mouse_event.mouse_button.global_x = pos.x;
				mouse_event.mouse_button.global_y = pos.y;
				input->set_mouse_pos(pos);
				mouse_event.mouse_button.button_index = 1;
				mouse_event.mouse_button.pressed = is_begin;
				last_mouse_pos = pos;
			}

			if (is_begin) {
				if (touch.state.has(index)) // Defensive
					break;
				touch.state[index] = pos;
				input->parse_input_event(input_event);
				input->parse_input_event(mouse_event);
			} else {
				if (!touch.state.has(index)) // Defensive
					break;
				touch.state.erase(index);
				input->parse_input_event(input_event);
				input->parse_input_event(mouse_event);
			}
		}

		if( event.type ==  SDL_FINGERMOTION )
		{
			// if(OS::get_singleton()->is_stdout_verbose())
				// print_line("SDL_FINGERMOTION");

			InputEvent input_event;
			input_event.ID = ++event_id;
			input_event.device = 0;

			InputEvent mouse_event;
			mouse_event.ID = ++event_id;
			mouse_event.device = 0;

			int index = (int)event.tfinger.fingerId;
			Point2i pos = Point2i(event.tfinger.x, event.tfinger.y);

			Map<int, Vector2>::Element *curr_pos_elem = touch.state.find(index);
			if (!curr_pos_elem) 
			// {// Defensive
				//if( OS::get_singleton()->is_stdout_verbose() )
				//	print_line("Cant drag!");
				break;
			// }

			if (curr_pos_elem->value() != pos) 
			{
				input_event.type = InputEvent::SCREEN_DRAG;
				input_event.screen_drag.index = index;
				input_event.screen_drag.x = pos.x;
				input_event.screen_drag.y = pos.y;
				input_event.screen_drag.relative_x = pos.x - curr_pos_elem->value().x;
				input_event.screen_drag.relative_y = pos.y - curr_pos_elem->value().y;
				input->parse_input_event(input_event);

				if (index == touch_mouse_index) 
				{
					mouse_event.type = InputEvent::MOUSE_MOTION;
					mouse_event.mouse_motion.x = pos.x;
					mouse_event.mouse_motion.y = pos.y;
					mouse_event.mouse_motion.global_x = pos.x;
					mouse_event.mouse_motion.global_y = pos.y;
					input->set_mouse_pos(pos);
					mouse_event.mouse_motion.relative_x = pos.x - last_mouse_pos.x;
					mouse_event.mouse_motion.relative_y = pos.y - last_mouse_pos.y;
					last_mouse_pos = pos;
					input->parse_input_event(mouse_event);
				}

				curr_pos_elem->value() = pos;
			}
		}
#endif
	// 	// if (event.type == SDL_MOUSEWHEEL) {
	// 	// 	last_timestamp = event.wheel.timestamp;

	// 	// 	uint32_t dir = event.wheel.direction;
	// 	// 	int32_t amount_x = event.wheel.x;
	// 	// 	int32_t amount_y = event.wheel.y;
	// 	// 	int position_x = 0;
	// 	// 	int position_y = 0;
	// 	// 	ButtonList button;

	// 	// 	if (dir == SDL_MOUSEWHEEL_FLIPPED) {
	// 	// 		amount_x *= -1;
	// 	// 		amount_y *= -1;
	// 	// 	}

	// 	// 	if (amount_y < 0)
	// 	// 		button = BUTTON_WHEEL_DOWN;
	// 	// 	else if (amount_y > 0)
	// 	// 		button = BUTTON_WHEEL_UP;
	// 	// 	else if (amount_x < 0)
	// 	// 		button = BUTTON_WHEEL_RIGHT;
	// 	// 	else if (amount_x > 0)
	// 	// 		button = BUTTON_WHEEL_LEFT;

	// 	// 	uint32_t button_state = SDL_GetMouseState(&position_x, &position_y);

	// 	// 	Ref<InputEventMouseButton> sc;
	// 	// 	sc.instance();

	// 	// 	get_key_modifier_state(sc);
	// 	// 	sc->set_button_mask(get_mouse_button_state(button_state, false));
	// 	// 	sc->set_position(Vector2(position_x, position_y));
	// 	// 	sc->set_global_position(sc->get_position());
	// 	// 	sc->set_button_index(button);
	// 	// 	sc->set_pressed(true);
	// 	// 	input->parse_input_event(sc);
	// 	// 	sc->set_pressed(false);
	// 	// 	input->parse_input_event(sc);

	// 	// 	continue;
	// 	// }

		// Outside of text input mode. Events created here won't have unicode mappings.
		if (event.type == SDL_KEYDOWN & text_edit_mode == SDL_FALSE) 
		{
			if(OS::get_singleton()->is_stdout_verbose())
				print_line("SDL_KEYDOWN");
			last_timestamp = event.key.timestamp;
			SDL_Keysym keysym = event.key.keysym;
			SDL_Scancode scancode = keysym.scancode;
			SDL_Keycode keycode = keysym.sym;

			InputEvent ievent;
			ievent.ID = ++event_id;
			ievent.device = 0;
			ievent.type = InputEvent::KEY;
			// Ref<InputEventKey> k;

			// k.instance();
			if (keycode == 0) {
				continue;
			}

			ievent.key.mod = get_key_modifier_state(0);
			unsigned int non_printable_keycode = KeyMappingSDL::get_non_printable_keycode(keycode);

			ievent.key.pressed = (event.key.state == SDL_PRESSED);
			ievent.key.echo = (event.key.repeat > 0);

			// Not quite sure how we should handle this to be honest.
			if (non_printable_keycode != 0) {
				ievent.key.scancode = (non_printable_keycode);
			} else {
				ievent.key.scancode = (scancode);
			}
			
			input->parse_input_event(ievent);
			continue;
			// If we're in text input mode.
		} 
		else if (text_edit_mode == SDL_TRUE) {
		 	SDL_Keysym keysym = event.key.keysym;
		 	SDL_Keycode keycode = keysym.sym;

		 	unsigned int non_printable_keycode = KeyMappingSDL::get_non_printable_keycode(keycode);

		 	// If a modifier / non-printable key is hit, handle that directly
		 	if (non_printable_keycode != 0) {
		 		//Ref<InputEventKey> k;
		 		//k.instance();
				InputEvent input_event;
				input_event.ID = ++event_id;
				input_event.device = 0;
				input_event.type = InputEvent::KEY;

		 		input_event.key.mod = get_key_modifier_state(0);
		 		input_event.key.pressed = (event.key.state == SDL_PRESSED);
		 		input_event.key.scancode = (non_printable_keycode);
		 		input_event.key.echo = (event.key.repeat > 0);
		 		//TODO old godot API style
		 		input->parse_input_event(input_event);
		 		continue;
		 		// Otherwise wait until TextInput events to emit the key event with unicode.
		 	} else {
		 		current_scancode = keysym.scancode;
		 		current_echo = event.key.repeat > 0;
		 	}
		 }

		 if (event.type == SDL_TEXTINPUT && text_edit_mode == SDL_TRUE) {
		 	last_timestamp = event.text.timestamp;

		 	String tmp;
		 	tmp.parse_utf8(event.text.text);
		 	for (int i = 0; i < tmp.length(); i++) {
		 		if (tmp[i] == 0) continue;

		 		InputEvent input_event;
				input_event.ID = ++event_id;
				input_event.device = 0;
				input_event.type = InputEvent::KEY;
		 		input_event.key.mod = get_key_modifier_state(0);
		 		input_event.key.unicode = tmp[i];
		 		input_event.key.pressed =true;
		 		if (current_scancode) 
					input_event.key.scancode = current_scancode;
		 		input_event.key.echo =current_echo;

		 		input->parse_input_event(input_event);
		 		continue;
		 	}
		 }

		if (event.type == SDL_KEYUP) {
			if(OS::get_singleton()->is_stdout_verbose())
				print_line("SDL_KEYUP");
			last_timestamp = event.key.timestamp;
			SDL_Keysym keysym = event.key.keysym;
			SDL_Scancode scancode = keysym.scancode;
			SDL_Keycode keycode = keysym.sym;

			InputEvent ievent;
			ievent.ID = ++event_id;
			ievent.device = 0;
			ievent.type = InputEvent::KEY;

			// Ref<InputEventKey> k;
			// k.instance();
			if (keycode == 0) {
				continue;
			}

			ievent.key.mod = get_key_modifier_state(0);

			ievent.key.unicode = (keycode);
			ievent.key.pressed = (event.key.state == SDL_PRESSED);
			ievent.key.scancode = (scancode);
			ievent.key.echo = (event.key.repeat > 0);

			// ievent.key = *k;
			input->parse_input_event(ievent);
			continue;
		}

	// 	// if (event.type == SDL_DROPFILE) {

	// 	// 	dropped_files.push_back(event.drop.file);
	// 	// 	continue;
	// 	// }

	// 	// // Allow dragging and dropping text into text inputs.
	// 	// if (event.type == SDL_DROPTEXT) {
	// 	// 	last_timestamp = event.text.timestamp;

	// 	// 	String tmp;
	// 	// 	tmp.parse_utf8(event.drop.file);
	// 	// 	for (int i = 0; i < tmp.length(); i++) {
	// 	// 		if (tmp[i] == 0) continue;

	// 	// 		Ref<InputEventKey> k;
	// 	// 		k.instance();
	// 	// 		get_key_modifier_state(k);
	// 	// 		k->set_unicode(tmp[i]);
	// 	// 		k->set_pressed(true);
	// 	// 		k->set_echo(false);

	// 	// 		input->parse_input_event(k);
	// 	// 	}

	// 	// 	continue;
	// 	// }
	}

	if (do_mouse_warp) {
		// Handle mouse warp here if needed. Not sure.
	}

	if (dropped_files.size() > 0) {
		main_loop->drop_files(dropped_files);
	}
}

MainLoop *OS_SDL::get_main_loop() const {
	return main_loop;
}

void OS_SDL::delete_main_loop() {
	if (main_loop)
		memdelete(main_loop);
	main_loop = NULL;
}

void OS_SDL::set_main_loop(MainLoop *p_main_loop) {

	main_loop = p_main_loop;
	input->set_main_loop(p_main_loop);
}

bool OS_SDL::can_draw() const {
	return !minimized;
};

void OS_SDL::set_clipboard(const String &p_text) {

	OS::set_clipboard(p_text);

	SDL_SetClipboardText((const char *)p_text.utf8().get_data());
};

String OS_SDL::get_clipboard() const {
	// StringBuilder *sb = new StringBuilder();
	// sb->append(SDL_GetClipboardText());

	// return sb->as_string();
}

String OS_SDL::get_name() {
	return "SDL";
}

Error OS_SDL::shell_open(String p_uri) {

	Error ok;
	List<String> args;
	args.push_back(p_uri);
	ok = execute("xdg-open", args, false);
	if (ok == OK)
		return OK;
	ok = execute("gnome-open", args, false);
	if (ok == OK)
		return OK;
	ok = execute("kde-open", args, false);
	return ok;
}

bool OS_SDL::_check_internal_feature_support(const String &p_feature) {

	return p_feature == "pc" || p_feature == "s3tc";
}

String OS_SDL::get_config_path() const {

	if (has_environment("XDG_CONFIG_HOME")) {
		return get_environment("XDG_CONFIG_HOME");
	} else if (has_environment("HOME")) {
		return get_environment("HOME").plus_file(".config");
	} else {
		return ".";
	}
}

String OS_SDL::get_data_path() const {

	if (has_environment("XDG_DATA_HOME")) {
		return get_environment("XDG_DATA_HOME");
	} else if (has_environment("HOME")) {
		return get_environment("HOME").plus_file(".local/share");
	} else {
		return get_config_path();
	}
}

String OS_SDL::get_cache_path() const {

	if (has_environment("XDG_CACHE_HOME")) {
		return get_environment("XDG_CACHE_HOME");
	} else if (has_environment("HOME")) {
		return get_environment("HOME").plus_file(".cache");
	} else {
		return get_config_path();
	}
}

String OS_SDL::get_system_dir(SystemDir p_dir) const {

	String xdgparam;

	switch (p_dir) {
		case SYSTEM_DIR_DESKTOP: {

			xdgparam = "DESKTOP";
		} break;
		case SYSTEM_DIR_DCIM: {

			xdgparam = "PICTURES";

		} break;
		case SYSTEM_DIR_DOCUMENTS: {

			xdgparam = "DOCUMENTS";

		} break;
		case SYSTEM_DIR_DOWNLOADS: {

			xdgparam = "DOWNLOAD";

		} break;
		case SYSTEM_DIR_MOVIES: {

			xdgparam = "VIDEOS";

		} break;
		case SYSTEM_DIR_MUSIC: {

			xdgparam = "MUSIC";

		} break;
		case SYSTEM_DIR_PICTURES: {

			xdgparam = "PICTURES";

		} break;
		case SYSTEM_DIR_RINGTONES: {

			xdgparam = "MUSIC";

		} break;
	}

	String pipe;
	List<String> arg;
	arg.push_back(xdgparam);
	Error err = const_cast<OS_SDL *>(this)->execute("xdg-user-dir", arg, true, NULL, &pipe);
	if (err != OK)
		return ".";
	return pipe.strip_edges();
}

void OS_SDL::move_window_to_foreground() {
	SDL_RaiseWindow(sdl_window);
}

void OS_SDL::set_cursor_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	if (p_shape == current_cursor) return;

	SDL_Cursor *sdl_cursor;

	if (cursors[p_shape] == NULL) {
		SDL_SystemCursor sdl_cursor_id;
		// FIXME: SDL only supports a subset of system cursors, perhaps we should figure out how to load them ourselves?
		switch (p_shape) {
			case CURSOR_ARROW:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_ARROW;
				break;
			case CURSOR_IBEAM:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_IBEAM;
				break;
			case CURSOR_POINTING_HAND:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_HAND;
				break;
			case CURSOR_CROSS:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_CROSSHAIR;
				break;
			case CURSOR_WAIT:
			case CURSOR_BUSY:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_WAIT;
				break;
			case CURSOR_DRAG:
			case CURSOR_CAN_DROP:
			case CURSOR_MOVE:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_HAND;
				break;
			case CURSOR_FORBIDDEN:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_NO;
				break;
			case CURSOR_VSIZE:
			case CURSOR_VSPLIT:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_SIZENS;
				break;
			case CURSOR_HSIZE:
			case CURSOR_HSPLIT:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_SIZEWE;
				break;
			case CURSOR_BDIAGSIZE:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_SIZENWSE;
				break;
			case CURSOR_FDIAGSIZE:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_SIZENESW;
				break;
			default:
				sdl_cursor_id = SDL_SYSTEM_CURSOR_ARROW;
		}

		cursors[p_shape] = SDL_CreateSystemCursor(sdl_cursor_id);
	}

	sdl_cursor = cursors[p_shape];
	current_cursor = p_shape;

	SDL_SetCursor(sdl_cursor);
}

// TODO: Not sure if this works..
void OS_SDL::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	// if (p_cursor.is_valid()) {
	// 	Ref<Texture> texture = p_cursor;
	// 	Ref<Image> img = texture->get_data()->duplicate();
	// 	img->convert(Image::FORMAT_RGBA8);

	// 	int w = img->get_width();
	// 	int h = img->get_height();

	// 	// FIXME: Should this fail with SDL?
	// 	ERR_FAIL_COND(w != 32 || h != 32);

	// 	PoolVector<uint8_t>::Read r = img->get_data().read();

	// 	uint8_t *pr = const_cast<uint8_t *>(r.ptr());

	// 	SDL_Surface *cursor_surface = SDL_CreateRGBSurfaceFrom(pr, w, h, 32, w * 4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	// 	if (cursor_surface == NULL) {
	// 		fprintf(stderr, "Creating window icon surface failed: %s", SDL_GetError());
	// 	}

	// 	SDL_Cursor *prev_cursor = cursors[p_shape];
	// 	cursors[p_shape] = SDL_CreateColorCursor(cursor_surface, p_hotspot.x, p_hotspot.y);
	// 	SDL_SetCursor(cursors[p_shape]);
	// 	// Don't free the previous cursor set for this shape until it is no longer in use.
	// 	if (prev_cursor) SDL_FreeCursor(cursors[p_shape]);

	// 	// FIXME: I can't tell from the SDL docs whether or not the surface should be freed after setting the cursor or not.
	// 	// I assume yes based on the implementation of SetWindowIcon, but this should be tested.
	// 	SDL_FreeSurface(cursor_surface);
	// }
}

void OS_SDL::release_rendering_thread() {

	context_gl->release_current();
}

void OS_SDL::make_rendering_thread() {

	context_gl->make_current();
}

void OS_SDL::swap_buffers() {

	context_gl->swap_buffers();
}

bool OS_SDL::has_touchscreen_ui_hint() const {
	return true;
}

void OS_SDL::alert(const String &p_alert, const String &p_title) {

	const char *alert = p_alert.utf8().get_data();
	const char *title = p_title.utf8().get_data();

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, alert, sdl_window);
}

void OS_SDL::set_icon(const Ref<Image> &p_icon) {

	// if (p_icon.is_valid()) {
	// 	Ref<Image> img = p_icon->duplicate();
	// 	img->convert(Image::FORMAT_RGBA8);

	// 	int w = img->get_width();
	// 	int h = img->get_height();

	// 	PoolVector<uint8_t>::Read r = img->get_data().read();

	// 	uint8_t *pr = const_cast<uint8_t *>(r.ptr());

	// 	SDL_Surface *icon_surface = SDL_CreateRGBSurfaceFrom(pr, w, h, 32, w * 4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	// 	if (icon_surface == NULL) {
	// 		fprintf(stderr, "Creating window icon surface failed: %s", SDL_GetError());
	// 	}

	// 	SDL_SetWindowIcon(sdl_window, icon_surface);
	// 	SDL_FreeSurface(icon_surface);

	// } else {
	// 	// FIXME: Does this actually work? Test.
	// 	SDL_SetWindowIcon(sdl_window, NULL);
	// }
}

void OS_SDL::force_process_input() {
	process_events(); // get rid of pending events
#ifdef JOYDEV_ENABLED
	joypad->process_joypads();
#endif
}

void OS_SDL::run() {

	force_quit = false;

	if (!main_loop)
		return;

	main_loop->init();

	//uint64_t last_ticks=get_ticks_usec();

	//int frames=0;
	//uint64_t frame=0;

	while (!force_quit) {

		process_events(); // get rid of pending events
#ifdef JOYDEV_ENABLED
		joypad->process_joypads();
#endif
		if (Main::iteration() == true)
			break;
	};

	main_loop->finish();
}

bool OS_SDL::is_joy_known(int p_device) {
	return input->is_joy_mapped(p_device);
}

String OS_SDL::get_joy_guid(int p_device) const {
	return input->get_joy_guid_remapped(p_device);
}

void OS_SDL::_set_use_vsync(bool p_enable) {
	if (context_gl)
		return context_gl->set_use_vsync(p_enable);
}
/*
bool OS_SDL::is_vsync_enabled() const {

	if (context_gl)
		return context_gl->is_using_vsync();

	return true;
}
*/

// OS::PowerState OS_SDL::get_power_state() {
// 	return power_manager->get_power_state();
// }

int OS_SDL::get_power_seconds_left() {
	return power_manager->get_power_seconds_left();
}

int OS_SDL::get_power_percent_left() {
	return power_manager->get_power_percent_left();
}

void OS_SDL::disable_crash_handler() {
	crash_handler.disable();
}

bool OS_SDL::is_disable_crash_handler() const {
	return crash_handler.is_disabled();
}

static String get_mountpoint(const String &p_path) {
	struct stat s;
	if (stat(p_path.utf8().get_data(), &s)) {
		return "";
	}

#ifdef HAVE_MNTENT
	dev_t dev = s.st_dev;
	FILE *fd = setmntent("/proc/mounts", "r");
	if (!fd) {
		return "";
	}

	struct mntent mnt;
	char buf[1024];
	size_t buflen = 1024;
	while (getmntent_r(fd, &mnt, buf, buflen)) {
		if (!stat(mnt.mnt_dir, &s) && s.st_dev == dev) {
			endmntent(fd);
			return String(mnt.mnt_dir);
		}
	}

	endmntent(fd);
#endif
	return "";
}

Error OS_SDL::move_to_trash(const String &p_path) {
	String trashcan = "";
	String mnt = get_mountpoint(p_path);

	if (mnt != "") {
		String path(mnt + "/.Trash-" + itos(getuid()) + "/files");
		struct stat s;
		if (!stat(path.utf8().get_data(), &s)) {
			trashcan = path;
		}
	}

	if (trashcan == "") {
		char *dhome = getenv("XDG_DATA_HOME");
		if (dhome) {
			trashcan = String(dhome) + "/Trash/files";
		}
	}

	if (trashcan == "") {
		char *home = getenv("HOME");
		if (home) {
			trashcan = String(home) + "/.local/share/Trash/files";
		}
	}

	if (trashcan == "") {
		ERR_PRINTS("move_to_trash: Could not determine trashcan location");
		return FAILED;
	}

	List<String> args;
	args.push_back("-p");
	args.push_back(trashcan);
	Error err = execute("mkdir", args, true);
	if (err == OK) {
		List<String> args2;
		args2.push_back(p_path);
		args2.push_back(trashcan);
		err = execute("mv", args2, true);
	}

	return err;
}

// This could probably be done in a less eye-offending fashion.
OS::LatinKeyboardVariant OS_SDL::get_latin_keyboard_variant() const {
	SDL_Keycode keysFromScanCodes[6] = {
		SDL_GetKeyFromScancode(SDL_SCANCODE_Q),
		SDL_GetKeyFromScancode(SDL_SCANCODE_W),
		SDL_GetKeyFromScancode(SDL_SCANCODE_E),
		SDL_GetKeyFromScancode(SDL_SCANCODE_R),
		SDL_GetKeyFromScancode(SDL_SCANCODE_T),
		SDL_GetKeyFromScancode(SDL_SCANCODE_Y)
	};

	if (
			keysFromScanCodes[0] == SDLK_q &&
			keysFromScanCodes[1] == SDLK_w &&
			keysFromScanCodes[2] == SDLK_e &&
			keysFromScanCodes[3] == SDLK_r &&
			keysFromScanCodes[4] == SDLK_t &&
			keysFromScanCodes[5] == SDLK_z) return LATIN_KEYBOARD_QWERTZ;

	if (
			keysFromScanCodes[0] == SDLK_a &&
			keysFromScanCodes[1] == SDLK_z &&
			keysFromScanCodes[2] == SDLK_e &&
			keysFromScanCodes[3] == SDLK_r &&
			keysFromScanCodes[4] == SDLK_t &&
			keysFromScanCodes[5] == SDLK_y) return LATIN_KEYBOARD_AZERTY;

	if (
			keysFromScanCodes[0] == SDLK_QUOTE &&
			keysFromScanCodes[1] == SDLK_COMMA &&
			keysFromScanCodes[2] == SDLK_PERIOD &&
			keysFromScanCodes[3] == SDLK_p &&
			keysFromScanCodes[4] == SDLK_y &&
			keysFromScanCodes[5] == SDLK_f) return LATIN_KEYBOARD_DVORAK;

	if (
			keysFromScanCodes[0] == SDLK_x &&
			keysFromScanCodes[1] == SDLK_v &&
			keysFromScanCodes[2] == SDLK_l &&
			keysFromScanCodes[3] == SDLK_c &&
			keysFromScanCodes[4] == SDLK_w &&
			keysFromScanCodes[5] == SDLK_k) return LATIN_KEYBOARD_NEO;

	if (
			keysFromScanCodes[0] == SDLK_q &&
			keysFromScanCodes[1] == SDLK_w &&
			keysFromScanCodes[2] == SDLK_f &&
			keysFromScanCodes[3] == SDLK_p &&
			keysFromScanCodes[4] == SDLK_g &&
			keysFromScanCodes[5] == SDLK_j) return LATIN_KEYBOARD_COLEMAK;

	return LATIN_KEYBOARD_QWERTY;
}

OS_SDL::OS_SDL() 
{
#ifdef PULSEAUDIO_ENABLED
	AudioDriverManagerSW::add_driver(&driver_pulseaudio);
#endif

// #ifdef ALSA_ENABLED
// 	AudioDriverManagerSW::add_driver(&driver_alsa);
// #endif

	if ( AudioDriverManagerSW::get_driver_count() == 0 ) 
	{
		WARN_PRINT("Nosound driver found... set default dummy audio driver. ");
		AudioDriverManagerSW::add_driver(&driver_dummy);
	}

	minimized = false;
	// xim_style = 0L;
	event_id = 0;
	num_touches = 0;
	mouse_mode = MOUSE_MODE_VISIBLE;
	is_audio_resource_acquired = false;
	audio_resource = NULL;
}

void OS_SDL::start_audio_driver() {
	//if (AudioDriverManager::get_driver(0)->init() != OK) {
		//ERR_PRINT("Initializing audio failed.");
	//}
	if ( AudioDriverManagerSW::get_driver(0)->init() != OK) {
		ERR_PRINT("Initializing audio failed.");
	}
}

void OS_SDL::stop_audio_driver() {
	for (int i = 0; i < get_audio_driver_count(); i++) {
		AudioDriverManagerSW::get_driver(i)->finish();
	}
}

static void on_audio_resource_acquired(audioresource_t* audio_resource, bool acquired, void* user_data) 
{
	//AudioDriver* driver = (AudioDriver*) user_data;
	OS_SDL* os = (OS_SDL*) user_data;

	if (acquired) {
		print_line("starting audio driver");
		// start playback
		os->is_audio_resource_acquired = true;
		os->start_audio_driver();
	} else {
		print_line("stopping audio driver");
		// stop playback
		os->stop_audio_driver();
	}
}
