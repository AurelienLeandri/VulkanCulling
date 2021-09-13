#pragma once

#include <GeometryIncludes.h>

namespace leo {

	class Camera
	{
	public:
		Camera();
		Camera(glm::vec3 position, glm::vec3 look_at, glm::vec3 up_axis, float fov);

	public:
		const glm::vec3& getPosition() const;
		const glm::vec3& getUp() const;
		const glm::vec3& getRight() const;
		const glm::vec3& getFront() const;
		float getFOV() const;

		void setPosition(const glm::vec3& position);
		void setFront(const glm::vec3& front);

	private:
		void _recomputeMatrices();

	private:
		// Camera position and cartesian coordinates system
		glm::vec3 _position;
		glm::vec3 _up;
		glm::vec3 _right;
		glm::vec3 _front;

		//glm::mat4 _viewMatrix = glm::mat4(1);
		//glm::mat4 _projectionMatrix = glm::mat4(1);

		float _fov;
	};
}
