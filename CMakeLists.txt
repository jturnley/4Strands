cmake_minimum_required(VERSION 3.25)

project(4Strands VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The engine-independent fluid solver builds on its own, so it can be tested without the game.
add_library(FourStrandsFluid STATIC
	src/fluid/Clusters.cpp
	src/fluid/Ground.cpp
	src/fluid/Skin.cpp
	src/fluid/Solver.cpp
	src/fluid/Surface.cpp
)
target_include_directories(FourStrandsFluid PUBLIC src)

# TODO: pick the CommonLibF4 fork that matches the target Fallout 4 build, clone it into
# extern/CommonLibF4, and adjust the target name below and the plugin entry point in src/main.cpp.
set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibF4")
if(NOT EXISTS "${COMMONLIB_DIR}/CMakeLists.txt")
	message(WARNING "CommonLibF4 is missing from extern/CommonLibF4; only the fluid library is built.")
	return()
endif()

add_subdirectory("${COMMONLIB_DIR}" CommonLibF4 EXCLUDE_FROM_ALL)

add_library(${PROJECT_NAME} SHARED
	src/main.cpp
)

find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h" REQUIRED)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "4Strands")
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(${PROJECT_NAME} PRIVATE src ${SIMPLEINI_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE FourStrandsFluid CommonLibF4::CommonLibF4)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
