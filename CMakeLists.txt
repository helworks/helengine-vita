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

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/platform/psvita/PsVitaBootHost.cpp
)

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
