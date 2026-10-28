// Copyright (C) 2026 IbnMars
// Portions Copyright (c) 2026 ohfor; see licenses/scie/LICENSE.txt.
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "CraftingHooks.h"

#include "CraftingSession.h"
#include "PendingHooks.h"
#include "Storage.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>

namespace HCSE::CraftingHooks {
namespace {
using GetContainerItemCount = std::int32_t (*)(RE::TESObjectREFR*, bool, bool);
using GetInventoryItemEntryAtIdx = RE::InventoryEntryData* (*)(RE::TESObjectREFR*, std::int32_t, bool);
using GetInventoryItemCount = std::int32_t (*)(RE::InventoryChanges*, RE::TESBoundObject*, void*);
using GetInventoryItemCountMember = std::int16_t (*)(RE::InventoryChanges*, RE::TESBoundObject*);
// MSVC's returned ObjectRefHandle uses the hidden result pointer after `this`.
using RemoveItem = RE::ObjectRefHandle* (*)(RE::TESObjectREFR*, RE::ObjectRefHandle*, RE::TESBoundObject*, std::int32_t,
                                            RE::ITEM_REMOVE_REASON, RE::ExtraDataList*, RE::TESObjectREFR*,
                                            const RE::NiPoint3*, const RE::NiPoint3*);
// AE 16916 (1.6.1170 RVA 0x258BB0) checks materials only and returns bool in AL.
using RecipeAvailability = bool (*)(const RE::BGSConstructibleObject*);

GetContainerItemCount originalGetContainerItemCount = nullptr;
GetInventoryItemEntryAtIdx originalGetInventoryItemEntryAtIdx = nullptr;
GetInventoryItemCount originalGetInventoryItemCount = nullptr;
GetInventoryItemCount originalGetInventoryItemCountVR = nullptr;
GetInventoryItemCountMember originalGetInventoryItemCountMemberVR = nullptr;
RemoveItem originalRemoveItem = nullptr;
RecipeAvailability originalRecipeAvailability = nullptr;
CraftingSession session;
std::recursive_mutex sessionMutex;
bool installed = false;

SourceInventory GetSnapshot(std::uint32_t a_handle, RE::TESBoundObject* a_item = nullptr) {
    SourceInventory result{a_handle, 0, {}};
    auto ref = RE::TESObjectREFR::LookupByHandle(a_handle);
    if (!ref) {
        return result;
    }
    auto inventory =
        ref->GetInventory([a_item](RE::TESBoundObject& a_object) { return !a_item || &a_object == a_item; });
    if (!a_item) {
        result.entryCount = originalGetContainerItemCount(ref.get(), false, true);
    }
    for (const auto& [object, data] : inventory) {
        if (!object || !data.second) {
            continue;
        }
        InventoryItem item{object->GetFormID(), data.first, data.second->IsQuestObject(), {}};
        if (a_item && data.second->extraLists) {
            for (auto* list : *data.second->extraLists) {
                if (list) {
                    item.instances.push_back({reinterpret_cast<std::uintptr_t>(list), list->GetCount()});
                }
            }
        }
        result.items.push_back(std::move(item));
    }
    return result;
}

void RefreshSession() {
    std::vector<SourceInventory> sources;
    for (const auto& source : session.GetSources()) {
        sources.push_back(GetSnapshot(source.refHandle));
    }
    session.Refresh(sources);
}

StationType GetStationType(const RE::TESFurniture& a_furniture) {
    using BenchType = RE::TESFurniture::WorkBenchData::BenchType;
    switch (a_furniture.workBenchData.benchType.get()) {
    case BenchType::kCreateObject:
        return StationType::kCrafting;
    case BenchType::kSmithingWeapon:
    case BenchType::kSmithingArmor:
        return StationType::kTempering;
    case BenchType::kEnchanting:
        return StationType::kEnchanting;
    case BenchType::kAlchemy:
        return StationType::kAlchemy;
    default:
        return StationType::kUnknown;
    }
}

bool EnsureSessionActive() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* ui = RE::UI::GetSingleton();
    if (!player || !ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || !Storage::IsReady()) {
        session.Reset();
        return false;
    }
    const auto furnitureHandle = player->GetOccupiedFurniture();
    auto furniture = furnitureHandle.get();
    auto* base = furniture ? furniture->GetBaseObject() : nullptr;
    auto* workbench = base ? base->As<RE::TESFurniture>() : nullptr;
    if (!workbench || workbench->workBenchData.benchType.get() == RE::TESFurniture::WorkBenchData::BenchType::kNone) {
        session.Reset();
        return false;
    }
    if (!session.IsCurrentFurniture(furnitureHandle.native_handle())) {
        std::vector<SourceInventory> sources{GetSnapshot(player->CreateRefHandle().native_handle())};
        for (const auto handle : Storage::GetCraftingContainerHandles()) {
            auto snapshot = GetSnapshot(handle);
            if (snapshot.entryCount > 0) {
                sources.push_back(std::move(snapshot));
            }
        }
        session.Begin(furnitureHandle.native_handle(), GetStationType(*workbench), sources);
    }
    if (session.NeedsRefresh()) {
        RefreshSession();
    }
    return true;
}

bool ShouldPullFormType(RE::FormType a_type) {
    switch (session.GetStationType()) {
    case StationType::kTempering:
        return a_type == RE::FormType::Misc;
    case StationType::kEnchanting:
        return a_type == RE::FormType::Misc || a_type == RE::FormType::SoulGem;
    case StationType::kAlchemy:
        return a_type == RE::FormType::Ingredient || a_type == RE::FormType::AlchemyItem;
    default:
        return true;
    }
}

std::int32_t GetCombinedItemCount(RE::TESBoundObject* a_item, void* a_filter, GetInventoryItemCount a_getCount) {
    if (!a_item) {
        return 0;
    }
    if (!a_filter) {
        return session.GetItemCount(a_item->GetFormID());
    }
    std::vector<std::int32_t> counts;
    for (const auto& source : session.GetSources()) {
        auto ref = RE::TESObjectREFR::LookupByHandle(source.refHandle);
        auto* inventory = ref ? ref->GetInventoryChanges() : nullptr;
        if (inventory) {
            // Keep the native predicate opaque; it includes the game's quest-item rules.
            counts.push_back(a_getCount(inventory, a_item, a_filter));
        }
    }
    return SumItemCounts(counts);
}

std::int32_t HookGetContainerItemCount(RE::TESObjectREFR* a_ref, bool a_useMerchant, bool a_playable) {
    std::lock_guard lock(sessionMutex);
    if (!a_ref || !a_ref->IsPlayerRef() || !EnsureSessionActive()) {
        return originalGetContainerItemCount(a_ref, a_useMerchant, a_playable);
    }
    return session.GetEntryCount();
}

RE::InventoryEntryData* HookGetInventoryItemEntryAtIdx(RE::TESObjectREFR* a_ref, std::int32_t a_index,
                                                       bool a_useMerchant) {
    std::lock_guard lock(sessionMutex);
    if (!a_ref || !a_ref->IsPlayerRef() || !EnsureSessionActive()) {
        return originalGetInventoryItemEntryAtIdx(a_ref, a_index, a_useMerchant);
    }
    // Index zero starts the menu's next enumeration, including when that entry is filtered out.
    if (a_index == 0) {
        session.BeginEnumeration();
    }
    const auto* source = session.GetSourceForIndex(a_index);
    auto ref = source ? RE::TESObjectREFR::LookupByHandle(source->refHandle) : RE::NiPointer<RE::TESObjectREFR>{};
    if (!source || !ref) {
        return nullptr;
    }
    auto* rawEntry = originalGetInventoryItemEntryAtIdx(ref.get(), a_index - source->startIndex, false);
    if (!rawEntry || reinterpret_cast<std::uintptr_t>(rawEntry) < 0x10000) {
        return nullptr;
    }
    // The engine returns an allocated entry with its own list nodes, borrowing the inventory's ExtraDataLists.
    std::unique_ptr<RE::InventoryEntryData> entry(rawEntry);
    auto* item = entry->object;
    if (!item || entry->IsQuestObject() || (!ref->IsPlayerRef() && !ShouldPullFormType(item->GetFormType()))) {
        return nullptr;
    }
    const bool keepInstances = item->IsWeapon() || item->IsArmor() || item->GetFormType() == RE::FormType::SoulGem;
    if (!session.IncludeEntry(item->GetFormID(), keepInstances)) {
        return nullptr;
    }
    if (!keepInstances) {
        if (const auto count = session.GetItemCount(item->GetFormID()); count > 0) {
            entry->countDelta = count;
        }
    }
    return entry.release();
}

std::int32_t HookGetInventoryItemCount(RE::InventoryChanges* a_inventory, RE::TESBoundObject* a_item, void* a_filter) {
    std::lock_guard lock(sessionMutex);
    if (!EnsureSessionActive()) {
        return originalGetInventoryItemCount(a_inventory, a_item, a_filter);
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || a_inventory != player->GetInventoryChanges()) {
        return originalGetInventoryItemCount(a_inventory, a_item, a_filter);
    }
    return GetCombinedItemCount(a_item, a_filter, originalGetInventoryItemCount);
}

std::int32_t HookGetInventoryItemCountVR(RE::InventoryChanges* a_inventory, RE::TESBoundObject* a_item,
                                         void* a_filter) {
    std::lock_guard lock(sessionMutex);
    if (!EnsureSessionActive()) {
        return originalGetInventoryItemCountVR(a_inventory, a_item, a_filter);
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || a_inventory != player->GetInventoryChanges()) {
        return originalGetInventoryItemCountVR(a_inventory, a_item, a_filter);
    }
    return GetCombinedItemCount(a_item, a_filter, originalGetInventoryItemCountVR);
}

std::int16_t HookGetInventoryItemCountMemberVR(RE::InventoryChanges* a_inventory, RE::TESBoundObject* a_item) {
    std::lock_guard lock(sessionMutex);
    if (!EnsureSessionActive()) {
        return originalGetInventoryItemCountMemberVR(a_inventory, a_item);
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || a_inventory != player->GetInventoryChanges()) {
        return originalGetInventoryItemCountMemberVR(a_inventory, a_item);
    }
    return static_cast<std::int16_t>(std::min<std::int32_t>(a_item ? session.GetItemCount(a_item->GetFormID()) : 0,
                                                            std::numeric_limits<std::int16_t>::max()));
}

RE::ObjectRefHandle* HookRemoveItem(RE::TESObjectREFR* a_ref, RE::ObjectRefHandle* a_result, RE::TESBoundObject* a_item,
                                    std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason,
                                    RE::ExtraDataList* a_extraList, RE::TESObjectREFR* a_moveTo,
                                    const RE::NiPoint3* a_dropLocation, const RE::NiPoint3* a_rotation) {
    std::lock_guard lock(sessionMutex);
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* ui = RE::UI::GetSingleton();
    if (!player || a_ref != player || !session.IsActive() || !a_item || a_count <= 0 || !a_result || !ui ||
        !ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME) || a_reason != RE::ITEM_REMOVE_REASON::kRemove) {
        return originalRemoveItem(a_ref, a_result, a_item, a_count, a_reason, a_extraList, a_moveTo, a_dropLocation,
                                  a_rotation);
    }
    *a_result = {};
    std::vector<SourceInventory> inventories;
    for (const auto& source : session.GetSources()) {
        inventories.push_back(GetSnapshot(source.refHandle, a_item));
    }
    const auto plan =
        PlanItemRemoval(inventories, a_item->GetFormID(), a_count, reinterpret_cast<std::uintptr_t>(a_extraList));
    std::int32_t removed = 0;
    for (const auto& removal : plan) {
        auto ref = RE::TESObjectREFR::LookupByHandle(removal.refHandle);
        if (!ref) {
            continue;
        }
        auto* extraList = reinterpret_cast<RE::ExtraDataList*>(removal.extraList);
        if (ref->IsPlayerRef()) {
            originalRemoveItem(ref.get(), a_result, a_item, removal.count, a_reason, extraList, a_moveTo,
                               a_dropLocation, a_rotation);
        } else {
            *a_result =
                ref->RemoveItem(a_item, removal.count, a_reason, extraList, a_moveTo, a_dropLocation, a_rotation);
        }
        removed += removal.count;
    }
    session.InventoryChanged(a_item->GetFormID(), removed);
    if (removed < a_count) {
        SKSE::log::warn("RemoveItem: {} x{} could not be fulfilled from crafting sources", a_item->GetName(),
                        a_count - removed);
    }
    return a_result;
}

