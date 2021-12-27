#include "Application.h"

#include <iostream>

int main() {
	Application application;

	std::cout << "Initializing application" << std::endl;
	if (application.init()) {
		std::cerr << "Error. Application failed to initialize. Exiting." << std::endl;
		return 2;
	}

	std::cout << "Loading scene" << std::endl;
	//const char* scenePath = "resources/models/Sponza/Sponza.scene";
	const char* scenePath = "resources/models/Sponza/super_sponza.scene";
	//const char* scenePath = "resources/models/Monkey/monkey.scene";
	//const char* scenePath = "resources/models/Monkey/super_monkey.scene";
	//const char* scenePath = "resources/models/Monkey/monkey_occluded.scene";
	//const char* scenePath = "resources/models/sphere_scene.scene";
	if (application.loadScene(scenePath)) {
		std::cerr << "Error: Scene loading failed. Exiting." << std::endl;
		return 1;
	}

	std::cout << "Starting application" << std::endl;
	if (application.start()) {
		std::cerr << "Error while running the application. Exiting." << std::endl;
		return 3;
	}

	return 0;
}