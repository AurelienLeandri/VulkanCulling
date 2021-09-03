#pragma once

#include <GeometryIncludes.h>

#include <memory>

namespace leo {
	class Transform;
	class Material;
	class Shape;

	struct SceneObject {
		std::shared_ptr<const Transform> transform;
		std::shared_ptr<const Material> material;
		std::shared_ptr<const Shape> shape;
	};
}
