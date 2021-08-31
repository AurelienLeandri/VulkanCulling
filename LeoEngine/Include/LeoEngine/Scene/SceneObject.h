#pragma once

#include <GeometryIncludes.h>

#include <memory>

namespace leo {
	class Transform;
	class Material;
	class Shape;

	class SceneObject {
	public:
		SceneObject(const std::shared_ptr<const Shape> shape);

	public:
		const std::shared_ptr<const Transform> getTransform() const;
		void setTransform(std::shared_ptr<const Transform> transform);
		const std::shared_ptr<const Material> getMaterial() const;
		void setMaterial(std::shared_ptr<const Material> material);

	private:
		std::shared_ptr<const Transform> _transform;
		std::shared_ptr<const Material> _material;
		const std::shared_ptr<const Shape> _shape;
	};
}
