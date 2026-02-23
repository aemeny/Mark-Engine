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
Source/Utils/Mark_FatalHandling.h
Source/Utils/Mark_Utils.h
Source/Utils/TimeTracker.h
Source/Utils/TimeTracker.cpp

Source/Renderer/Vulkan/Mark_VulkanCore.h
Source/Renderer/Vulkan/Mark_VulkanCore.cpp
Source/Renderer/Vulkan/Mark_PhysicalDevices.h
Source/Renderer/Vulkan/Mark_PhysicalDevices.cpp
Source/Renderer/Vulkan/Mark_SwapChain.h
Source/Renderer/Vulkan/Mark_SwapChain.cpp
Source/Renderer/Vulkan/Mark_WindowToVulkanHandler.h
Source/Renderer/Vulkan/Mark_WindowToVulkanHandler.cpp
Source/Renderer/Vulkan/Mark_CommandBuffers.h
Source/Renderer/Vulkan/Mark_CommandBuffers.cpp
Source/Renderer/Vulkan/Mark_Queue.h
Source/Renderer/Vulkan/Mark_Queue.cpp
Source/Renderer/Vulkan/Mark_WindowQueueHelper.h
Source/Renderer/Vulkan/Mark_WindowQueueHelper.cpp
Source/Renderer/Vulkan/Mark_Shader.h
Source/Renderer/Vulkan/Mark_Shader.cpp
Source/Renderer/Vulkan/Mark_GraphicsPipeline.h
Source/Renderer/Vulkan/Mark_GraphicsPipeline.cpp
Source/Renderer/Vulkan/Mark_GraphicsPipelineCache.h
Source/Renderer/Vulkan/Mark_GraphicsPipelineCache.cpp
Source/Renderer/Vulkan/Mark_VertexBuffer.h
Source/Renderer/Vulkan/Mark_VertexBuffer.cpp
Source/Renderer/Vulkan/Mark_UniformBuffer.h
Source/Renderer/Vulkan/Mark_UniformBuffer.cpp
Source/Renderer/Vulkan/Mark_ModelHandler.h
Source/Renderer/Vulkan/Mark_ModelHandler.cpp
Source/Renderer/Vulkan/Mark_TextureHandler.h
Source/Renderer/Vulkan/Mark_TextureHandler.cpp
Source/Renderer/Vulkan/Mark_imguiRenderer.h
Source/Renderer/Vulkan/Mark_imguiRenderer.cpp
Source/Renderer/Vulkan/Mark_BufferAndMemoryHelper.h
Source/Renderer/Vulkan/Mark_BufferAndMemoryHelper.cpp
Source/Renderer/Vulkan/Mark_ComputePipeline.h
Source/Renderer/Vulkan/Mark_ComputePipeline.cpp

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