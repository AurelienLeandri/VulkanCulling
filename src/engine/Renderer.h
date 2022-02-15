#pragma once

namespace leoscene {
	class Camera;
	class Scene;
}

struct GLFWwindow;
struct ApplicationState;

class Renderer
{
public:
	Renderer(const ApplicationState* applicationState, const leoscene::Camera* camera);

public:
	virtual void init(GLFWwindow* window) = 0;
	virtual void cleanup() = 0;
	virtual void drawFrame() = 0;
	virtual void loadSceneToRenderer(const leoscene::Scene* scene) = 0;

protected:
	const leoscene::Camera* _camera = nullptr;  // Application camera
	const ApplicationState* _applicationState = nullptr;  // Application state
};
