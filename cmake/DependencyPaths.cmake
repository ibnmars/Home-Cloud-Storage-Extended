# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
include_guard(DIRECTORY)
if(MSVC)
    include("${CMAKE_CURRENT_LIST_DIR}/MSVCPaths.cmake")
    hcse_msvc_path_options(path_options)
    add_compile_options(${path_options})
endif()
