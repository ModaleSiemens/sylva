#include "mesh_node.hpp"
#include "types.hpp"

namespace mdsm::vkei
{
    void MeshNode::draw(const glm::mat4& top_matrix, DrawContext& context)
    {
        glm::mat4 node_matrix {
            top_matrix * world_transform
        };

        for(auto& surface : mesh->surfaces)
        {
            RenderObject def;

            def.index_count = surface.count;
            def.first_index = surface.start_index;
            def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
            def.material = &surface.material->data;

            def.transform = node_matrix;
            def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;

            context.opaque_surfaces.push_back(def);
        }
    }
}