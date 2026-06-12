#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_CXX17
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <tiny_obj_loader.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <array>
#include <limits>
#include <string>

#include "vertex.hpp"

namespace Vulkan
{
	struct VulkanContext
	{
		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> present_modes;
		};

		struct QueueFamilyIndices
		{
			std::optional<uint32_t> graphics;
			std::optional<uint32_t> present;

			bool is_complete()
			{
				return graphics.has_value() && present.has_value();
			}

			std::set<uint32_t> get_unique_indices()
			{
				if (!is_complete())
				{
					return {};
				}

				return 
				{
					graphics.value(),
					present.value()
				};
			}

			std::vector<uint32_t> get_indices_list()
			{
				std::vector<uint32_t> list;
				list.push_back(graphics.value());
				list.push_back(present.value());

				return list;
			}
		};

		const std::vector<const char*> validation_layers = {
			"VK_LAYER_KHRONOS_validation",
		};

		const std::vector<const char*> device_extensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		static const uint32_t frames_in_flight = 3;

#ifdef NDEBUG
		const bool enable_validation_layers = false;
#else
		const bool enable_validation_layers = true;
#endif

		bool framebuffer_resized = false;

		VkInstance instance = VK_NULL_HANDLE;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		VkQueue graphics_queue = VK_NULL_HANDLE;
		uint32_t graphics_queue_index = -1;
		VkQueue present_queue = VK_NULL_HANDLE;

		VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
		std::vector<VkImage> swap_chain_images;
		std::vector<VkImageView> swap_chain_image_views;
		std::vector<VkFramebuffer> swap_chain_framebuffers;
		VkFormat swap_chain_format;
		VkExtent2D swap_chain_extent = { 0 };

		VkRenderPass render_pass = VK_NULL_HANDLE;

		VkImage depth_image = VK_NULL_HANDLE;
		VkDeviceMemory depth_image_memory = VK_NULL_HANDLE;
		VkImageView depth_image_view = VK_NULL_HANDLE;
		const VkFormat depth_image_format = VK_FORMAT_D32_SFLOAT;

		VkSampler texture_sampler = VK_NULL_HANDLE;

		std::vector<VkBuffer> uniform_buffers;
		std::vector<VkDeviceMemory> uniform_buffers_memory;
		std::vector<void*> uniform_buffers_mapped;

		VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptor_set_layout{};

		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		VkPipeline graphics_pipeline = VK_NULL_HANDLE;

		VkCommandPool command_pool = VK_NULL_HANDLE;
		std::array<VkCommandBuffer, frames_in_flight> command_buffers;
		std::array<VkSemaphore, frames_in_flight> image_available_semaphores;
		std::array<VkSemaphore, frames_in_flight> render_finished_semaphores;
		std::array<VkFence, frames_in_flight> in_flight_fences;

		int current_frame = 0;

		void init(GLFWwindow* window);
		void init_imgui();
		void destroy();

		void recreateSwapChain(GLFWwindow* window);
		void create_swap_chain(GLFWwindow* window);
		void create_swap_chain_image_views();
		void cleanup_swap_chain();

		void create_texture(const char* path, VkImage& texture_image,
			VkImageView& texture_image_view, VkDeviceMemory& texture_image_memory);

		void create_vertex_buffer(std::vector<vertex_t>& vertices,
			VkBuffer& vertex_buffer, VkDeviceMemory& vertex_buffer_memory);

		void create_index_buffer(std::vector<uint32_t>& indices,
			VkBuffer& index_buffer, VkDeviceMemory& index_buffer_memory);

		void createBuffer(VkBuffer& buffer, VkDeviceMemory& memory,
			VkDeviceSize size, VkBufferUsageFlagBits usage,
			VkMemoryPropertyFlags memProperties);

		void allocate_descriptor_sets(std::vector<VkDescriptorSet>& descriptor_sets);
		void create_descriptor_sets(std::vector<VkDescriptorSet>& descriptor_sets,
			VkImageView& texture_image_view);

		VkImageView create_image_view(VkImage image, VkFormat format,
			VkImageAspectFlags aspectMask);


		void copyBuffer(VkBuffer& src, VkBuffer& dst, VkDeviceSize size);
		void copyBufferToImage(VkBuffer buffer, VkImage image,
			uint32_t width, uint32_t height);

		VkCommandBuffer begin_one_time_commands();
		void end_one_time_commands(VkCommandBuffer& buffer);

	private:
		void create_instance();
		void create_surface(GLFWwindow* window);
		void pick_physical_device();
		void create_logical_device();
		void create_render_pass();
		void create_descriptor_set_layout();
		void create_descriptor_pool();
		void create_uniform_buffers();
		void create_command_pool();
		void create_command_buffers();
		void create_depth_resources();
		void create_frame_buffers();
		void create_texture_sampler(VkSampler& texture_sampler);
		void create_graphics_pipeline();
		void create_sync_objects();
		void cleanup_sync_objects();

		void create_image(VkImage& image, VkDeviceMemory& imageMemory,
			uint32_t width, uint32_t height,
			VkImageTiling tiling, VkImageUsageFlags usage,
			VkMemoryPropertyFlags properties, VkFormat format);

		void transition_image_layout(VkImage image, VkFormat format,
			VkImageLayout oldLayout, VkImageLayout newLayout);

		bool is_physical_device_suitable(VkPhysicalDevice device);
		bool check_device_extensions_support(VkPhysicalDevice device);
		bool check_validation_layers();
		void check_extensions(const std::vector<const char*>& extensions);
		std::vector<const char*> get_required_extensions();

		QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

		VkSurfaceFormatKHR choose_swap_surface_format(
			const std::vector<VkSurfaceFormatKHR>& available_formats);
		VkPresentModeKHR choose_swap_present_mode(
			const std::vector<VkPresentModeKHR>& available_present_modes);
		VkExtent2D choose_swap_extent(GLFWwindow* window,
			const VkSurfaceCapabilitiesKHR& capabilities);

		VkShaderModule createShaderModule(const std::vector<char>& code);
		uint32_t pickMemoryType(uint32_t filter, VkMemoryPropertyFlags properties);
		std::vector<char> read_file(const std::string& filename);

		static void check_vk_result(VkResult err);
	};
}
