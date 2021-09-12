#include <Scene/ResourcesLoading/SceneLoader.h>

#include <Scene/Scene.h>
#include <Scene/SceneObject.h>
#include <Scene/Transform.h>
#include <Scene/ResourcesLoading/ModelLoader.h>
#include <Scene/Camera.h>

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>

namespace leo {
	namespace {
		void loadCameraEntry(std::stringstream& entry, std::shared_ptr<Camera>& camera, size_t lineNb);
		void loadModelEntry(
			std::stringstream& entry,
			std::unordered_map<std::string, Model>& models,
			const std::unordered_map<std::string, std::shared_ptr<const Transform>>& transforms,
			const std::string& fileDirectoryPath,
			size_t lineNb);
		void loadTransformEntry(std::stringstream& entry, std::unordered_map<std::string, std::shared_ptr<const Transform>>& transforms, size_t lineNb);
		void addModelInstance(std::stringstream& entry, std::shared_ptr<Scene> scene,
			const std::unordered_map<std::string, Model>& models,
			const std::unordered_map<std::string, std::shared_ptr<const Transform>>& transforms,
			size_t lineNb);
	}

	SceneLoaderException::SceneLoaderException(const char* message, size_t lineNb) : message(message), lineNb(lineNb) {
	}

	const char* SceneLoaderException::what() const noexcept {
		std::cerr << "SceneLoaderException (line " << lineNb << "): " << message << std::endl;
		return message;
	}

	void SceneLoader::loadScene(const char* filePath, std::shared_ptr<Scene>& scene, std::shared_ptr<Camera>& camera)
	{
		if (!scene) {
			scene = std::make_shared<Scene>();
		}

		std::string strFilePath(filePath);
		std::string fileDirectoryPath = strFilePath.substr(0, strFilePath.find_last_of('/'));
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			std::unordered_map<std::string, std::shared_ptr<const Transform>> transforms;
			transforms["__identity"] = std::make_shared<Transform>();
			std::unordered_map<std::string,  Model> models;
			std::string line;
			std::string entryType;
			size_t lineNb = 0;
			while (std::getline(ifs, line)) {
				std::stringstream ss(line);
				ss >> entryType;
				if (ss.fail()) {
					throw SceneLoaderException("Could not start reading line. File is empty or the line contains an invalid character.", lineNb);
				}
				if (entryType == "c") loadCameraEntry(ss, camera, lineNb);
				else if (entryType == "t") loadTransformEntry(ss, transforms, lineNb);
				else if (entryType == "m") loadModelEntry(ss, models, transforms, fileDirectoryPath, lineNb);
				else if (entryType == "o") addModelInstance(ss, scene, models, transforms, lineNb);
				else {
					throw SceneLoaderException("Could not start reading line. First character of the line does not correspond to any type of entry.", lineNb);
				}
				lineNb++;
			}
			ifs.close();

			if (!camera) {
				camera = std::make_shared<Camera>(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::radians(90.f));
			}
		}
		else {
			throw std::runtime_error("Could not open the scene file");
		}
	}

	namespace {
		void loadCameraEntry(std::stringstream& entry, std::shared_ptr<Camera>& camera, size_t lineNb)
		{
			if (camera) {
				throw SceneLoaderException("An entry for a camera was previously found. Only specify one camera entry.", lineNb);
			}
			glm::vec3 position(0);
			glm::vec3 target(0);
			float fov = 0;
			entry >> position.x >> position.y >> position.z >> target.x >> target.y >> target.z >> fov;
			if (fov <= 0 || glm::length(position - target) <= 0.0001f || entry.fail()) {
				throw SceneLoaderException("Could not read the line. Some of the tokens are invalid or absent. Check format and values.", lineNb);
			}
			camera = std::make_shared<Camera>(position, target, glm::vec3(0, 1, 0), glm::radians(fov));
		}

		void loadModelEntry(
			std::stringstream& entry,
			std::unordered_map<std::string, Model>& models,
			const std::unordered_map<std::string, std::shared_ptr<const Transform>> &transforms,
			const std::string& fileDirectoryPath,
			size_t lineNb)
		{
			std::string modelPath;
			std::string modelName;
			entry >> modelName >> modelPath;
			if (entry.fail() || !modelName.size() || !modelPath.size()) {
				throw SceneLoaderException("Could not read the line. Some of the tokens are invalid or absent. Check format and values.", lineNb);
			}
			if (models.find(modelName) != models.end()) {
				throw SceneLoaderException("A model with that name was already created. No duplicates are allowed for model entries. Choose a different name.", lineNb);
			}
			Model m = ModelLoader::loadModel((fileDirectoryPath + "/" + modelPath).c_str());
			models[modelName] = m;
		}

		void loadTransformEntry(std::stringstream& entry, std::unordered_map<std::string, std::shared_ptr<const Transform>>& transforms, size_t lineNb)
		{
			TransformParameters p = {};
			std::string transformName;
			entry >> transformName;
			if (entry.fail() || !transformName.size()) {
				throw SceneLoaderException("Could not read the line. Some of the tokens are invalid or absent. Check format and values.", lineNb);
			}
			if (transformName == "__identity") {
				throw SceneLoaderException("Could not read transform entry. The transform name \"__identity\" is reserved. Please choose another transform name", lineNb);
			}
			if (transforms.find(transformName) != transforms.end()) {
				throw SceneLoaderException("A transform with that name was already created. No duplicates are allowed for transform entries. Choose a different name.", lineNb);
			}
			entry >> p.translation.x >> p.translation.y >> p.translation.z
				>> p.scaling.x >> p.scaling.y >> p.scaling.z
				>> p.rotation_rads.x >> p.rotation_rads.y >> p.rotation_rads.z;
			if (entry.fail() || p.scaling.x == 0 || p.scaling.y == 0 || p.scaling.z == 0) {
				throw SceneLoaderException("Could not read the line. Some of the tokens are invalid or absent. Check format and values.", lineNb);
			}
			p.rotation_rads.x = glm::radians(p.rotation_rads.x);
			p.rotation_rads.y = glm::radians(p.rotation_rads.y);
			p.rotation_rads.z = glm::radians(p.rotation_rads.z);
			transforms[transformName] = std::make_shared<Transform>(p);
		}

		void addModelInstance(
			std::stringstream& entry,
			std::shared_ptr<Scene> scene,
			const std::unordered_map<std::string,
			Model>& models,
			const std::unordered_map<std::string, std::shared_ptr<const Transform>>& transforms,
			size_t lineNb)
		{
			std::string modelName;
			std::string transformName;
			entry >> modelName;
			if (entry.fail() || !modelName.size()) {
				throw SceneLoaderException("Could not read the line. Some of the tokens are invalid or absent. Check format and values.", lineNb);
			}
			if (models.find(modelName) == models.end()) {
				throw SceneLoaderException("No model was created under the given name. Specify a model entry with that name beforehand.", lineNb);
			}

			std::shared_ptr<const Transform> transform;
			entry >> modelName;
			if (!entry.fail() && transformName.size()) {
				if (transforms.find(transformName) == transforms.end()) {
					throw SceneLoaderException("No transform was created under the given name. Specify a transform entry with that name beforehand.", lineNb);
				}
				transform = transforms.at(transformName);
			}
			else {
				transform = transforms.at("__identity");
			}

			Model m = models.at(modelName);
			for (SceneObject object : m.objects) {
				if (object.transform) {
					object.transform = std::make_shared<Transform>(*transform * *object.transform);
				}
				else {
					object.transform = transform;
				}
				scene->objects.push_back(object);
			}
		}
	}
}
