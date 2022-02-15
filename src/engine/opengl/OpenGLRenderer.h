#pragma once

#include "../Renderer.h"

class OpenGLRenderer : public Renderer
{
public:
	OpenGLRenderer(const ApplicationState* applicationState, const leoscene::Camera* camera);

public:
	// Inherited via Renderer
	virtual void init(GLFWwindow* window) override;
	virtual void cleanup() override;
	virtual void drawFrame() override;
	virtual void loadSceneToRenderer(const leoscene::Scene* scene) override;

private:
	bool _sceneLoaded = false;
	bool _initialized = false;
};