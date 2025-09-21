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

# Projects
add_subdirectory(Core)
add_subdirectory(App)