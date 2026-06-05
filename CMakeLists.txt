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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(VITA_APP_NAME "Helengine PS Vita")
set(VITA_TITLEID "HLEN00001")
set(VITA_VERSION "01.00")

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
endif()

add_executable(${PROJECT_NAME} ${HELENGINE_PSVITA_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

if(DEFINED HELENGINE_CORE_CPP_ROOT AND NOT HELENGINE_CORE_CPP_ROOT STREQUAL "")
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSVITA_HAS_GENERATED_CORE=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${HELENGINE_CORE_CPP_ROOT})
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSVITA_HAS_GENERATED_CORE=0)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    SceDisplay_stub
    SceLibKernel_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
)
