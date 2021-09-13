#pragma once

#include <memory>
#include <exception>

namespace leo {
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
		static void loadScene(const char* filePath, Scene* scene, Camera* camera);
	};
}
