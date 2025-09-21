#
# Dependencies (Vulkan + GLFW + GLM)
#
include(FetchContent)

# Vulkan
find_package(Vulkan REQUIRED) # Vulkan::Vulkan

# GLFW
if (NOT TARGET glfw)
  find_package(glfw3 3.4 QUIET)     # package name glfw3, target is 'glfw'
  if (NOT glfw3_FOUND)
    message(STATUS "GLFW 3.4 not found; fetching...")
    # set options BEFORE populate
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(glfw
      DOWNLOAD_EXTRACT_TIMESTAMP OFF
      URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    )
    FetchContent_MakeAvailable(glfw)   # creates target: glfw
  endif()
endif()

# GLM
if (NOT TARGET glm::glm)
  find_package(glm 1.0.1 CONFIG QUIET) # glm::glm
  if (NOT glm_FOUND)
    message(STATUS "GLM not found; fetching...")
    set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glm
      DOWNLOAD_EXTRACT_TIMESTAMP OFF
      URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
    )
    FetchContent_MakeAvailable(glm)    # creates glm and glm::glm
  endif()
endif()

if (TARGET glfw)
  set_target_properties(glfw PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET glm)
  set_target_properties(glm PROPERTIES FOLDER "Dependencies")
endif()

set_target_properties(glm PROPERTIES FOLDER "Dependencies")