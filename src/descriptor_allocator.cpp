#include "descriptor_allocator.hpp"

void DescriptorAllocator::init_pool(VkDevice device,
    uint32_t max_sets,
    std::span<PoolSizeRatio> pool_ratios)
{
    std::vector<VkDescriptorPoolSize> pool_sizes {};

    for (PoolSizeRatio ratio : pool_ratios)
        pool_sizes.push_back( VkDescriptorPoolSize { .type = ratio.type, .descriptorCount = uint32_t(ratio.ratio * max_sets) } );

    VkDescriptorPoolCreateInfo pool_info { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags         = { 0 };
    pool_info.maxSets       = { max_sets };
    pool_info.poolSizeCount = { (uint32_t)pool_sizes.size() };
    pool_info.pPoolSizes    = { pool_sizes.data() };

    vkCreateDescriptorPool(device, &pool_info, nullptr, &_pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, _pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, _pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocate_info { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocate_info.pNext              = { nullptr };
    allocate_info.descriptorPool     = { _pool };
    allocate_info.descriptorSetCount = { 1 };
    allocate_info.pSetLayouts        = { &layout };

    VkDescriptorSet descriptor_set {};
    result_check(vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set));

    return descriptor_set;
}
