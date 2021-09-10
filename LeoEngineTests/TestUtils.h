#pragma once

#include <exception>

class TestFailedException : public std::exception {
	public:
	TestFailedException(const char* message);
	virtual const char* what() const noexcept;
private:
	const char* message;
};

void test(bool condition, const char* errorMessage);
void testClose(float value0, float value1, const char* message, float e = 0.00001f);
