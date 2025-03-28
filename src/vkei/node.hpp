#pragma once

#include "renderable.hpp"
#include "types.hpp"
#include <memory>

namespace mdsm::vkei
{
    struct Node : public Renderable
    {
        void refreshTransform(const glm::mat4& parent_matix);

        virtual void draw(const glm::mat4& top_matrix, DrawContext& context) override;

        std::weak_ptr<Node> parent;
        std::vector<std::shared_ptr<Node>> children;

        glm::mat4 local_transform;
        glm::mat4 world_transform;
    };
}