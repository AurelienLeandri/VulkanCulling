#include <iostream>

#include "SceneTests.h"
#include "TestUtils.h"

int main() {
	for (auto func : sceneTests) {
		try {
			func();
		}
		catch (TestFailedException e) {
			std::cout << e.what() << std::endl;
		}
	}
}
