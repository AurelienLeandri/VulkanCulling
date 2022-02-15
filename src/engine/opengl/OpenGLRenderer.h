#pragma once

#include "../Renderer.h"

class Window;

class OpenGLRenderer : public Renderer
{
public:
	OpenGLRenderer(const ApplicationState* applicationState, const leoscene::Camera* camera);

public:
	// Inherited via Renderer
	virtual void init(Window* window) override;
	virtual void cleanup() override;
	virtual void drawFrame() override;
	virtual void loadSceneToRenderer(const leoscene::Scene* scene) override;

	void notifyWindowResize();

private:
	void _resizeViewport(size_t width, size_t height);

private:
	Window* _window = nullptr;

	bool _sceneLoaded = false;
	bool _initialized = false;
	bool _viewportNeedsResize = false;
};