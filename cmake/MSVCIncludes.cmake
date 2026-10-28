# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_GENERATOR MATCHES "Ninja")
    # CMake can decode localized cl.exe diagnostics with the wrong OEM codepage.
    # Ninja needs the prefix in the same UTF-8 bytes the compiler actually emits.
    set(probe "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/HCSEIncludes.cpp")
    file(WRITE "${probe}" "#include <cstddef>\n")
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c "${probe}"
        "/Fo${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/HCSEIncludes.obj"
        OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE result ENCODING UTF-8)
    string(REGEX MATCH "(^|[\r\n])([^\r\n]*: +)([A-Za-z]:[/\\\\][^\r\n]*)" match "${output}\n${errors}")
    if(NOT result EQUAL 0 OR NOT match)
        message(FATAL_ERROR "Could not determine MSVC's include prefix; use an x64 Native Tools environment.")
    endif()
    string(REGEX REPLACE " +$" " " prefix "${CMAKE_MATCH_2}")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${prefix}")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${prefix}")
    message(STATUS "MSVC include prefix: ${prefix}")
endif()
