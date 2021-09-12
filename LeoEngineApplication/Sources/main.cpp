#include "Application.h"

#include <iostream>

int main() {
	Application application;

	if (application.loadScene("../Resources/Models/Sponza/Sponza.scene")) {
		std::cerr << "Error: Scene loading failed. Exiting." << std::endl;
		return 1;
	}

	if (application.start()) {
		std::cerr << "Error. Application failed to start. Exiting." << std::endl;
		return 2;
	}

	return 0;
}