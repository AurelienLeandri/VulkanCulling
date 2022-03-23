#include "GeometryUtils.h"

glm::mat4 fpsCameraToViewMatrix(float pitch, float yaw, const glm::vec3& position, bool positiveZ)
{
    float cosPitch = glm::cos(pitch);
    float sinPitch = glm::sin(pitch);
    float cosYaw = glm::cos(yaw);
    float sinYaw = glm::sin(yaw);

    glm::vec3 xAxis{ cosYaw, 0, -sinYaw };
    glm::vec3 yAxis{ sinYaw * sinPitch, cosPitch, cosYaw * sinPitch };
    glm::vec3 zAxis{ sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw };

    return glm::mat4{
        { xAxis.x, yAxis.x, zAxis.x, 0 },
        { xAxis.y, yAxis.y, zAxis.y, 0 },
        { xAxis.z, yAxis.z, zAxis.z, 0 },
        { -glm::dot(xAxis, position), -glm::dot(yAxis, position), -glm::dot(zAxis, position), 1 },
    };
}
