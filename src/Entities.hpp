#pragma once

#include <vector>

#include "math/Math.hpp"

struct Entity
{
    glm::vec3 scale = glm::vec3(1, 1, 1);
    glm::vec3 position;
    glm::quat orientation;
    size_t instance_id = 0;
};

class EntityList : public std::vector<Entity>
{

};
