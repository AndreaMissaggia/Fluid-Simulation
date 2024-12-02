#include "result_check.hpp"

void result_check(VkResult result)
{
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        std::printf("Detected Vulkan error: %d\n", result);
        abort();
    }
}
