/** 
 * Made by Sashikknox
*/

#include "main/main.h"
#include "os_sdl.h"

int main(int argc, char *argv[]) {
	OS_SDL os;

	Error error = Main::setup(argv[0], argc - 1, &argv[1]);
	if (error != OK) {
		return 255;
	}

	if (Main::start()) {
		os.run();
	}

	Main::cleanup();

	return os.get_exit_code();
}