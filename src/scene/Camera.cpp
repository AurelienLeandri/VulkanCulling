#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace leoscene {

	Camera::Camera() :
		Camera(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0), glm::radians(90.f))
	{
	}

	Camera::Camera(glm::vec3 position, glm::vec3 look_at, glm::vec3 up_axis, float fov) :
		_position(position), _fov(fov)
	{
		// Compute coordinate system
		_front = glm::normalize(look_at - position);
		_right = glm::normalize(glm::cross(_front, glm::normalize(up_axis)));
		_up = glm::normalize(cross(_right, _front));
	}

	const glm::vec3& Camera::getPosition() const
	{
		return _position;
	}

	const glm::vec3& Camera::getUp() const
	{
		return _up;
	}

	const glm::vec3& Camera::getRight() const
	{
		return _right;
	}

	const glm::vec3& Camera::getFront() const
	{
		return _front;
	}

	void Camera::setPosition(const glm::vec3& position)
	{
		_position = position;
	}

	float Camera::getFOV() const {
		return _fov;
	}

	void Camera::setFront(const glm::vec3& front)
	{
		_front = glm::normalize(front);
		// Also re-calculate the Right and Up vector
		_right = glm::normalize(glm::cross(_front, glm::vec3(0, -1, 0)));
		// Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		_up = glm::normalize(glm::cross(_right, _front));
	}
}
