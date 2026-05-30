cmake_minimum_required(VERSION 3.20)
project(GameEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# ---- GLFW + GLM: FetchContent (vcpkg 불필요) ----
include(FetchContent)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

add_executable(engine
    src/main.cpp
    src/game/GameState.cpp
    src/platform/Window.cpp
    src/renderer/VulkanContext.cpp
    src/world/World.cpp
    src/world/TerrainGen.cpp
 "src/game/Camera.h" "src/platform/InputManager.h" "src/platform/InputManager.cpp")

target_include_directories(engine PRIVATE src ${Vulkan_INCLUDE_DIRS})
target_link_libraries(engine PRIVATE Vulkan::Vulkan glfw glm::glm)

# VS 디버거 작업 디렉토리를 exe 위치로 고정 (셰이더 경로 문제 해결)
set_target_properties(engine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:engine>"
)

# ---- Shader compilation ----
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK.")
endif()

set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR $<TARGET_FILE_DIR:engine>/shaders)

set(SHADERS
    ${SHADER_SRC_DIR}/triangle.vert
    ${SHADER_SRC_DIR}/triangle.frag
    ${SHADER_SRC_DIR}/chunk.vert
    ${SHADER_SRC_DIR}/chunk.frag
)

set(SPIRV_OUTPUTS)
foreach(SHADER ${SHADERS})
    get_filename_component(NAME ${SHADER} NAME)
    set(OUT ${CMAKE_BINARY_DIR}/spirv/${NAME}.spv)
    add_custom_command(
        OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/spirv
        COMMAND ${GLSLC} ${SHADER} -o ${OUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${NAME}"
    )
    list(APPEND SPIRV_OUTPUTS ${OUT})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(engine shaders)

# Copy compiled shaders next to the executable
add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/spirv/triangle.vert.spv ${SHADER_OUT_DIR}/triangle.vert.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/spirv/triangle.frag.spv ${SHADER_OUT_DIR}/triangle.frag.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/spirv/chunk.vert.spv    ${SHADER_OUT_DIR}/chunk.vert.spv
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/spirv/chunk.frag.spv    ${SHADER_OUT_DIR}/chunk.frag.spv
)
