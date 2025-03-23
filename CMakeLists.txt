cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_SOURCES
	"src/main.cpp"
    "src/game.cpp"
    "src/vulkan_engine.cpp"
)

set(IMGUI_SOURCES
	external/imgui/imgui.cpp
	external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
)

project(Sylva VERSION 1.0 LANGUAGES CXX)

add_executable(Sylva)

target_sources(Sylva PRIVATE ${APP_SOURCES})

# Nets library
execute_process(
    COMMAND meson setup ${CMAKE_BINARY_DIR}/meson_build ${CMAKE_SOURCE_DIR}/external/nets
    COMMAND meson compile -C ${CMAKE_BINARY_DIR}/meson_build
)

add_library(Nets STATIC IMPORTED)

set(NETS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/nets/include)
set(NETS_LIB_DIR ${CMAKE_BINARY_DIR}/meson_build)

set_target_properties(Nets PROPERTIES
    IMPORTED_LOCATION ${NETS_LIB_DIR}/libnets.dll.a
    INTERFACE_INCLUDE_DIRECTORIES ${NETS_INCLUDE_DIR}
)

add_library(ImGui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
)

target_include_directories(ImGui PUBLIC external/imgui)

target_include_directories(Sylva PRIVATE
    external/collection/include
    external/cimg
)

find_package(fastgltf REQUIRED)
find_package(glm REQUIRED)
find_package(sdl3 REQUIRED)
find_package(vk-bootstrap REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Volk REQUIRED)

target_link_libraries(Sylva PRIVATE
    Nets
    ImGui
    fastgltf
    glm::glm
    sdl3
    vk-bootstrap
    Vulkan::Vulkan
    -lstdc++exp
    -lwsock32
    Volk
)

find_program(GLSL_VALIDATOR glslangValidator)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")

    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")

    message(STATUS ${GLSL})
    message(STATUS COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV})

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(Sylva Shaders)