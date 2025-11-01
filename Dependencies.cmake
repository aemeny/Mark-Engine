#
# Dependencies/CMakeLists.txt (Vulkan + GLFW + GLM)
#
include(FetchContent)

# Vulkan
find_package(Vulkan REQUIRED) # Vulkan::Vulkan

# Volk (Vulkan function loader)
find_package(volk QUIET)
if (NOT volk_FOUND)
  message(STATUS "volk not found; fetching...")
  include(FetchContent)
  FetchContent_Declare(volk
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/zeux/volk/archive/refs/tags/1.4.304.zip
  )
  FetchContent_MakeAvailable(volk)    # creates target: volk
endif()

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

### glslang
include(FetchContent)
set(ENABLE_GLSLANG_BINARIES    OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL                OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER         OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING              OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST               OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT                 OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glslang
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(glslang)

# Expose a variable so subprojects can link cleanly
set(MARK_GLSLANG_TARGETS
    glslang::glslang
    glslang::SPIRV
    glslang::glslang-default-resource-limits
    CACHE INTERNAL "glslang targets to link"
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if (TARGET volk)
  set_target_properties(volk PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET glfw)
  set_target_properties(glfw PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET glm)
  set_target_properties(glm PROPERTIES FOLDER "Dependencies")
endif()