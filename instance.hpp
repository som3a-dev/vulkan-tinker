#pragma once

#include "vertex.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.h>

struct InstanceBuffer
{
	VkBuffer buffer;
	VkDeviceMemory buffer_memory;

    int instance_count;
};