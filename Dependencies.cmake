#
# Dependencies/CMakeLists.txt (Vulkan + GLFW + GLM)
#
include(FetchContent)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


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

set(MARK_GLSLANG_TARGETS
    glslang::glslang
    glslang::SPIRV
    glslang::glslang-default-resource-limits
    CACHE INTERNAL "glslang targets to link"
)


### Dear ImGui
FetchContent_Declare(imgui
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5.zip
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
  FetchContent_Populate(imgui)

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp

    # Backends: GLFW (platform) + Vulkan (renderer)
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  )

  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
  )

  target_link_libraries(imgui PUBLIC
    glfw               
    Vulkan::Vulkan
  )

  target_compile_features(imgui PUBLIC cxx_std_23)
endif()


# ---------- libktx (KTX2 + BasisU) ----------
set(KTX_FEATURE_TOOLS          OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC            OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS          OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS  OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD      OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VULKAN         ON  CACHE BOOL "" FORCE)

FetchContent_Declare(ktx
  GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
  GIT_TAG        v4.4.2
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
)
FetchContent_MakeAvailable(ktx)

# Helper
function(_set_folder_if_real tgt folder)
  if (TARGET ${tgt})
    get_target_property(_aliased ${tgt} ALIASED_TARGET)
    if (NOT _aliased)  # skip ALIAS targets like KTX::ktx
      set_target_properties(${tgt} PROPERTIES FOLDER "${folder}")
    endif()
  endif()
endfunction()

set(_ktx_targets
  ktx           # main static lib
  ktx_read      # small util
  ktx_version   # version obj
  objUtil
  obj_basisu_cbind
  astcenc-avx2-static
)

foreach(_t IN LISTS _ktx_targets)
  _set_folder_if_real(${_t} "Dependencies/KTX")
endforeach()


if (TARGET KTX::ktx)
  set(MARK_KTX_TARGET KTX::ktx CACHE INTERNAL "")
else()
  set(MARK_KTX_TARGET ktx CACHE INTERNAL "")
endif()
if (TARGET imgui)
  set_target_properties(imgui PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET volk)
  set_target_properties(volk PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET glfw)
  set_target_properties(glfw PROPERTIES FOLDER "Dependencies")
endif()
if (TARGET glm)
  set_target_properties(glm PROPERTIES FOLDER "Dependencies")
endif()