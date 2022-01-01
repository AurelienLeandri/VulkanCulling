#pragma once

#include "ModelLoader.h"

#include <memory>
#include <exception>

namespace leoscene {
	class Scene;
	class Camera;

	class SceneLoaderException : public std::exception {
	public:
		SceneLoaderException(const char* message, size_t lineNb);
		virtual const char* what() const noexcept;
	private:
		const char* message;
		size_t lineNb;
	};

	class SceneLoader {
	public:
		void loadScene(const char* filePath, Scene* scene, Camera* camera);
		
	private:
		void _loadModelEntry(
			std::stringstream& entry,
			std::unordered_map<std::string, Model>& models,
			const std::unordered_map<std::string, std::shared_ptr<const Transform>>& transforms,
			const std::string& fileDirectoryPath,
			size_t lineNb);

	private:
		ModelLoader _modelLoader;
	};
}
