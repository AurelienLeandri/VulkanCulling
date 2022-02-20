#include "engine/Application.h"

#define _CRTDBG_MAP_ALLOC
#include <stdio.h> 
#include <crtdbg.h>

#include <iostream>

namespace {
	void printUsage();
}

int main(int argc, const char** argv) {
	// TODO: add parameter for choosing which renderer to start the application with
	if (argc > 2) {
		std::cerr << "Error: too many arguments." << std::endl;
		printUsage();
		return 1;
	}

	const char* scenePath = "resources/models/Monkey/super_monkey.scene";
	if (argc == 2) {
		if (!strcmp(argv[1], "--help")) {
			printUsage();
			return 0;
		}
		else {
			scenePath = argv[1];
		}
	}

	{
		Application application;

		std::cout << "Initializing application" << std::endl;

		Application::Options applicationOptions{};
		applicationOptions.startingRenderer = "OpenGLRenderer";

		if (application.init(applicationOptions)) {
			std::cerr << "Error. Application failed to initialize. Exiting." << std::endl;
			return 2;
		}

		std::cout << "Loading scene" << std::endl;
		if (application.loadScene(scenePath)) {
			std::cerr << "Error: Scene loading failed. Exiting." << std::endl;
			return 2;
		}

		std::cout << "Starting application" << std::endl;
		if (application.start()) {
			std::cerr << "Error while running the application. Exiting." << std::endl;
			return 2;
		}

		application.cleanup();
	}

	return 0;
}

namespace {
	void printUsage() {
		std::cout << "Usage:" << std::endl
			<< "\t" << "LeoEngine.exe [my_file.scene]" << "\t" << "Open the scene file with the renderer." << std::endl
			<< "\t" << "LeoEngine.exe --help [...]" << "\t" << "Print this help." << std::endl;
		std::cout << "Notes:" << std::endl
			<< "\t" << "If no scene file is provided, will open \"resources/models/Sponza/super_sponza.scene\"." << std::endl << std::endl;
	}
}
