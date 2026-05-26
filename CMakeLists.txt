# CMakeList.txt : CMake project for vulkan-tinker, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("vulkan-tinker")

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glm REQUIRED)
find_package(Stb REQUIRED)

# Add source to this project's executable.
add_executable (vulkan-tinker vulkan-tinker.cpp vendor/imgui_impl_glfw.cpp vendor/imgui_impl_vulkan.cpp vendor/imgui.cpp vendor/imgui_draw.cpp vendor/imgui_demo.cpp vendor/imgui_tables.cpp vendor/imgui_widgets.cpp)

target_link_libraries(vulkan-tinker PRIVATE glfw)
target_link_libraries(vulkan-tinker PRIVATE Vulkan::Vulkan)
target_link_libraries(vulkan-tinker PRIVATE glm::glm)
target_include_directories(vulkan-tinker PRIVATE ${Stb_INCLUDE_DIR})
target_include_directories(vulkan-tinker PRIVATE vendor)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vulkan-tinker PROPERTY CXX_STANDARD 20)
  set_property(TARGET vulkan-tinker PROPERTY DENABLE_CPP20_MODULE ON)
endif()

# Define the source and destination
set(ASSET_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(DEST_FOLDER "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets")

# Copy the folder during the build process
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASSET_FOLDER}"
        "${DEST_FOLDER}"
    COMMENT "Copying assets to build directory..."
)

# TODO: Add tests and install targets if needed.
