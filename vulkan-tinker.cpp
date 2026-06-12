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

#include "vulkan_ctx.hpp"
#include "model.hpp"
#include "vertex.hpp"
#include "ubo.hpp"


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

class App
{
public:
	void run()
	{
		init();
		main_loop();
		cleanup();
	}

private:
	Vulkan::VulkanContext vk_ctx;

	std::vector<Model> models;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> mats;

	const int instance_count = 1;
	VkBuffer instance_buffer = VK_NULL_HANDLE;
	VkDeviceMemory instance_buffer_memory = VK_NULL_HANDLE;

	const int window_width = 800;
	const int window_height = 600;
	bool minimized = false;

	glm::vec3 camera_pos = glm::vec3(0.0f, 0.5f, -3.0f);
	glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);

	float dt = 0.0f;
	float last_frame = 0.0f; // time of the last frame

	float mouse_x = 400;
	float mouse_y = 300;

	float yaw = 88.2f;
	float pitch = 0;

	GLFWwindow *window = nullptr;

	int current_frame = 0;

	void init()
	{
		init_window();
		vk_ctx.init(window);
		create_instance_buffer();
		load_models();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForVulkan(window, true);
		vk_ctx.init_imgui();
	}

	void init_window()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();

		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		window = glfwCreateWindow(window_width, window_height, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
		glfwSetCursorPosCallback(window, mouse_callback);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index, ImDrawData* imgui_draw_data)
	{
		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vk_ctx.render_pass;
		renderPassInfo.framebuffer = vk_ctx.swap_chain_framebuffers[image_index];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = vk_ctx.swap_chain_extent;

		VkClearValue clearValues[2] = {};

		// Must follow the same order as the attachments
		clearValues[0].color = {{1.0f, 1.0f, 1.0f, 1.0f}};
		clearValues[1].depthStencil = {1.0f, 0};
		
		renderPassInfo.clearValueCount = sizeof(clearValues) / sizeof(VkClearValue);
		renderPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_ctx.graphics_pipeline);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = vk_ctx.swap_chain_extent;

		vkCmdSetScissor(command_buffer, 0, 1, &scissor);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)vk_ctx.swap_chain_extent.width;
		viewport.height = (float)vk_ctx.swap_chain_extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(command_buffer, 0, 1, &viewport);

		Model model = models[1];

		VkBuffer vertex_buffer = model.vertex_buffer;
		VkBuffer index_buffer = model.index_buffer;

		VkBuffer vertexBuffers[] = {vertex_buffer, instance_buffer};
		VkDeviceSize offsets[] = {0, 0};

		vkCmdBindVertexBuffers(command_buffer, 0,
		sizeof(vertexBuffers) / sizeof(VkBuffer), vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer,
		index_buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS, vk_ctx.pipeline_layout,
		0, 1, &(model.descriptor_sets[0]),
		0, nullptr);

		vkCmdDrawIndexed(command_buffer,
		model.indices.size(), instance_count, 0, 0, 0);

		ImGui_ImplVulkan_RenderDrawData(imgui_draw_data, command_buffer);

		vkCmdEndRenderPass(command_buffer);

		if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void main_loop()
	{
		while (!glfwWindowShouldClose(window))
		{
			float current_frame = glfwGetTime();
			dt = current_frame - last_frame;
			last_frame = current_frame;

			glfwPollEvents();
			if (!minimized)
			{
				process_input(window);
				glm::vec3 direction;
				direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
				direction.y = sin(glm::radians(pitch));
				direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

				camera_front = glm::normalize(direction);
				draw_frame();
			}
		}

		vkDeviceWaitIdle(vk_ctx.device);
	}

	void draw_frame()
	{
		vkWaitForFences(vk_ctx.device, 1, &vk_ctx.in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

		uint32_t image_index;
		VkResult res = vkAcquireNextImageKHR(vk_ctx.device, vk_ctx.swap_chain, UINT64_MAX, vk_ctx.image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);

		if (res == VK_ERROR_OUT_OF_DATE_KHR || vk_ctx.framebuffer_resized)
		{
			vk_ctx.recreateSwapChain(window);
			return;
		}
		else if ((res != VK_SUCCESS) && (res != VK_SUBOPTIMAL_KHR))
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(vk_ctx.device, 1, &vk_ctx.in_flight_fences[current_frame]);
		vkResetCommandBuffer(vk_ctx.command_buffers[current_frame], 0);

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

//		ImGui::ShowDemoWindow();

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
									ImGuiWindowFlags_AlwaysAutoResize | 
									ImGuiWindowFlags_NoSavedSettings | 
									ImGuiWindowFlags_NoFocusOnAppearing | 
									ImGuiWindowFlags_NoNav;

		if (ImGui::Begin("Performance Diagnostics", nullptr, window_flags)) {
			ImGui::Text("VULKAN RENDERER");
			ImGui::Separator();
			
			// Grab values directly from ImGui's built-in timing metrics
			float fps = ImGui::GetIO().Framerate;
			float ms = 1000.0f / fps;

			ImGui::Text("Performance: %.2f FPS", fps);
			ImGui::Text("Frame Time:  %.3f ms", ms);
			ImGui::Text("Cam Pos:     %.3f, %.3f, %.3f", camera_pos.x, camera_pos.y, camera_pos.z);
			
			// You can easily push your own engine metrics here later!
			// ImGui::Text("Draw Calls:  %d", globalDrawCallCount);
		}
		ImGui::End();

		ImGui::Render();

		ImDrawData* draw_data = ImGui::GetDrawData();

		record_command_buffer(vk_ctx.command_buffers[current_frame], image_index, draw_data);

		update_uniform_buffer(current_frame);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {vk_ctx.image_available_semaphores[current_frame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = waitSemaphores;
		submit_info.pWaitDstStageMask = waitStages;

		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &vk_ctx.command_buffers[current_frame];

		VkSemaphore signalSemaphores[] = {vk_ctx.render_finished_semaphores[current_frame]};
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(vk_ctx.graphics_queue, 1, &submit_info, vk_ctx.in_flight_fences[current_frame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &vk_ctx.swap_chain;
		presentInfo.pImageIndices = &image_index;

		res = vkQueuePresentKHR(vk_ctx.present_queue, &presentInfo);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || vk_ctx.framebuffer_resized)
		{
			vk_ctx.recreateSwapChain(window);
			return;
		}
		else if ((res != VK_SUCCESS) && (res != VK_SUBOPTIMAL_KHR))
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		current_frame = (current_frame + 1) % vk_ctx.frames_in_flight;
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
		glm::vec3(0.0f, 10.0f, 400.0f));

//		ubo.model = glm::translate(glm::mat4(1.0f),
//		glm::vec3(0.0f, 0.0f, 0.0f));

//		degrees += 0.2f * factor;

		if ((time - lastFlipTime) > 5)
		{
			lastFlipTime = time;
			factor *= -1;
		}

		glm::vec3 camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 camera_direction = glm::normalize(camera_pos - camera_target);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 camera_right = glm::normalize(glm::cross(up, camera_direction));

		/*ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));*/

// 		const float radius = 4.0f;
		// float camX = sin(glfwGetTime()) * radius;
		// float camZ = cos(glfwGetTime()) * radius;
		glm::mat4 view;
		view = glm::lookAt(camera_pos,
		camera_pos + camera_front, camera_up);

		ubo.view = view;

		ubo.proj = glm::perspective(glm::radians(60.0f),
		vk_ctx.swap_chain_extent.width / (float)vk_ctx.swap_chain_extent.height,
		0.1f, 10000.0f);

		ubo.proj[1][1] *= -1; // GLM y is flipped
		memcpy(vk_ctx.uniform_buffers_mapped[currentImage], &ubo,
		sizeof(ubo));
	}

	void cleanup()
	{
		vkDeviceWaitIdle(vk_ctx.device);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		ImGui::DestroyContext();

		vkDestroyBuffer(vk_ctx.device, instance_buffer, nullptr);
		vkFreeMemory(vk_ctx.device, instance_buffer_memory, nullptr);
		for (Model& model : models)
		{
			vkDestroyImage(vk_ctx.device, model.texture_image, nullptr);
			vkFreeMemory(vk_ctx.device, model.texture_image_memory, nullptr);
			vkDestroyImageView(vk_ctx.device, model.texture_image_view, nullptr);

			vkDestroyBuffer(vk_ctx.device, model.vertex_buffer, nullptr);
			vkFreeMemory(vk_ctx.device, model.vertex_buffer_memory, nullptr);

			vkDestroyBuffer(vk_ctx.device, model.index_buffer, nullptr);
			vkFreeMemory(vk_ctx.device, model.index_buffer_memory, nullptr);
		}

		vk_ctx.destroy();

		glfwDestroyWindow(window);
		glfwTerminate();
	}

	Model load_model(const tinyobj::shape_t& shape, const tinyobj::material_t& mat)
	{
		Model model;
		model.attrib = attrib;
		model.load_mesh(shape);

		std::string path = "assets/workshop/" + mat.diffuse_texname;
		vk_ctx.create_texture(path.c_str(), model.texture_image, model.texture_image_view,
		model.texture_image_memory);

		vk_ctx.create_vertex_buffer(model.vertices, model.vertex_buffer, model.vertex_buffer_memory);
		vk_ctx.create_index_buffer(model.indices, model.index_buffer, model.index_buffer_memory);

		vk_ctx.allocate_descriptor_sets(model.descriptor_sets);
		vk_ctx.create_descriptor_sets(model.descriptor_sets, model.texture_image_view);

		return model;
	}

	void load_models()
	{
		if (shapes.size() == 0)
		{
			std::string err;
			if (!tinyobj::LoadObj(&attrib, &shapes, &mats, &err,
				"assets/workshop/ps1-style-workshop.obj", "assets/workshop/"))
			{
				throw std::runtime_error(err);
			}
			else if (err.empty() == false)
			{
				std::cout << err << std::endl;
			}
		}

		models.push_back(load_model(shapes[0], mats[0]));
		models.push_back(load_model(shapes[1], mats[1]));
	}

	void create_instance_buffer()
	{
		std::vector<glm::mat4> instances;

		float x = 50.0f;
		for (int i = 0; i < instance_count; i++)
		{
			glm::mat4 m = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			m = glm::translate(m, glm::vec3(x, -600.0f, 1800.0f));

			instances.push_back(m);

			x += 1.5f;
		}

		VkDeviceSize size = sizeof(instances[0]) * instances.size();

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
		vk_ctx.createBuffer(stagingBuffer, stagingBufferMemory,
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* memory;
		vkMapMemory(vk_ctx.device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &memory);
		memcpy(memory, instances.data(), size);
		vkUnmapMemory(vk_ctx.device, stagingBufferMemory);

		vk_ctx.createBuffer(instance_buffer, instance_buffer_memory,
		size,
		(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		vk_ctx.copyBuffer(stagingBuffer, instance_buffer, size);

		vkDestroyBuffer(vk_ctx.device, stagingBuffer, nullptr);
		vkFreeMemory(vk_ctx.device, stagingBufferMemory, nullptr);
	}

	static void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
	    app->vk_ctx.framebuffer_resized = true;

		if ((width == 0) || (height == 0))
		{
			app->minimized = true;
		}
		else
		{
			app->minimized = false;
		}
	}

	static void mouse_callback(GLFWwindow* window, double x, double y)
	{
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));

		static bool first_time = true;
		if (first_time)
		{
			app->mouse_x = x;
			app->mouse_y = y;
			first_time = false;
		}

		float offset_x = x - app->mouse_x;
		float offset_y = y - app->mouse_y;
//		offset_y = 0;
		app->mouse_x = x;
		app->mouse_y = y;

		const float sens = 0.1f;
		offset_x *= sens;
		offset_y *= sens;

		app->yaw += offset_x;
		app->pitch += offset_y;

		if (app->pitch > 89.0f)
		{
			app->pitch = 89.0f;
		}
		else if (app->pitch < -89.0f)
		{
			app->pitch = -89.0f;
		}

		glm::vec3 direction;
		direction.x = cos(glm::radians(app->yaw)) * cos(glm::radians(app->pitch));
		direction.y = sin(glm::radians(app->pitch));
		direction.z = sin(glm::radians(app->yaw)) * cos(glm::radians(app->pitch));

		app->camera_front = glm::normalize(direction);
	}

	void process_input(GLFWwindow* window)
	{
		float camera_speed = 200.5f * dt;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		{
			camera_speed *= 5;
		}

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		{
			camera_pos += camera_speed * camera_front;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		{
			camera_pos -= camera_speed * camera_front;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		{
			camera_pos -= glm::normalize(glm::cross(camera_front, camera_up)) * camera_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		{
			camera_pos += glm::normalize(glm::cross(camera_front, camera_up)) * camera_speed;
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