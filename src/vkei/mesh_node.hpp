#pragma once

#include "node.hpp"
#include "types.hpp"
#include <memory>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei
{
    struct MeshNode : public Node
    {   
        virtual void draw(const glm::mat4& top_matrix, DrawContext& context) override;
        
        std::shared_ptr<MeshAsset> mesh;
    };
}