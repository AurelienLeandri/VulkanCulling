#pragma once

#include <GeometryIncludes.h>

namespace leo {
	struct HitRecord;

	class Shape {
	public:
		enum class Type {
			TRIANGLE,
			MESH,
			SPHERE
		};

	public:
		virtual float area() const = 0;
		virtual glm::vec3 sample(const HitRecord& record, float& pdf) const = 0;
		virtual float pdf(const glm::vec3& point, const HitRecord& record) const = 0;
		virtual Type getType() const = 0;
	};
}
