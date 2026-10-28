// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "Storage.h"

#include "Forms.h"
#include "Version.h"

#include "SKSE/SKSE.h"

namespace HCSE::Storage {
namespace {
constexpr RE::FormID kContainerListID = 0x800;
constexpr RE::FormID kSharedContainerID = 0x802;
constexpr std::string_view kHomeCloudStorage = "Home Cloud Storage.esp";
RE::FormID containerListID = 0;
RE::FormID sharedContainerID = 0;
}

bool Initialize() {
    auto* list = Forms::LookupForm<RE::BGSListForm>(kContainerListID, Version::kPluginFile);
    containerListID = list ? list->GetFormID() : 0;
    auto* storage = Forms::LookupForm<RE::TESObjectREFR>(kSharedContainerID, kHomeCloudStorage);
    sharedContainerID = storage ? storage->GetFormID() : 0;
    if (!list) {
        SKSE::log::error("Could not resolve {}|0x{:03X}; cloud crafting disabled", Version::kPluginFile,
                         kContainerListID);
    }
    if (!storage) {
        SKSE::log::error("Could not resolve the Home Cloud Storage backend");
    }
    return IsReady();
}

bool IsReady() {
    return containerListID != 0;
}
std::uint32_t GetSharedContainerHandle() {
    auto* storage = RE::TESForm::LookupByID<RE::TESObjectREFR>(sharedContainerID);
    return storage ? storage->GetHandle().native_handle() : 0;
}

std::vector<std::uint32_t> GetCraftingContainerHandles() {
    auto* list = RE::TESForm::LookupByID<RE::BGSListForm>(containerListID);
    if (!list) {
        return {};
    }
    std::vector<ContainerCandidate> candidates;
    list->ForEachForm([&](RE::TESForm* a_form) {
        if (a_form) {
            auto* ref = a_form->As<RE::TESObjectREFR>();
            auto* base = ref ? ref->GetBaseObject() : nullptr;
            const bool valid = base && base->GetFormType() == RE::FormType::Container;
            if (valid) {
                candidates.push_back({a_form->GetFormID(), ref->CreateRefHandle().native_handle(), true});
            } else {
                SKSE::log::warn("Ignoring non-container reference {:08X} in HCSE_ContainerList", a_form->GetFormID());
            }
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return SelectContainers(candidates);
}
}
