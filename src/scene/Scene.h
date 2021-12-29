#pragma once

#include "Scene.h"
#include "SceneObject.h"

#include <vector>

namespace leoscene {
	class Light;

	class Scene {
	public:
		std::vector<SceneObject> objects;
		std::vector<std::shared_ptr<Light>> lights;
	};
}
