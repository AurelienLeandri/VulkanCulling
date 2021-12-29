#pragma once

#include "GeometryIncludes.h"

namespace leoscene {
	class Texture {
	public:
		enum class Type {
			CONSTANT,
			IMAGE
		};

	public:
		Texture(Type type);
		virtual ~Texture() = default;

	public:
		virtual glm::vec4 getTexel(float u, float v) const = 0;

	public:
		const Type type;
	};
}
