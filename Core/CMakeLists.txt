#
# Core
#

set(SOURCES
Source/Core/Window.h
Source/Core/Window.cpp
Source/Renderer/Renderer.h
Source/Renderer/Renderer.cpp
)

add_library(Core STATIC)

target_sources(Core PRIVATE ${SOURCES})

# Match VS filters to directory structure on disk
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

target_compile_definitions(Core 
	PRIVATE 
	GLFW_INCLUDE_NONE)

target_link_libraries(Core
	PRIVATE
    	Vulkan::Vulkan
   	glfw
  	glm::glm
)

target_include_directories(Core PUBLIC "Source" "Extern/stb")