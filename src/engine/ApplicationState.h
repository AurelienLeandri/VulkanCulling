#pragma once

struct ApplicationState {
	/*
	* All data set by the input manager or user config and read by the renderers.
	*/
	bool frustumCulling = true;
	bool occlusionCulling = true;
	bool makeAllObjectsTransparent = false;
	bool lockCullingCamera = false;
	Renderer* activeRenderer = nullptr;

	struct FPSCamera {
		glm::vec3 position{ 0 };
		float pitch{ 0 };  // In degrees
		float yaw{ 0 };  // In degrees
	} fpsCamera;;
};
