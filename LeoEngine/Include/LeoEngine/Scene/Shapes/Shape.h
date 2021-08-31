#pragma once

#include <GeometryIncludes.h>

#include <memory>

namespace leo {
	struct HitRecord;
	class Transform;
	class Material;

	class Shape {
	public:
		virtual float area() const = 0;

		virtual glm::vec3 sample(const HitRecord& record, float& pdf) const = 0;

		virtual float pdf(const glm::vec3& point, const HitRecord& record) const = 0;

	public:
		const std::shared_ptr<const Transform> getTransform() const;
		void setTransform(std::shared_ptr<const Transform> transform);

	private:
		std::shared_ptr<const Transform> _transform;
		std::shared_ptr<const Material> _material;
	};
}
