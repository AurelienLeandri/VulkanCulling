#pragma once

namespace leo {
	class Scene;

	class SceneLoader {
	public:
		static bool loadScene(const char* fileName, Scene& scene);
	};
}
