#pragma once

#include "TextureLoader.h"

#include <assimp/scene.h>

#include <unordered_map>
#include <memory>

namespace leoscene {
	struct SceneObject;
	class Transform;
	class Material;
	class Mesh;

	struct Model {
		std::vector<SceneObject> objects;
	};

	class ModelLoader {
	public:
		struct LoadingOptions {
			std::shared_ptr<const Transform> globalTransform = std::make_shared<Transform>();
		};

	public:
		ModelLoader();

	public:
		const Model loadModel(const char* filePath, LoadingOptions options = {});
		const Model loadSphereModel(uint32_t xSegments, uint32_t ySegments, LoadingOptions options = {});

	private:
		void _processNode(
			aiNode* node,
			const aiScene* aiScene,
			const std::string& fileDirectoryPath,
			std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& modelMaterials,
			std::unordered_map<aiMesh*, std::shared_ptr<Mesh>>& modelMeshes,
			std::vector<SceneObject>& sceneObjects,
			aiMatrix4x4 transform);

		void _processMesh(
			aiMesh* assimpMesh,
			const aiScene* aiScene,
			const std::string& fileDirectoryPath,
			std::unordered_map<aiMaterial*, std::shared_ptr<Material>>& model_materials,
			std::unordered_map<aiMesh*, std::shared_ptr<Mesh>>& modelMeshes,
			std::vector<SceneObject>& sceneObjects,
			aiMatrix4x4 transform);

		std::shared_ptr<Material> _loadMaterial(aiMaterial* assimpMaterial, const std::string& fileDirectoryPath);

		std::shared_ptr<ImageTexture> _loadMaterialTexture(
			aiMaterial* assimpMaterial,
			aiTextureType assimpTextureType,
			const std::string& fileDirectoryPath);

	private:
		std::unordered_map<std::string, Model> _modelsCache;
		std::unordered_map<uint32_t, std::unordered_map<uint32_t, Model>> _spheresCache;
		const std::shared_ptr<Material> _defaultMaterial;
		TextureLoader _textureLoader;

	};
}
