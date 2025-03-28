#pragma once

#include "types.hpp"
#include <glm/fwd.hpp>

namespace mdsm::vkei
{
    class Renderable
    {
        virtual void draw(const glm::mat4& top_matrix, DrawContext& context) = 0;
    };
}