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
	if (application.loadScene("../Resources/Models/Sponza/Sponza.scene")) {
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