bool HookRecipeAvailability(const RE::BGSConstructibleObject* a_recipe) {
    std::lock_guard lock(sessionMutex);
    const bool active = EnsureSessionActive();
    if (!originalRecipeAvailability(a_recipe)) {
        return false;
    }
    if (!active) {
        return true;
    }
    bool allowed = true;
    a_recipe->requiredItems.ForEachContainerObject([&](RE::ContainerObject& a_entry) {
        if (a_entry.obj && !session.HasUnprotectedMaterials(a_entry.obj->GetFormID(), a_entry.count)) {
            allowed = false;
            return RE::BSContainer::ForEachResult::kStop;
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return allowed;
}

struct CraftingMenuEventSink final : RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (a_event && a_event->menuName == RE::CraftingMenu::MENU_NAME && !a_event->opening) {
            ResetSession();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
}

bool Install() {
    if (installed) {
        return true;
    }
    try {
        PendingHooks hooks;
        hooks.Create(REL::Relocation<std::uintptr_t>(REL::VariantID(19274, 19700, 0x29f980)).address(),
                     &HookGetContainerItemCount, originalGetContainerItemCount);
        hooks.Create(REL::Relocation<std::uintptr_t>(REL::VariantID(19273, 19699, 0x29f910)).address(),
                     &HookGetInventoryItemEntryAtIdx, originalGetInventoryItemEntryAtIdx);
        if (REL::Module::IsVR()) {
            hooks.Create(REL::Module::get().base() + 0x1f7ed0, &HookGetInventoryItemCountMemberVR,
                         originalGetInventoryItemCountMemberVR);
            hooks.Create(REL::Module::get().base() + 0x1f7f10, &HookGetInventoryItemCountVR,
                         originalGetInventoryItemCountVR);
        } else {
            hooks.Create(REL::Relocation<std::uintptr_t>(REL::VariantID(15869, 16109, 0x1f7f10)).address(),
                         &HookGetInventoryItemCount, originalGetInventoryItemCount);
        }
        hooks.Create(REL::Relocation<std::uintptr_t>(REL::VariantID(16561, 16916, 0x21c720)).address(),
                     &HookRecipeAvailability, originalRecipeAvailability);
        hooks.Enable();
        originalRemoveItem = reinterpret_cast<RemoveItem>(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_PlayerCharacter[0]).write_vfunc(0x56, &HookRemoveItem));
        hooks.Commit();
        installed = true;
        return true;
    } catch (const std::exception& error) {
        SKSE::log::error("Could not install crafting hooks: {}", error.what());
        return false;
    }
}

void OnDataLoaded() {
    if (installed && Storage::IsReady()) {
        if (auto* ui = RE::UI::GetSingleton()) {
            static CraftingMenuEventSink sink;
            ui->AddEventSink(&sink);
        }
    }
}

void ResetSession() {
    std::lock_guard lock(sessionMutex);
    session.Reset();
}
}
