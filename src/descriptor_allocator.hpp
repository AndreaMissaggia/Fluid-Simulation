#ifndef DESCRIPTOR_ALLOCATOR_HPP
#define DESCRIPTOR_ALLOCATOR_HPP

#include <span>
#include <vector>
#include <vulkan/vulkan.h>

#include "result_check.hpp"

class DescriptorAllocator {
public:
    struct PoolSizeRatio
    {
        VkDescriptorType type {};
        float ratio           {};
    };

    void init_pool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
    VkDescriptorPool _pool;
};

#endif // DESCRIPTOR_ALLOCATOR_HPP
