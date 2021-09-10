#include <Scene/Scene.h>
#include <Scene/ResourcesLoading/SceneLoader.h>
#include <Scene/Lights/DirectionalLight.h>

int main() {
	std::shared_ptr<leo::Scene> scene;
	std::shared_ptr<leo::Camera> camera;
	leo::SceneLoader::loadScene("../Resources/Models/Sponza/Sponza.scene", scene, camera);
	scene->lights.push_back(std::make_shared<leo::DirectionalLight>(glm::vec3(0, -1, 0), glm::vec3(1000)));
	return 0;
}