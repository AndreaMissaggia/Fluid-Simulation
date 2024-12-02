#include "deletion_queue.hpp"

void DeletionQueue::enqueue_image(VkImage image)
{
    _image_handles.push_back(image);
}

void DeletionQueue::enqueue_buffer(VkBuffer buffer_handle)
{
    _buffer_handles.push_back(buffer_handle);
}

void DeletionQueue::enqueue_deletor(std::function<void()>&& function)
{
    _deletors.push_back(function);
}

void DeletionQueue::flush(VkDevice device_handle)
{
    for (auto image_handle : _image_handles)
        vkDestroyImage(device_handle, image_handle, nullptr);

    for (auto buffer_handle : _buffer_handles)
        vkDestroyBuffer(device_handle, buffer_handle, nullptr);

    for (auto it { _deletors.rbegin() }; it != _deletors.rend(); ++it)
        (*it)(); //call functors

    _image_handles.clear();
    _buffer_handles.clear();
    _deletors.clear();
}

void DeletionQueue::flush()
{
    for (auto it { _deletors.rbegin() }; it != _deletors.rend(); ++it)
        (*it)(); //call functors

    _deletors.clear();
}


