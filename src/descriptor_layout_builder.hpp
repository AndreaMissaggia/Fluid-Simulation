#ifndef DESCRIPTOR_LAYOUT_BUILDER_HPP
#define DESCRIPTOR_LAYOUT_BUILDER_HPP

#include <vector>
#include <vulkan/vulkan.h>

#include "result_check.hpp"

class DescriptorLayoutBuilder {
public:
    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();

    VkDescriptorSetLayout build
    (
        VkDevice device,
        VkShaderStageFlags shader_stages,
        void* p_next = nullptr,
        VkDescriptorSetLayoutCreateFlags flags = 0
    );

private:
    std::vector<VkDescriptorSetLayoutBinding> _bindings;
};

#endif // DESCRIPTOR_LAYOUT_BUILDER_HPP
