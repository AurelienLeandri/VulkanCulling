#pragma once

#include <vector>

void Sponza();

static const std::vector<void(*)()> sceneTests = {
	Sponza
};
