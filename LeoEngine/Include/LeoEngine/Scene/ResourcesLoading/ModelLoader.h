#pragma once

#include <unordered_map>
#include <memory>

namespace leo {
	struct SceneObject;
	class Transform;
	class Material;

	struct Model {
		std::vector<SceneObject> objects;
	};

	class ModelLoader {
	public:
		struct LoadingOptions {
			std::shared_ptr<const Transform> globalTransform = std::make_shared<Transform>();
		};

	public:
		static const Model loadModel(const char* filePath, LoadingOptions options = {});
		static const Model loadSphereModel(uint32_t xSegments, uint32_t ySegments, LoadingOptions options = {});

	private:
		static std::unordered_map<std::string, Model> _modelsCache;
		static std::unordered_map<uint32_t, std::unordered_map<uint32_t, Model>> _spheresCache;
		static const std::shared_ptr<Material> _defaultMaterial;
	};
}
