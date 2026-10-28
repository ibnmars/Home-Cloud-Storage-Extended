# Copyright (C) 2026 IbnMars
# SPDX-License-Identifier: GPL-3.0-or-later
# Additional permissions: see EXCEPTIONS.md.
add_executable(CraftingTests CraftingTests.cpp)
target_link_libraries(CraftingTests PRIVATE HCSECrafting)
add_test(NAME CraftingRules COMMAND CraftingTests)
add_executable(HorseIntegrationTests HorseIntegrationTests.cpp)
target_compile_features(HorseIntegrationTests PRIVATE cxx_std_23)
target_include_directories(HorseIntegrationTests PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME HorseIntegrationRules COMMAND HorseIntegrationTests)
add_test(NAME PackagingRules COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/PackagingTests.py")
