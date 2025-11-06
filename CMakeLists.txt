cmake_minimum_required(VERSION 3.20)

project(Mark
	VERSION 0.1.0
	HOMEPAGE_URL "https://github.com/aemeny/Mark-Engine"
	LANGUAGES CXX
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

# Projects
add_subdirectory(Core)
add_subdirectory(App)