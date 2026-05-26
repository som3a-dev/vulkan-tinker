/*
 * All of the helper functions that submit commands so far have been set up to execute synchronously by waiting for the queue to become idle. For practical applications it is recommended to combine these operations in a single command buffer and execute them asynchronously for higher throughput, especially the transitions and copy in the createTextureImage function. Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands to execute the commands that have been recorded so far. It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.
 * (The above note sounds extremely important for performance, its a must have quickly)
 * */


#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
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

static void init_crtdbg();

struct vertex_t
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 tex_coord;

	static const int attribute_count = 3;

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

		return attributes;
	}

	bool operator==(const vertex_t& other) const
	{
		return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
	}
};

// uniform buffer object
struct ubo_t
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
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

class App
{
public:
	void run()
	{
		init_window();
		init_vulkan();
		main_loop();
		cleanup();
	}

private:
	std::vector<vertex_t> vertices;
	std::vector<uint32_t> indices;

	static const uint32_t frames_in_flight = 3;

	const int window_width = 800;
	const int window_height = 600;
	bool minimized = false;
	bool framebuffer_resized = false;

	const std::vector<const char*> validation_layers = {
//		"VK_LAYER_KHRONOS_validation",
	};


#ifdef NDEBUG
	const bool enable_validation_layers = false;
#else
	const bool enable_validation_layers = false;
#endif

	GLFWwindow *window = nullptr;

	VkInstance instance = VK_NULL_HANDLE;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
	std::vector<VkImage> swap_chain_images;
	std::vector<VkImageView> swap_chain_image_views;
	std::vector<VkFramebuffer> swap_chain_framebuffers;
	VkFormat swap_chain_format;
	VkExtent2D swap_chain_extent = { 0 };

	VkRenderPass render_pass = VK_NULL_HANDLE;

	int current_frame = 0;

	VkImage texture_image = VK_NULL_HANDLE;
	VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;
	VkImageView texture_image_view = VK_NULL_HANDLE;
	VkSampler texture_sampler = VK_NULL_HANDLE;

	VkImage depth_image = VK_NULL_HANDLE;
	VkDeviceMemory depth_image_memory = VK_NULL_HANDLE;
	VkImageView depth_image_view = VK_NULL_HANDLE;
	const VkFormat depth_image_format = VK_FORMAT_D32_SFLOAT; // TODO(omar): check if the format is supported

	VkBuffer vertex_buffer = VK_NULL_HANDLE;
	VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;

	VkBuffer index_buffer = VK_NULL_HANDLE;
	VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;

	std::vector<VkBuffer> uniform_buffers;
	std::vector<VkDeviceMemory> uniform_buffers_memory;
	std::vector<void*> uniform_buffers_mapped;

	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptor_sets;
	VkDescriptorSetLayout descriptor_set_layout{};

	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkPipeline graphics_pipeline = VK_NULL_HANDLE;

	VkCommandPool command_pool = VK_NULL_HANDLE;
	std::array<VkCommandBuffer, frames_in_flight> command_buffers;

	std::array<VkSemaphore, frames_in_flight> image_available_semaphores;
	std::array<VkSemaphore, frames_in_flight> render_finished_semaphores;
	std::array<VkFence, frames_in_flight> in_flight_fences;

	VkQueue graphics_queue = VK_NULL_HANDLE;
	VkQueue present_queue = VK_NULL_HANDLE;

	const std::vector<const char*> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	std::vector<char> read_file(const std::string& filename)
	{
		FILE* fp = fopen(filename.c_str(), "rb");
		if (!fp)
		{
			throw std::runtime_error("failed to open file!");
		}

		fseek(fp, 0, SEEK_END);
		size_t file_sz = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		std::vector<char> buf(file_sz);
		fread(buf.data(), file_sz, 1, fp);

		fclose(fp);

		return buf;
	}

	void init()
	{
		init_window();
		init_vulkan();

/*		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_InitInfo initInfo = {};*/
	}

