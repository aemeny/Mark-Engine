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
Source/Utils/MarkUtils.h

Source/Renderer/MarkVulkanCore.h
Source/Renderer/MarkVulkanCore.cpp
Source/Renderer/MarkVulkanPhysicalDevices.h
Source/Renderer/MarkVulkanPhysicalDevices.cpp
Source/Renderer/MarkVulkanSwapChain.h
Source/Renderer/MarkVulkanSwapChain.cpp
Source/Renderer/WindowToVulkanHandler.h
Source/Renderer/WindowToVulkanHandler.cpp
Source/Renderer/MarkVulkanCommandBuffers.h
Source/Renderer/MarkVulkanCommandBuffers.cpp
Source/Renderer/MarkVulkanQueue.h
Source/Renderer/MarkVulkanQueue.cpp
Source/Renderer/MarkVulkanRenderPassCache.h
Source/Renderer/MarkVulkanRenderPassCache.cpp
Source/Renderer/MarkVulkanRenderPass.h
Source/Renderer/MarkVulkanRenderPass.cpp
Source/Renderer/MarkVulkanShader.h
Source/Renderer/MarkVulkanShader.cpp
Source/Renderer/MarkVulkanGraphicsPipeline.h
Source/Renderer/MarkVulkanGraphicsPipeline.cpp
Source/Renderer/MarkVulkanGraphicsPipelineCache.h
Source/Renderer/MarkVulkanGraphicsPipelineCache.cpp
Source/Renderer/MarkVulkanVertexBuffer.h
Source/Renderer/MarkVulkanVertexBuffer.cpp
Source/Renderer/MarkVulkanUniformBuffer.h
Source/Renderer/MarkVulkanUniformBuffer.cpp
Source/Renderer/VulkanModelHandler.h
Source/Renderer/VulkanModelHandler.cpp
Source/Renderer/VulkanTextureHandler.h
Source/Renderer/VulkanTextureHandler.cpp

Source/Engine/EarlyCameraController.h
Source/Engine/EarlyCameraController.cpp
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
	MARK_ASSETS_DIR="${MARK_ASSETS_DIR}"
	MARK_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(Core
	PRIVATE
    	Vulkan::Vulkan
        volk
   	glfw
  	glm::glm
	${MARK_GLSLANG_TARGETS}
)