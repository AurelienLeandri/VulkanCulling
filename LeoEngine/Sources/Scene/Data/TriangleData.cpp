#include <Scene/Data/TriangleData.h>

namespace leo {
	TriangleData::TriangleData(const Vertex& v0, const Vertex& v1, const Vertex& v2)
		: vertices({v0, v1, v2})
	{
	}
}
