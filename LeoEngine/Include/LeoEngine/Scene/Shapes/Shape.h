#pragma once

#include <GeometryIncludes.h>

#include <memory>

namespace leo {
	struct HitRecord;
	class Transform;
	class Material;

	class Shape {
	public:
		enum class Type {
			TRIANGLE,
			MESH,
			SPHERE
		};

	public:
		Shape(Type type);

	public:
		virtual float area() const = 0;

		virtual glm::vec3 sample(const HitRecord& record, float& pdf) const = 0;

		virtual float pdf(const glm::vec3& point, const HitRecord& record) const = 0;

	public:
		const std::shared_ptr<const Transform> getTransform() const;
		void setTransform(std::shared_ptr<const Transform> transform);
		const std::shared_ptr<const Material> getMaterial() const;
		void setMaterial(std::shared_ptr<const Material> material);

	public:
		const Type type;

	private:
		std::shared_ptr<const Transform> _transform;
		std::shared_ptr<const Material> _material;
	};
}
