#include "shader.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

namespace mdsm::vkei 
{
    Shader::ShaderSourceNotFound::ShaderSourceNotFound(const std::filesystem::path source_path)
    :   
        source_path {source_path},
        runtime_error {
            std::format(
                "Shader source code not found at {}!", source_path.string()
            )
        }
    {
    }

    Shader::CouldNotOpenFile::CouldNotOpenFile(const std::filesystem::path source_path)
    :   
        source_path {source_path},
        runtime_error {
            std::format(
                "Could not open shader file at {}!", source_path.string()
            )
        }
    {
    }    

    Shader::ShaderCompilationFailed::ShaderCompilationFailed(
        const std::filesystem::path source_path,
        const VkResult result
    )
    :   source_path {source_path},
        result {result},
        runtime_error {
            std::format(
                "Shader compilation failed for code at {} with error {}!",
                source_path.string(),
                string_VkResult(result)
            )
        }
    {
    }    

    Shader::Shader(const VkShaderStageFlagBits stage, const std::filesystem::path source_path)
    :
        stage {stage},
        source_path {source_path}
    {
        if(!std::filesystem::exists(source_path))
        {
            throw ShaderSourceNotFound{source_path};
        }
    }

    void Shader::compile(const VkDevice device)
    {
        if(!std::filesystem::exists(source_path))
        {
            throw ShaderSourceNotFound{source_path};
        }

        std::ifstream source_file {
            source_path, std::ios::binary
        };

        if(!source_file.is_open())
        {
            throw CouldNotOpenFile{source_path};
        }

        const auto source_file_size {
            std::filesystem::file_size(source_path)
        };

        std::vector<std::uint32_t> buffer (
            source_file_size / sizeof(std::uint32_t)
        );

        source_file.read(reinterpret_cast<char*>(buffer.data()), source_file_size);

        source_file.close();

        VkShaderModuleCreateInfo create_info {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr
        };

        create_info.codeSize = buffer.size() * sizeof(std::uint32_t);
        create_info.pCode = buffer.data();

        if(
            const auto result {
                vkCreateShaderModule(device, &create_info, nullptr, &module)
            };
            result != VK_SUCCESS
        )
        {
            throw ShaderCompilationFailed{source_path, result};
        }
    }
}