cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

project(helengine_psvita LANGUAGES C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(VITA_APP_NAME "Helengine PS Vita")
set(VITA_TITLEID "HLEN00001")
set(VITA_VERSION "01.00")
set(HELENGINE_PSVITA_VPK_FILE_ARGS)

set(HELENGINE_PSVITA_SOURCES
    src/main.cpp
    src/platform/psvita/PsVitaBootHost.cpp
)

if(DEFINED HELENGINE_CORE_CPP_ROOT AND NOT HELENGINE_CORE_CPP_ROOT STREQUAL "")
    if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helcpp_config.hpp")
        message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp. Regenerate helengine.core C++ output first.")
    endif()

    if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/helengine_core_unity.cpp")
        set(HELENGINE_CORE_TRANSLATION_UNIT "${HELENGINE_CORE_CPP_ROOT}/helengine_core_unity.cpp")
    elseif(EXISTS "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp")
        set(HELENGINE_CORE_TRANSLATION_UNIT "${HELENGINE_CORE_CPP_ROOT}/helengine_core_amalgamated.cpp")
    else()
        message(FATAL_ERROR "HELENGINE_CORE_CPP_ROOT does not contain helengine_core_unity.cpp or helengine_core_amalgamated.cpp. Regenerate helengine.core C++ output first.")
    endif()

    list(APPEND HELENGINE_PSVITA_SOURCES "${HELENGINE_CORE_TRANSLATION_UNIT}")

    if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp")
        list(APPEND HELENGINE_PSVITA_SOURCES "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp")
    endif()

    if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp")
        list(APPEND HELENGINE_PSVITA_SOURCES "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp")
    endif()

    if(EXISTS "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
        list(APPEND HELENGINE_PSVITA_SOURCES "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp")
    endif()

    list(APPEND HELENGINE_PSVITA_SOURCES
        src/platform/psvita/PsVitaInputBackend.cpp
        src/platform/psvita/PsVitaRuntimeDiagnosticsProvider.cpp
        src/platform/psvita/PsVitaRuntimeSceneCatalogFactory.cpp
        src/platform/psvita/rendering/PsVitaGpuTexture.cpp
        src/platform/psvita/rendering/PsVitaGxmRenderer.cpp
        src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.cpp
        src/platform/psvita/rendering/PsVitaPackedModelReader.cpp
        src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
        src/platform/psvita/rendering/PsVitaRenderManager3D.cpp
        src/platform/psvita/rendering/PsVitaRuntimeModel.cpp
        src/platform/psvita/rendering/PsVitaRuntimeSubmesh.cpp
        src/platform/psvita/rendering/PsVitaRuntimeTexture.cpp
        src/platform/psvita/rendering/PsVitaTextureCache.cpp
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cooked")
    file(GLOB_RECURSE HELENGINE_PSVITA_COOKED_FILES
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cooked/*")

    foreach(cooked_file IN LISTS HELENGINE_PSVITA_COOKED_FILES)
        if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${cooked_file}")
            list(APPEND HELENGINE_PSVITA_VPK_FILE_ARGS FILE "${CMAKE_CURRENT_SOURCE_DIR}/${cooked_file}" "${cooked_file}")
        endif()
    endforeach()
endif()

add_executable(${PROJECT_NAME} ${HELENGINE_PSVITA_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

if(DEFINED HELENGINE_CORE_CPP_ROOT AND NOT HELENGINE_CORE_CPP_ROOT STREQUAL "")
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSVITA_HAS_GENERATED_CORE=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${HELENGINE_CORE_CPP_ROOT})
    target_compile_options(${PROJECT_NAME} PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/psvita/PsVitaGeneratedCoreCompatibility.hpp
    )
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSVITA_HAS_GENERATED_CORE=0)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    pthread
    vita2d
    SceAppMgr_stub
    SceCommonDialog_stub
    SceCtrl_stub
    SceDisplay_stub
    SceGxm_stub
    SceLibKernel_stub
    ScePgf_stub
    ScePvf_stub
    SceShaccCg_stub
    SceSysmodule_stub
    SceTouch_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    ${HELENGINE_PSVITA_VPK_FILE_ARGS}
)
