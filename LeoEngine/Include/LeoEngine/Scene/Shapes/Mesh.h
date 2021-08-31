#pragma once

#include <Scene/Shapes/Shape.h>

namespace leo {
	struct MeshData;

	class Mesh : public Shape
	{
	public:
		Mesh(const MeshData* data);

	private:
		const MeshData* const _data;
	};
}
