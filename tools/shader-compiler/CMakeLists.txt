cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

project(helengine_psvita_shader_compiler LANGUAGES C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/PsVitaShaderCompilerJobReader.cpp
    src/PsVitaShaderCompilerQueueProcessor.cpp
    src/PsVitaShaderCompilerResultWriter.cpp
    ../../src/platform/psvita/shaders/PsVitaShaderArtifactWriter.cpp
    ../../src/platform/psvita/shaders/PsVitaShaderArtifactHash.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ../../src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SceIofilemgr_stub
    SceKernelModulemgr_stub
    SceLibKernel_stub
    SceShaccCg_stub
    SceShaccCgExt
    taihen_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk HLEN00002 ${PROJECT_NAME}.self
    VERSION "01.00"
    NAME "Helengine Vita Shader Compiler"
    FILE "${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/icon0.png" "sce_sys/icon0.png"
)
