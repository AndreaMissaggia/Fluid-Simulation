#include "descriptor_layout_builder.hpp"

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding new_bind {};
    new_bind.binding         = { binding };
    new_bind.descriptorCount = { 1 };
    new_bind.descriptorType  = { type };

    _bindings.push_back(new_bind);
}

void DescriptorLayoutBuilder::clear()
{
    _bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build
(
    VkDevice device,
    VkShaderStageFlags shader_stages,
    void* p_next,
    VkDescriptorSetLayoutCreateFlags flags
)
{
    for (auto& binding : _bindings)
        binding.stageFlags |= shader_stages;

    VkDescriptorSetLayoutCreateInfo info { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pBindings    = { _bindings.data() };
    info.bindingCount = { (uint32_t)_bindings.size() };
    info.flags        = { flags };

    VkDescriptorSetLayout set {};
    result_check(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}
