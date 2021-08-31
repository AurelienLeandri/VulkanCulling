#include <Scene/SceneObject.h>

namespace leo {
	SceneObject::SceneObject(const std::shared_ptr<const Shape> shape)
		: _shape(shape)
	{
	}

	const std::shared_ptr<const Transform> SceneObject::getTransform() const
	{
		return _transform;
	}

	void SceneObject::setTransform(std::shared_ptr<const Transform> transform)
	{
		_transform = transform;
	}

	const std::shared_ptr<const Material> SceneObject::getMaterial() const
	{
		return _material;
	}

	void SceneObject::setMaterial(std::shared_ptr<const Material> material)
	{
		_material = material;
	}

}
