#ifndef DELETION_QUEUE_HPP
#define DELETION_QUEUE_HPP

#include <vector>
#include <functional>
#include <deque>
#include <vulkan/vulkan.h>

#include "result_check.hpp"

class DeletionQueue {
public:
    void enqueue_image(VkImage image_handle);
    void enqueue_buffer(VkBuffer buffer_handle);
    void enqueue_deletor(std::function<void()>&& function);

    void flush(VkDevice device_handle);
    void flush();

private:
    std::vector<VkImage>              _image_handles  {};
    std::vector<VkBuffer>             _buffer_handles {};
    std::deque<std::function<void()>> _deletors       {};
};

#endif // DELETION_QUEUE_HPP
