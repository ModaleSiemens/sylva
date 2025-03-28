#pragma once

#include <filesystem>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace mdsm::vkei 
{
    class Shader 
    {
        public:
            Shader(const VkShaderStageFlagBits stage, const std::filesystem::path source_path);
            Shader(const VkShaderStageFlagBits stage);

            Shader(const Shader&) = delete;
            Shader& operator=(const Shader&) = delete;

            Shader(Shader&&) = default;

            void setStage(const VkShaderStageFlagBits stage);
            void setPath(const std::filesystem::path source_path);

            void compile(const VkDevice device);

            void destroy(const VkDevice device);

            operator VkShaderModule() const;

            class ShaderSourceNotFound : public std::runtime_error
            {
                public:
                    explicit ShaderSourceNotFound(const std::filesystem::path source_path);

                    const std::filesystem::path source_path;
            };

            class CouldNotOpenFile : public std::runtime_error
            {
                public:
                    explicit CouldNotOpenFile(const std::filesystem::path source_path);

                    const std::filesystem::path source_path;
            };

            class ShaderCompilationFailed : public std::runtime_error
            {
                public:
                    explicit ShaderCompilationFailed(const std::filesystem::path source_path, const VkResult result);

                    const std::filesystem::path source_path;
                    const VkResult result;
            };            

        private:
            std::filesystem::path source_path;
            
            VkShaderStageFlagBits stage;
            VkShaderModule module;
    };
}