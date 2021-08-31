#include <Scene/Shapes/Shape.h>

namespace leo {
	Shape::Shape(Type type)
		: type(type)
	{
	}

	const std::shared_ptr<const Transform> Shape::getTransform() const
	{
		return _transform;
	}

	void Shape::setTransform(std::shared_ptr<const Transform> transform)
	{
		_transform = transform;
	}

	const std::shared_ptr<const Material> Shape::getMaterial() const
	{
		return _material;
	}

	void Shape::setMaterial(std::shared_ptr<const Material> material)
	{
		_material = material;
	}

}
