#pragma once

#include <unordered_map>
#include <memory>

namespace leo {
	class SceneObject;

	struct Model {
		std::vector<std::shared_ptr<SceneObject>> objects;
	};

	class ModelLoader {
	public:
		static const Model* loadModel(const char* fileName);

	private:
		static std::unordered_map<std::string, Model> _modelsCache;
	};
}
