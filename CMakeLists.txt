# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
cmake_minimum_required(VERSION 3.23)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/project.json" HCSE_METADATA)
string(JSON HCSE_ID GET "${HCSE_METADATA}" id)
string(JSON HCSE_NAME GET "${HCSE_METADATA}" name)
string(JSON HCSE_AUTHOR GET "${HCSE_METADATA}" author)
string(JSON HCSE_VERSION GET "${HCSE_METADATA}" version)
project(${HCSE_ID} VERSION ${HCSE_VERSION} LANGUAGES CXX)

option(HCSE_BUILD_PLUGIN "Build the Windows SKSE DLL" ON)
include(CTest)
if(MSVC)
    include(cmake/MSVCPaths.cmake)
    hcse_msvc_path_options(path_options)
    add_compile_options(${path_options})
endif()

if(HCSE_BUILD_PLUGIN OR BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

add_library(HCSECrafting STATIC src/CraftingSession.cpp)
target_include_directories(HCSECrafting PUBLIC src)
target_compile_features(HCSECrafting PUBLIC cxx_std_23)

if(HCSE_BUILD_PLUGIN)
    find_package(CommonLibSSE CONFIG REQUIRED)
    find_package(minhook CONFIG REQUIRED)
    configure_file(cmake/Version.h.in generated/Version.h @ONLY)
    configure_file(cmake/Version.rc.in generated/Version.rc @ONLY)
    add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR "${HCSE_AUTHOR}" SOURCES
        src/Plugin.cpp src/Storage.cpp src/CraftingHooks.cpp src/HorseIntegration.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.rc")
    target_include_directories(${PROJECT_NAME} PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
    target_link_libraries(${PROJECT_NAME} PRIVATE HCSECrafting minhook::minhook)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:preprocessor)
        target_link_options(${PROJECT_NAME} PRIVATE "/PDBALTPATH:${PROJECT_NAME}.pdb")
    endif()
endif()

if(MSVC)
    target_compile_options(HCSECrafting PRIVATE /W4 /permissive-)
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(HCSE_BUILD_PLUGIN AND CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "Release packaging requires BUILD_TESTING=ON.")
    endif()
    if(EXISTS "${PROJECT_SOURCE_DIR}/dependencies/sources")
        set(dependency_sources "${PROJECT_SOURCE_DIR}/dependencies/sources")
    elseif(DEFINED ENV{VCPKG_DOWNLOADS} AND NOT "$ENV{VCPKG_DOWNLOADS}" STREQUAL "")
        set(dependency_sources "$ENV{VCPKG_DOWNLOADS}")
    elseif(DEFINED VCPKG_ROOT)
        set(dependency_sources "${VCPKG_ROOT}/downloads")
    else()
        set(dependency_sources "${PROJECT_SOURCE_DIR}/build/vcpkg-downloads")
    endif()
    add_custom_target(Package ALL
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure --no-tests=error -C "$<CONFIG>"
        COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/package_release.py"
            --dll "$<TARGET_FILE:${PROJECT_NAME}>" --dependency-sources "${dependency_sources}" --download-sources
        DEPENDS ${PROJECT_NAME} CraftingTests HorseIntegrationTests
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        COMMENT "Testing and creating FOMOD and source archives"
        VERBATIM)
endif()
include(cmake/MSVCIncludes.cmake)
