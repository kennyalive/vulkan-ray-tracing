cmake_minimum_required(VERSION 3.15)
set (TARGET_NAME vulkan-ray-tracing)
project(${TARGET_NAME})

set(PROGRAM_SOURCE
    src/acceleration_structure.cpp
    src/acceleration_structure.h
    src/demo.cpp
    src/demo.h
    src/gpu_mesh.h
    src/lib.cpp
    src/lib.h
    src/linear_algebra.cpp
    src/linear_algebra.h
    src/main.cpp
    src/std.cpp
    src/std.h
    src/triangle_mesh.cpp
    src/triangle_mesh.h
    src/vk.cpp
    src/vk.h
    src/vk_utils.cpp
    src/vk_utils.h
    src/kernels/copy_to_swapchain.cpp
    src/kernels/copy_to_swapchain.h
    src/kernels/draw_mesh.cpp
    src/kernels/draw_mesh.h
    src/kernels/raytrace_scene.cpp
    src/kernels/raytrace_scene.h
)
set(SHADER_ENTRY_POINT_FILES
    src/shaders/copy_to_swapchain.comp.glsl
    src/shaders/raster_mesh.frag.glsl
    src/shaders/raster_mesh.vert.glsl
    src/shaders/rt_mesh.rchit.glsl
    src/shaders/rt_mesh.rgen.glsl
    src/shaders/rt_mesh.rmiss.glsl
)
set(SHADER_OTHER_FILES
    src/shaders/common.glsl
    src/shaders/rt_utils.glsl
)
list(APPEND SHADER_SOURCE ${SHADER_ENTRY_POINT_FILES} ${SHADER_OTHER_FILES})

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE "${CMAKE_SOURCE_DIR}/src" FILES ${PROGRAM_SOURCE})
source_group(TREE "${CMAKE_SOURCE_DIR}/src/shaders" PREFIX shaders FILES ${SHADER_SOURCE})

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/MP /W3)
    # Match MSVC Release
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_compile_options($<$<CONFIG:Release>:/Gy>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF>)
    add_link_options($<$<CONFIG:Release>:/OPT:ICF>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_executable(${TARGET_NAME} ${PROGRAM_SOURCE} ${SHADER_SOURCE})
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_subdirectory(third-party)
target_link_libraries(${TARGET_NAME} third-party)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${TARGET_NAME} PRIVATE
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VS_DPI_AWARE "PerMonitor"
)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})

function(add_shader SHADER)
    get_filename_component(BASE_NAME ${SHADER} NAME_WLE)
    set(SPV_FILE "${CMAKE_SOURCE_DIR}/data/spirv/${BASE_NAME}.spv")
    set(SHADER_FILE "${CMAKE_SOURCE_DIR}/${SHADER}")
    add_custom_command(
        OUTPUT "${SPV_FILE}"
        MAIN_DEPENDENCY "${SHADER_FILE}"
        DEPENDS "${SHADER_OTHER_FILES}"
        COMMAND "$ENV{VULKAN_SDK}/bin/glslangValidator" "${SHADER_FILE}" -V --target-env vulkan1.2 -o "${SPV_FILE}"
        COMMAND "$ENV{VULKAN_SDK}/bin/spirv-opt" ${SPV_FILE} -O --strip-debug -o "${SPV_FILE}"
    )
endfunction()

foreach(SHADER ${SHADER_ENTRY_POINT_FILES})
    add_shader(${SHADER})
endforeach()
