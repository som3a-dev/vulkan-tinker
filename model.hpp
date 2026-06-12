#pragma once

#include "vertex.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_CXX17
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <tiny_obj_loader.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <array>
#include <limits>

struct Model
{
	std::vector<vertex_t> vertices;
	std::vector<uint32_t> indices;
	tinyobj::attrib_t attrib;

	VkBuffer vertex_buffer = VK_NULL_HANDLE;
	VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;

	VkBuffer index_buffer = VK_NULL_HANDLE;
	VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;

	VkImage texture_image = VK_NULL_HANDLE;
	VkImageView texture_image_view = VK_NULL_HANDLE;
	VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;

	std::vector<VkDescriptorSet> descriptor_sets;

	void load_mesh(const tinyobj::shape_t& shape);
	void load_mat(const tinyobj::material_t& mat);
};
