#pragma once

#include "vertex.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.h>

struct Mesh
{
	VkBuffer vertex_buffer; 
	VkDeviceMemory vertex_buffer_memory; 

	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;

    int index_count;
};