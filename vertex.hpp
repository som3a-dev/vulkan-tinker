#pragma once

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

struct vertex_t
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static const int attribute_count = 7;

	static VkVertexInputBindingDescription get_binding_description()
	{
		VkVertexInputBindingDescription binding_description{};
		binding_description.binding = 0;
		binding_description.stride = sizeof(vertex_t);
		binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return binding_description;
	}

	static std::array<VkVertexInputAttributeDescription, attribute_count> get_attribute_descriptions()
	{
		std::array<VkVertexInputAttributeDescription, attribute_count> attributes{};

		{
			attributes[0].location = 0;
			attributes[0].binding = 0;
			attributes[0].offset = offsetof(vertex_t, pos);
			attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		}
		{
			attributes[1].location = 1;
			attributes[1].binding = 0;
			attributes[1].offset = offsetof(vertex_t, color);
			attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		}
		{
			attributes[2].location = 2;
			attributes[2].binding = 0;
			attributes[2].offset = offsetof(vertex_t, tex_coord);
			attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
		}

		{
			attributes[3].binding = 1;
			attributes[3].location = 3;
			attributes[3].offset = sizeof(glm::vec4) * 0;
			attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		{
			attributes[4].binding = 1;
			attributes[4].location = 4;
			attributes[4].offset = sizeof(glm::vec4) * 1;
			attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		{
			attributes[5].binding = 1;
			attributes[5].location = 5;
			attributes[5].offset = sizeof(glm::vec4) * 2;
			attributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		{
			attributes[6].binding = 1;
			attributes[6].location = 6;
			attributes[6].offset = sizeof(glm::vec4) * 3;
			attributes[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}

		return attributes;
	}

	bool operator==(const vertex_t& other) const
	{
		return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
	}
};

namespace std {
	template<> struct hash<vertex_t> {
		size_t operator()(vertex_t const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.tex_coord) << 1);
		}
	};
}