#pragma once

#include <unordered_map>
#include <memory>

namespace leo {
	class SceneObject;
	class Transform;

	struct Model {
		std::vector<SceneObject> objects;
	};

	class ModelLoader {
	public:
		struct LoadingOptions {
			std::shared_ptr<Transform> globalTransform;
		};

	public:
		static const Model loadModel(const char* filePath, LoadingOptions options = {});

	private:
		static std::unordered_map<std::string, Model> _modelsCache;
	};
}
