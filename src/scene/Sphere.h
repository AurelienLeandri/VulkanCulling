#pragma once

#include "Shape.h"

namespace leoscene {
	class Sphere : public Shape {
	public:
		struct alignas(16) Data {
			glm::vec3 position = glm::vec3(0);
			float radius = 1.f;
		};

	public:
		Sphere(const glm::vec3& position = glm::vec3(0), float radius = 1.f);

	public:
		virtual float area() const;
		virtual glm::vec3 sample(const HitRecord& record, float& pdf) const;
		virtual float pdf(const glm::vec3& point, const HitRecord& record) const;
		virtual Type getType() const;

	public:
		Data data;
		glm::vec3& position = data.position;
		float& radius = data.radius;
	};
}
