#include "node.hpp"
#include "types.hpp"

namespace mdsm::vkei
{
    void Node::refreshTransform(const glm::mat4& parent_matrix)
    {
        world_transform = parent_matrix * local_transform;

        for(auto& child : children)
        {
            child->refreshTransform(world_transform);
        }
    }

    void Node::draw(const glm::mat4& top_matrix, DrawContext& context)
    {
        for(auto& child : children)
        {
            child->draw(top_matrix, context);
        }
    }
}