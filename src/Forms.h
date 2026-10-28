// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#pragma once

#include "RE/Skyrim.h"
#include "REL/Relocation.h"
#include <string_view>

namespace HCSE::Forms {
template <class T> T* LookupForm(RE::FormID a_localFormID, std::string_view a_pluginName) {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) {
        return nullptr;
    }
    if (!REL::Module::IsVR()) {
        return data->LookupForm<T>(a_localFormID, a_pluginName);
    }
    auto* file = data->LookupModByName(a_pluginName);
    if (!file || file->compileIndex == 0xFF) {
        return nullptr;
    }
    // Resolve light indices explicitly for VR ESL Support.
    const RE::FormID formID =
        file->IsLight()
            ? 0xFE000000u | (static_cast<std::uint32_t>(file->smallFileCompileIndex) << 12) | (a_localFormID & 0xFFF)
            : (static_cast<std::uint32_t>(file->compileIndex) << 24) | (a_localFormID & 0xFFFFFF);
    auto* form = RE::TESForm::LookupByID(formID);
    return form ? form->As<T>() : nullptr;
}
}
