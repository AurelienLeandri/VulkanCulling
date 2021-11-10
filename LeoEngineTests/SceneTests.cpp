#include "SceneTests.h"

#include <Scene/Scene.h>
#include <Scene/ResourcesLoading/SceneLoader.h>
#include <Scene/Lights/DirectionalLight.h>

void Sponza() {
	leo::Scene scene;
	leo::Camera camera;
	leo::SceneLoader::loadSceneToDevice("../Resources/Models/Sponza/Sponza.scene", scene, camera);
	scene->lights.push_back(std::make_shared<leo::DirectionalLight>(glm::vec3(0, -1, 0), glm::vec3(1000)));
}
