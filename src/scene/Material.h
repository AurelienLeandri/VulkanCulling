#pragma once

#include <vector>

namespace leoscene {
	class BxDF;
	class Material {
	public:
		enum class Type {
			PERFORMANCE,
			PBR
		};
	public:
		virtual Type getType() const = 0;
	};
}
