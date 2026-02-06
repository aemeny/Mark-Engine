cmake_minimum_required(VERSION 3.31)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

project(Mark
	VERSION 0.1.0
	HOMEPAGE_URL "https://github.com/aemeny/Mark-Engine"
	LANGUAGES C CXX
)

# Required C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
include(Dependencies.cmake)

# ---- Asset root ----
set(MARK_ASSETS_DIR "" CACHE PATH "Absolute path to the App/Assets directory")
if (NOT MARK_ASSETS_DIR)
  # Default to repo_root/App/Assets
  set(MARK_ASSETS_DIR "${CMAKE_SOURCE_DIR}/App/Assets")
endif()
# Normalize for Windows
file(TO_CMAKE_PATH "${MARK_ASSETS_DIR}" MARK_ASSETS_DIR)

# ---- Engine Core root ----
set(MARK_ENGINE_CORE_DIR "" CACHE STRING "Relative path (from repo root) to Engine Core")
if (NOT MARK_ENGINE_CORE_DIR)
  # Default to repo_root/Core
  set(MARK_ENGINE_CORE_DIR "${CMAKE_SOURCE_DIR}/Core")
endif()
# Normalize for Windows
file(TO_CMAKE_PATH "${MARK_ENGINE_CORE_DIR}" MARK_ENGINE_CORE_DIR)

# ---- Engine fallback assets ----
# Path is RELATIVE to repo root (CMAKE_SOURCE_DIR)
set(MARK_FALLBACK_MODEL "" CACHE STRING "Relative path (from repo root) to engine fallback OBJ")
if (NOT MARK_FALLBACK_MODEL)
  set(MARK_FALLBACK_MODEL "${CMAKE_SOURCE_DIR}/Core/Handling/Fallback/Mark_Fallback_Model.obj")
endif()
# Normalize for Windows
file(TO_CMAKE_PATH "${MARK_FALLBACK_MODEL}" MARK_FALLBACK_MODEL)


# Projects
add_subdirectory(Core)
add_subdirectory(App)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT App)
endif()