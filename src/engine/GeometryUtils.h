#pragma once

#include <scene/GeometryIncludes.h>

glm::mat4 fpsCameraToViewMatrix(float pitch, float yaw, const glm::vec3& position, bool positiveZ = true);  // Pitch and yaw must be in radians