	void init_window()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(window_width, window_height, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	}

	void init_vulkan()
	{
		load_model();

		create_instance();
		create_surface();

		pick_physical_device();
		create_logical_device();

		create_swap_chain();
		create_swap_chain_image_views();

		create_render_pass();

		create_uniform_buffers();

		create_command_pool();

		create_depth_resources();
		create_frame_buffers();

		create_texture();
		create_texture_sampler();

		create_descriptor_set_layout();
		create_descriptor_pool();
		create_descriptor_sets();

		create_graphics_pipeline();

		create_vertex_buffer();
		create_index_buffer();

		create_command_buffers();
		create_sync_objects();
	}

	void load_model()
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
			"assets/models/viking_room.obj"))
		{
			throw std::runtime_error(err);
		}

		// TODO(omar): the unique vertex check is O(N)
		std::unordered_map<vertex_t, uint32_t> uniqueVertices{};

		for (const tinyobj::shape_t& shape : shapes)		
		{
			for (const tinyobj::index_t& index : shape.mesh.indices)
			{
				vertex_t vertex{};

				vertex.pos.x = attrib.vertices[3 * index.vertex_index + 0];
				vertex.pos.y = attrib.vertices[3 * index.vertex_index + 1];
				vertex.pos.z = attrib.vertices[3 * index.vertex_index + 2];

				vertex.tex_coord.x = attrib.texcoords[2 * index.texcoord_index + 0];
				vertex.tex_coord.y = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];

				vertex.color = {1.0f, 1.0f, 1.0f};

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = (uint32_t)(vertices.size());
					vertices.push_back(vertex);
				}

				indices.push_back(uniqueVertices[vertex]);
			}
		}

		std::cout << vertices.size() * sizeof(vertex_t) << std::endl;
	}

	void create_depth_resources()
	{
		create_image(depth_image, depth_image_memory,
			swap_chain_extent.width, swap_chain_extent.height,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			depth_image_format); 

		depth_image_view = create_image_view(depth_image, depth_image_format,
			VK_IMAGE_ASPECT_DEPTH_BIT);

		transition_image_layout(depth_image, depth_image_format,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}

	VkCommandBuffer begin_one_time_commands()
	{
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandPool = command_pool;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer command_buffer;
		vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(command_buffer, &begin_info);

		return command_buffer;
	}

	void end_one_time_commands(VkCommandBuffer& buffer)
	{
		vkEndCommandBuffer(buffer);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &buffer;

		vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphics_queue);

		vkFreeCommandBuffers(device, command_pool, 1, &buffer);
	}

	void create_image(VkImage& image, VkDeviceMemory& imageMemory,
					uint32_t width, uint32_t height,
					VkImageTiling tiling, VkImageUsageFlags usage,
					VkMemoryPropertyFlags properties, VkFormat format)
	{
		VkImageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = VK_IMAGE_TYPE_2D;

		// TODO(som3a): have a fallback format
		info.format = format;

		info.tiling = tiling;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		info.usage = usage;

		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.samples = VK_SAMPLE_COUNT_1_BIT;

		info.extent.width = width;
		info.extent.height = height;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;

		if (vkCreateImage(device, &info, nullptr,
				&image) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image,
		&memRequirements);

		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memRequirements.size;
		alloc_info.memoryTypeIndex = pickMemoryType(
		memRequirements.memoryTypeBits,
		properties);

		if (vkAllocateMemory(device, &alloc_info, nullptr,
			&imageMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate image memory");
		}

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void create_texture() {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("assets/textures/viking_room.png",
		&texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error("failed to load texture image");
		}

		VkDeviceSize texSize = texWidth * texHeight * 4;

		VkBuffer buffer;
		VkDeviceMemory bufferMemory;

		createBuffer(buffer, bufferMemory, texSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* data;
		vkMapMemory(device, bufferMemory, 0, texSize, 0, &data);
		memcpy(data, pixels, (size_t)texSize);
		vkUnmapMemory(device, bufferMemory);

		stbi_image_free(pixels);

		create_image(texture_image, texture_image_memory,
		texWidth, texHeight, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_FORMAT_R8G8B8A8_SRGB);

		transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB,
							  VK_IMAGE_LAYOUT_UNDEFINED,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(buffer, texture_image, texWidth, texHeight);

		transition_image_layout(texture_image, VK_FORMAT_R8G8B8A8_SRGB,
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, bufferMemory, nullptr);

		texture_image_view = create_image_view(texture_image, VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void create_texture_sampler()
	{
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(physical_device, &props);

		VkSamplerCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;

		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		create_info.anisotropyEnable = VK_TRUE;
		create_info.maxAnisotropy = props.limits.maxSamplerAnisotropy;
		
		create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		create_info.unnormalizedCoordinates = VK_FALSE;
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;

		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		if (vkCreateSampler(device, &create_info, nullptr, &texture_sampler) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture sampler");
		}
	}

	VkImageView create_image_view(VkImage image, VkFormat format,
		VkImageAspectFlags aspectMask)
	{
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = image;
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = format;

		create_info.subresourceRange.aspectMask = aspectMask;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		VkImageView view = VK_NULL_HANDLE;

		if (vkCreateImageView(device, &create_info, nullptr, &view) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture image view");
		}

		return view;
	}

	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkPipelineStageFlags srcStage;
		VkPipelineStageFlags dstStage;

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
				 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else 
		{
			throw std::invalid_argument("unsupported layout transition");
		}

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			// TODO(omar): we assume no stencil component here
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		VkCommandBuffer command_buffer = begin_one_time_commands();

		vkCmdPipelineBarrier(command_buffer,
							srcStage, dstStage,
							0,
							0, nullptr,
							0, nullptr,
							1, &barrier);

		end_one_time_commands(command_buffer);
	}

	void create_descriptor_sets()
	{
		std::vector<VkDescriptorSetLayout> layouts(frames_in_flight,
		descriptor_set_layout);

		VkDescriptorSetAllocateInfo alloc{};
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = descriptor_pool;
		alloc.descriptorSetCount = frames_in_flight;
		alloc.pSetLayouts = layouts.data();

		descriptor_sets.resize(frames_in_flight);
		if (vkAllocateDescriptorSets(device, &alloc,
				descriptor_sets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (int i = 0; i < frames_in_flight; i++)
		{
			VkDescriptorBufferInfo buffer{};
			buffer.buffer = uniform_buffers[i];
			buffer.range = VK_WHOLE_SIZE;

			VkDescriptorImageInfo image{};
			image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image.imageView = texture_image_view;
			image.sampler = texture_sampler;

			VkWriteDescriptorSet descriptorWrites[2] = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptor_sets[i];
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &buffer;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptor_sets[i];
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &image;

			vkUpdateDescriptorSets(device,
			sizeof(descriptorWrites) / sizeof(VkWriteDescriptorSet),
			descriptorWrites,
			0, nullptr);
		}
	}

	void create_descriptor_pool()
	{
		VkDescriptorPoolSize poolSizes[2] = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = frames_in_flight;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = frames_in_flight;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = frames_in_flight;

		if (vkCreateDescriptorPool(device,
				&poolInfo, nullptr,
				&descriptor_pool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void create_uniform_buffers()
	{
		VkDeviceSize bufferSize = sizeof(ubo_t);

		uniform_buffers.resize(frames_in_flight);
		uniform_buffers_memory.resize(frames_in_flight);
		uniform_buffers_mapped.resize(frames_in_flight);

		for (int i = 0; i < frames_in_flight; i++)
		{
			createBuffer(uniform_buffers[i],
			uniform_buffers_memory[i],
			bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			vkMapMemory(device, uniform_buffers_memory[i],
			0, bufferSize, 0,
			&uniform_buffers_mapped[i]);
		}
	}

	void create_descriptor_set_layout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding bindings[] = {
			uboLayoutBinding,
			samplerLayoutBinding
		};
		
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = sizeof(bindings) / sizeof(VkDescriptorSetLayoutBinding);
		layoutInfo.pBindings = bindings;

		if (vkCreateDescriptorSetLayout(device, &layoutInfo,
				nullptr, &descriptor_set_layout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	void create_index_buffer()
	{
		VkDeviceSize size = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		createBuffer(stagingBuffer, stagingBufferMemory,
		size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, indices.data(), size);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(index_buffer, index_buffer_memory,
		size,
		(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		copyBuffer(stagingBuffer, index_buffer, size);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void createBuffer(VkBuffer& buffer, VkDeviceMemory& memory,
	VkDeviceSize size, VkBufferUsageFlagBits usage,
	VkMemoryPropertyFlags memProperties)
	{
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Couldn't create buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memRequirements.size;
		alloc_info.memoryTypeIndex = pickMemoryType(memRequirements.memoryTypeBits, memProperties);

		if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate vertex buffer memory");
		}

		vkBindBufferMemory(device, buffer, memory, 0);
	}

	void create_vertex_buffer()
	{
		VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
		createBuffer(stagingBuffer, stagingBufferMemory,
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* memory;
		vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &memory);
		memcpy(memory, vertices.data(), size);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(vertex_buffer, vertex_buffer_memory,
		size,
		(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		copyBuffer(stagingBuffer, vertex_buffer, size);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	/*
	* NOTE: this function makes a barrier for vertex & index buffers
	* If used for something else, you should probably change it
	*/
	void copyBuffer(VkBuffer& src, VkBuffer& dst, VkDeviceSize size)
	{
		VkCommandBuffer command_buffer = begin_one_time_commands();

		VkBufferCopy copyInfo{};
		copyInfo.size = size;
		vkCmdCopyBuffer(command_buffer, src, dst, 1, &copyInfo);

		VkBufferMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = dst;
		barrier.size = size;

		vkCmdPipelineBarrier(command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr);

		end_one_time_commands(command_buffer);
	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer command_buffer = begin_one_time_commands();

		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(command_buffer,
							   buffer,
							   image,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   1,
							   &region);

		end_one_time_commands(command_buffer);
	}

	// Picks a suitable memory type acoording to requirements
	uint32_t pickMemoryType(uint32_t filter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

		for (int i = 0; i < memProperties.memoryTypeCount; i++)
		{
			VkMemoryType type = memProperties.memoryTypes[i];
			if ((filter & (1 << i)) && ((type.propertyFlags & properties) == properties))
			{
				return i;
			}
		}

		throw std::runtime_error("Failed to find suitable memory type");
	}

	void create_sync_objects()
	{
	    VkSemaphoreCreateInfo semaphoreInfo{};
	    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < frames_in_flight; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, image_available_semaphores.data() + i) != VK_SUCCESS) 
			{
				throw std::runtime_error("failed to create semaphores!");
			}
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, render_finished_semaphores.data() + i) != VK_SUCCESS) 
			{
				throw std::runtime_error("failed to create semaphores!");
			}

			if (vkCreateFence(device, &fenceInfo, nullptr, in_flight_fences.data() + i) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create fence!");
			}
		}
	}

	void create_command_pool()
	{
		QueueFamilyIndices queueFamilyIndices = find_queue_families(physical_device);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphics.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void create_command_buffers()
	{
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = command_buffers.size();

		if (vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void record_command_buffer(VkCommandBuffer command_buffer, uint32_t imageIndex)
	{
		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = render_pass;
		renderPassInfo.framebuffer = swap_chain_framebuffers[imageIndex];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = swap_chain_extent;

		VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		VkClearValue clearValues[2] = {};

		// Must follow the same order as the attachments
		clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
		clearValues[1].depthStencil = {1.0f, 0};
		
		renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(VkClearValue);
		renderPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

		VkBuffer vertexBuffers[] = {vertex_buffer};
		VkDeviceSize offsets[] = {0};

		vkCmdBindVertexBuffers(command_buffer, 0,
		sizeof(vertexBuffers) / sizeof(VkBuffer), vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer,
		index_buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
		0, 1, &(descriptor_sets[0]),
		0, nullptr);

		vkCmdDrawIndexed(command_buffer,
		indices.size(), 1, 0, 0, 0);

		vkCmdEndRenderPass(command_buffer);

		if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void create_frame_buffers()
	{
		swap_chain_framebuffers.resize(swap_chain_image_views.size());

		for (size_t i = 0; i < swap_chain_framebuffers.size(); i++)
		{
			VkImageView colorAttachment = swap_chain_image_views[i];

			VkImageView attachments[] = {
				colorAttachment, depth_image_view
			};

			VkFramebufferCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
			info.attachmentCount = sizeof(attachments) / sizeof(VkImageView);
			info.pAttachments = attachments;
			info.width = swap_chain_extent.width;
			info.height = swap_chain_extent.height;
			info.layers = 1;

			if (vkCreateFramebuffer(device, &info,
				nullptr, &swap_chain_framebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	VkShaderModule createShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = code.size();
		info.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule{};
		if (vkCreateShaderModule(device, &info, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;
	}

	void recreateSwapChain()
	{
		vkDeviceWaitIdle(device);
		framebuffer_resized = false;

		cleanup_sync_objects();
		cleanup_swap_chain();
	
		create_swap_chain();
		create_swap_chain_image_views();

		create_depth_resources();
		create_frame_buffers();

		create_sync_objects();
	}

	void create_swap_chain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physical_device);

		VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swapChainSupport.formats);
		VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.present_modes);
		VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities);

		swap_chain_format = surfaceFormat.format;
		swap_chain_extent = extent;

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		const uint32_t maxImageCount = swapChainSupport.capabilities.maxImageCount;
		if ((maxImageCount > 0) && 
			(imageCount > maxImageCount))
		{
			imageCount = maxImageCount;
		}

		std::cout << imageCount << std::endl;

		VkSwapchainCreateInfoKHR create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = surface;
		create_info.minImageCount = imageCount;
		create_info.imageFormat = surfaceFormat.format;
		create_info.imageColorSpace = surfaceFormat.colorSpace;
		create_info.imageExtent = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices queueIndices = find_queue_families(physical_device);
		std::set<uint32_t> queueIndicesSet = queueIndices.get_unique_indices();
		std::vector<uint32_t> queueIndicesList = queueIndices.get_indices_list();
		if (queueIndicesSet.size() != 1)
		{
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = static_cast<uint32_t>(queueIndicesList.size());
			create_info.pQueueFamilyIndices = queueIndicesList.data();
		}
		else
		{
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			create_info.queueFamilyIndexCount = 0;
			create_info.pQueueFamilyIndices = nullptr;
		}

		create_info.preTransform = swapChainSupport.capabilities.currentTransform;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.presentMode = presentMode;
		create_info.clipped = VK_TRUE;
		create_info.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain) != VK_SUCCESS)
		{
		    throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, swap_chain, &imageCount, nullptr);
		swap_chain_images.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swap_chain, &imageCount, swap_chain_images.data());
	}

	void create_swap_chain_image_views()
	{
		swap_chain_image_views.resize(swap_chain_images.size());

		for (size_t i = 0; i < swap_chain_images.size(); i++)
		{
			swap_chain_image_views[i] = create_image_view(swap_chain_images[i],
				swap_chain_format,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	void create_render_pass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swap_chain_format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depth_image_format;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachments[] = {
			colorAttachment, depthAttachment
		};

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = sizeof(attachments) / sizeof(VkAttachmentDescription);
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void create_graphics_pipeline()
	{
		std::vector<char> vertShaderCode = read_file("assets/shaders/vert.spv");
		std::vector<char> fragShaderCode = read_file("assets/shaders/frag.spv");
		VkShaderModule vertShader = createShaderModule(vertShaderCode);
		VkShaderModule fragShader = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertStageInfo{};
		vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageInfo.module = vertShader;
		vertStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragStageInfo{};
		fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageInfo.module = fragShader;
		fragStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStageInfos[] = {vertStageInfo, fragStageInfo};

		VkPipelineVertexInputStateCreateInfo vertexFormat{};
		vertexFormat.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkVertexInputBindingDescription bindingDesc = vertex_t::get_binding_description();
		vertexFormat.vertexBindingDescriptionCount = 1;
		vertexFormat.pVertexBindingDescriptions = &bindingDesc;

		std::array<VkVertexInputAttributeDescription, vertex_t::attribute_count> attributeDescs = vertex_t::get_attribute_descriptions();
		vertexFormat.vertexAttributeDescriptionCount = attributeDescs.size();
		vertexFormat.pVertexAttributeDescriptions = attributeDescs.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swap_chain_extent.width;
		viewport.height = (float)swap_chain_extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swap_chain_extent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptor_set_layout;

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline_layout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount =  sizeof(shaderStageInfos) / sizeof(VkPipelineShaderStageCreateInfo);
		pipelineInfo.pStages = shaderStageInfos;
		pipelineInfo.pVertexInputState = &vertexFormat;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil; 
		pipelineInfo.pColorBlendState = &colorBlending;

		pipelineInfo.layout = pipeline_layout;
		pipelineInfo.renderPass = render_pass;
		pipelineInfo.subpass = 0;

		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE,
			1, &pipelineInfo, nullptr, &graphics_pipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		vkDestroyShaderModule(device, vertShader, nullptr);
		vkDestroyShaderModule(device, fragShader, nullptr);
	}

	void main_loop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			if (!minimized)
			{
				draw_frame();
			}
		}

		vkDeviceWaitIdle(device);
	}

	void draw_frame()
	{
		vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &imageIndex);

		if (res == VK_ERROR_OUT_OF_DATE_KHR || framebuffer_resized)
		{
			recreateSwapChain();
			return;
		}
		else if ((res != VK_SUCCESS) && (res != VK_SUBOPTIMAL_KHR))
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(device, 1, &in_flight_fences[current_frame]);
		vkResetCommandBuffer(command_buffers[current_frame], 0);

		record_command_buffer(command_buffers[current_frame], imageIndex);

		update_uniform_buffer(current_frame);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {image_available_semaphores[current_frame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = waitSemaphores;
		submit_info.pWaitDstStageMask = waitStages;

		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffers[current_frame];

		VkSemaphore signalSemaphores[] = {render_finished_semaphores[current_frame]};
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swap_chain;
		presentInfo.pImageIndices = &imageIndex;

		res = vkQueuePresentKHR(present_queue, &presentInfo);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || framebuffer_resized)
		{
			recreateSwapChain();
			return;
		}
		else if ((res != VK_SUCCESS) && (res != VK_SUBOPTIMAL_KHR))
		{
			throw std::runtime_error("failed to present swap chain image!");
		}


		current_frame = (current_frame + 1) % frames_in_flight;
	}

	void update_uniform_buffer(uint32_t currentImage)
	{
		static auto start = std::chrono::high_resolution_clock::now();

		auto current = std::chrono::high_resolution_clock::now();

		float time = std::chrono::duration<float,
		std::chrono::seconds::period>(current - start).count();

		static float lastFlipTime = time;
		static float factor = 1;
		static float degrees = 1;

		ubo_t ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f),
		glm::radians(degrees),
		glm::vec3(0.0f, 0.0f, 1.0f));

		degrees += 0.2f * factor;

		if ((time - lastFlipTime) > 5)
		{
			lastFlipTime = time;
			factor *= -1;
		}

		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f));

		ubo.proj = glm::perspective(glm::radians(45.0f),
		swap_chain_extent.width / (float)swap_chain_extent.height,
		0.1f, 10.0f);

		ubo.proj[1][1] *= - 1; // GLM y is flipped
		memcpy(uniform_buffers_mapped[currentImage], &ubo,
		sizeof(ubo));
	}

	void cleanup()
	{
		cleanup_sync_objects();

		vkDestroyImage(device, texture_image, nullptr);
		vkFreeMemory(device, texture_image_memory, nullptr);
		vkDestroyImageView(device, texture_image_view, nullptr);
		vkDestroySampler(device, texture_sampler, nullptr);

		vkDestroyBuffer(device, vertex_buffer, nullptr);
		vkFreeMemory(device, vertex_buffer_memory, nullptr);

		vkDestroyBuffer(device, index_buffer, nullptr);
		vkFreeMemory(device, index_buffer_memory, nullptr);

		for (int i = 0; i < frames_in_flight; i++)
		{
			vkDestroyBuffer(device, uniform_buffers[i], nullptr);
			vkFreeMemory(device, uniform_buffers_memory[i], nullptr);
		}

		vkDestroyCommandPool(device, command_pool, nullptr);

		cleanup_swap_chain();

		vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

		vkDestroyPipeline(device, graphics_pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
		vkDestroyRenderPass(device, render_pass, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void cleanup_sync_objects()
	{
		for (int i = 0; i < frames_in_flight; i++)
		{
			vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
			vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
			vkDestroyFence(device, in_flight_fences[i], nullptr);
		}
	}

	void cleanup_swap_chain()
	{
		for (VkFramebuffer framebuffer : swap_chain_framebuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}
		for (VkImageView imageView : swap_chain_image_views)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroyImage(device, depth_image, nullptr);
		vkFreeMemory(device, depth_image_memory, nullptr);
		vkDestroyImageView(device, depth_image_view, nullptr);

		vkDestroySwapchainKHR(device, swap_chain, nullptr);
	}

	void create_surface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
	        throw std::runtime_error("failed to create window surface!");
		}
	}

	void create_logical_device()
	{
		QueueFamilyIndices indices = find_queue_families(physical_device);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = indices.get_unique_indices();

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pQueueCreateInfos = queueCreateInfos.data();
		create_info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		create_info.pEnabledFeatures = &deviceFeatures;
		create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
		create_info.ppEnabledExtensionNames = device_extensions.data();
		create_info.pEnabledFeatures = &deviceFeatures;

		if (enable_validation_layers)
		{
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();
		}
		else 
		{
			create_info.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physical_device, &create_info, nullptr, &device) != VK_SUCCESS)
		{
    		throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(device, indices.graphics.value(), 0, &graphics_queue);
		vkGetDeviceQueue(device, indices.present.value(), 0, &present_queue);
	}

	void pick_physical_device()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const VkPhysicalDevice& device : devices)
		{
			if (is_physical_device_suitable(device))
			{
				physical_device = device;
				break;
			}
		}

		if (physical_device == VK_NULL_HANDLE)
		{
			throw std::runtime_error("no suitable GPU was found!");
		}
	}

	bool is_physical_device_suitable(VkPhysicalDevice device)
	{
		// TODO(omar): Maybe say why the device is not suitable

		QueueFamilyIndices queueFams = find_queue_families(device);
		if (!queueFams.is_complete())
		{
			return false;
		}

		if (!check_device_extensions_support(device))
		{
			return false;
		}

		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		if (swapChainSupport.formats.empty() || swapChainSupport.present_modes.empty())
		{
			return false;
		}

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(device, &features);

		if (features.samplerAnisotropy == VK_FALSE)
		{
			return false;
		}

		return true;
	}

	bool check_device_extensions_support(VkPhysicalDevice device)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(device_extensions.begin(), device_extensions.end());

		for (const VkExtensionProperties& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
			if (requiredExtensions.empty())
			{
				return true;
			}
		}

		return false;
	}

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

	QueueFamilyIndices find_queue_families(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t fam_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &fam_count, nullptr);

		std::vector<VkQueueFamilyProperties> fams(fam_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &fam_count, fams.data());

		for (uint32_t i = 0; i < fam_count; i++)
		{
			if (indices.is_complete())
			{
				break;
			}

			const VkQueueFamilyProperties& fam = fams[i];
			if (fam.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphics = i;
			}

			VkBool32 presentation_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentation_support);
			if (presentation_support)
			{
				indices.present = i;
			}
		}

		return indices;
	}

	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details{};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

		if (format_count != 0) {
			details.formats.resize(format_count);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
		}

		uint32_t present_mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

		if (present_mode_count != 0) {
			details.present_modes.resize(present_mode_count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
	{
		for (const VkSurfaceFormatKHR& format : available_formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}

		return available_formats[0];
	}

	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes)
	{
		for (const VkPresentModeKHR& mode : available_present_modes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D extent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width);

			extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height);

			return extent;
		}
	}

	void create_instance()
	{
		if (enable_validation_layers)
		{
			if (!check_validation_layers())
			{
        		throw std::runtime_error("validation layers requested, but not available!");
			}
		}

		std::vector<const char*> extensions = get_required_extensions();
		check_extensions(extensions);

		VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Hello Vulkan Triangle";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledLayerCount = 0;
		create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		create_info.ppEnabledExtensionNames = extensions.data();

		if (enable_validation_layers)
		{
			create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
			create_info.ppEnabledLayerNames = validation_layers.data();
		}

        if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) 
		{
            throw std::runtime_error("failed to create instance!");
        }
	}

	bool check_validation_layers()
	{
		uint32_t layer_prop_count = 0;
		vkEnumerateInstanceLayerProperties(&layer_prop_count, nullptr);

		std::vector<VkLayerProperties> layer_props(layer_prop_count);
		vkEnumerateInstanceLayerProperties(&layer_prop_count, layer_props.data());

		for (const char* layer : validation_layers)
		{
			bool found = false;

			std::cout << layer << std::endl;
			for (const VkLayerProperties& layer_prop : layer_props)
			{
				std::cout << layer_prop.layerName << std::endl;
				if (strcmp(layer_prop.layerName, layer) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				return false;
			}
		}

		return true;
	}

	void check_extensions(const std::vector<const char*>& extensions)
	{
		uint32_t extension_prop_count = 0;
	 	vkEnumerateInstanceExtensionProperties(nullptr, &extension_prop_count, nullptr);

		std::vector<VkExtensionProperties> extension_props(extension_prop_count);
	 	vkEnumerateInstanceExtensionProperties(nullptr, &extension_prop_count, extension_props.data());

		for (const char* extension: extensions)
		{
			bool found = false;

			for (const VkExtensionProperties& extension_prop : extension_props)
			{
				if (strcmp(extension_prop.extensionName, extension) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				throw std::runtime_error("Required extension not supported: " + std::string(extension));
			}
		}
	}

	std::vector<const char*> get_required_extensions()
	{
		uint32_t glfw_extension_count = 0;
		const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

		return extensions;
	}


	static void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
	    app->framebuffer_resized = true;

		if ((width == 0) || (height == 0))
		{
			app->minimized = true;
		}
		else
		{
			app->minimized = false;
		}
	}
};

int main()
{
	init_crtdbg();

	int code = EXIT_SUCCESS;

	try
	{
		App app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		code = EXIT_FAILURE;
	}

	return code;
}

static void init_crtdbg()
{
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
}