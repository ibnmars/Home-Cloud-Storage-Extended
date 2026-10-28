# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
function(hcse_msvc_path_options output)
    get_filename_component(project_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(prefixes "${project_root}" "$ENV{VCPKG_ROOT}" "${VCPKG_ROOT_DIR}" "${VCPKG_INSTALLED_DIR}" "$ENV{USERPROFILE}")
    list(REMOVE_DUPLICATES prefixes)
    set(options)
    foreach(prefix IN LISTS prefixes)
        if(NOT prefix STREQUAL "")
            file(TO_CMAKE_PATH "${prefix}" prefix)
            string(REGEX REPLACE "/+$" "" prefix "${prefix}")
            list(APPEND options "/d1trimfile:${prefix}/")
        endif()
    endforeach()
    set(${output} "${options}" PARENT_SCOPE)
endfunction()
