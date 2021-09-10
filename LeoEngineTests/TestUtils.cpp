#include "TestUtils.h"

TestFailedException::TestFailedException(const char* message) : message(message) {
}

const char* TestFailedException::what() const noexcept {
	return message;
}

void test(bool condition, const char* errorMessage) {
	if (!condition) {
		throw TestFailedException(errorMessage);
	}
}

void testClose(float value0, float value1, const char* message, float e) {
	if (value0 - value1 > e) {
		throw TestFailedException(message);
	}
}