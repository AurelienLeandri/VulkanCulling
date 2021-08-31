#pragma once

#include <GeometryIncludes.h>

namespace leo {
	class Texture {
	public:
		enum class Type {
			CONSTANT,
			IMAGE
		};

	public:
		Texture(Type type);

	public:
		virtual glm::vec4 getTexel(float u, float v) const = 0;

	public:
		const Type type;
	};
}
