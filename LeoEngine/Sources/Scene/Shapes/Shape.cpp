#include <Scene/Shapes/Shape.h>

namespace leo {

	const std::shared_ptr<const Transform> Shape::getTransform() const
	{
		return _transform;
	}

	void Shape::setTransform(std::shared_ptr<const Transform> transform)
	{
		_transform = transform;
	}

}
