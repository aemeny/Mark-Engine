#
# Core/CMakeLists.txt
#

add_library(Core STATIC)

set(SOURCES
Include/Mark/Engine.h
Include/Mark/Settings/imguiSettings.h

Extern/Include/stb/stb_image.h
Extern/Include/objloader/tiny_obj_loader.h

Source/Engine.cpp
Source/Core.h
Source/Core.cpp

Source/Platform/GLFWContext.h
Source/Platform/GLFWContext.cpp
Source/Platform/Window.h
Source/Platform/Window.cpp
Source/Platform/WindowManager.h
Source/Platform/WindowManager.cpp
Source/Platform/imguiHandler.h
Source/Platform/imguiHandler.cpp

Source/Utils/VulkanUtils.h
Source/Utils/ErrorHandling.h
Source/Utils/Mark_Utils.h
Source/Utils/TimeTracker.h
Source/Utils/TimeTracker.cpp

Source/Renderer/Mark_VulkanCore.h
Source/Renderer/Mark_VulkanCore.cpp
Source/Renderer/Mark_VulkanPhysicalDevices.h
Source/Renderer/Mark_VulkanPhysicalDevices.cpp
Source/Renderer/Mark_VulkanSwapChain.h
Source/Renderer/Mark_VulkanSwapChain.cpp
Source/Renderer/Mark_WindowToVulkanHandler.h
Source/Renderer/Mark_WindowToVulkanHandler.cpp
Source/Renderer/Mark_VulkanCommandBuffers.h
Source/Renderer/Mark_VulkanCommandBuffers.cpp
Source/Renderer/Mark_VulkanQueue.h
Source/Renderer/Mark_VulkanQueue.cpp
Source/Renderer/Mark_VulkanShader.h
Source/Renderer/Mark_VulkanShader.cpp
Source/Renderer/Mark_VulkanGraphicsPipeline.h
Source/Renderer/Mark_VulkanGraphicsPipeline.cpp
Source/Renderer/Mark_VulkanGraphicsPipelineCache.h
Source/Renderer/Mark_VulkanGraphicsPipelineCache.cpp
Source/Renderer/Mark_VulkanVertexBuffer.h
Source/Renderer/Mark_VulkanVertexBuffer.cpp
Source/Renderer/Mark_VulkanUniformBuffer.h
Source/Renderer/Mark_VulkanUniformBuffer.cpp
Source/Renderer/Mark_VulkanModelHandler.h
Source/Renderer/Mark_VulkanModelHandler.cpp
Source/Renderer/Mark_VulkanTextureHandler.h
Source/Renderer/Mark_VulkanTextureHandler.cpp
Source/Renderer/Mark_imguiRenderer.h
Source/Renderer/Mark_imguiRenderer.cpp

Source/Engine/EarlyCameraController.h
Source/Engine/EarlyCameraController.cpp
Source/Engine/EngineStats.h
Source/Engine/EngineStats.cpp
Source/Engine/SettingsHandler.h
Source/Engine/SettingsHandler.cpp
)

target_sources(Core PRIVATE ${SOURCES})

# Match VS filters to directory structure on disk
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

target_include_directories(Core 
	PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Extern"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Handling"
)

target_compile_definitions(Core 
	PRIVATE 
	GLFW_INCLUDE_NONE
	MARK_ASSETS_DIR="${MARK_ASSETS_DIR}"
	MARK_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
	MARK_FALLBACK_MODEL="${MARK_FALLBACK_MODEL}"
	MARK_ENGINE_CORE_DIR="${MARK_ENGINE_CORE_DIR}"
)

target_link_libraries(Core
	PRIVATE
    	Vulkan::Vulkan
        volk
   	glfw
  	glm::glm
	imgui
	${MARK_GLSLANG_TARGETS}
	${MARK_KTX_TARGET}
)