#
# Core/CMakeLists.txt
#

add_library(Core STATIC)

set(SOURCES
Include/Mark/Engine.h
Source/Engine.cpp
Source/Core.h
Source/Core.cpp
Source/Platform/GLFWContext.h
Source/Platform/GLFWContext.cpp
Source/Platform/Window.h
Source/Platform/Window.cpp
Source/Platform/WindowManager.h
Source/Platform/WindowManager.cpp
Source/Utils/VulkanUtils.h
Source/Utils/ErrorHandling.h
Source/Renderer/MarkVulkanCore.h
Source/Renderer/MarkVulkanCore.cpp
Source/Renderer/MarkVulkanPhysicalDevices.h
Source/Renderer/MarkVulkanPhysicalDevices.cpp
)

target_sources(Core PRIVATE ${SOURCES})

# Match VS filters to directory structure on disk
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

target_include_directories(Core 
	PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(Core 
	PRIVATE 
	GLFW_INCLUDE_NONE
)

target_link_libraries(Core
	PRIVATE
    	Vulkan::Vulkan
        volk
   	glfw
  	glm::glm
)