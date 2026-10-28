// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HCSE {
enum class StationType { kCrafting, kTempering, kEnchanting, kAlchemy, kUnknown };

struct InventoryInstance {
    std::uintptr_t extraList;
    std::int32_t count;
};

struct InventoryItem {
    std::uint32_t formID;
    std::int32_t count;
    bool isQuestObject;
    std::vector<InventoryInstance> instances;
};

struct SourceInventory {
    std::uint32_t refHandle;
    std::int32_t entryCount;
    std::vector<InventoryItem> items;
};

struct ContainerSource {
    std::uint32_t refHandle;
    std::int32_t entryCount;
    std::int32_t startIndex;
};

struct ItemRemoval {
    std::uint32_t refHandle;
    std::int32_t count;
    std::uintptr_t extraList;
};

std::vector<ItemRemoval> PlanItemRemoval(std::span<const SourceInventory> a_sources, std::uint32_t a_formID,
                                         std::int32_t a_count, std::uintptr_t a_extraList);
std::int32_t SumItemCounts(std::span<const std::int32_t> a_counts);

class CraftingSession {
  public:
    void Begin(std::uint32_t a_furniture, StationType a_station, std::span<const SourceInventory> a_sources);
    void Refresh(std::span<const SourceInventory> a_sources);
    void Reset();
    void InventoryChanged(std::uint32_t a_formID, std::int32_t a_removed);

    bool IsActive() const;
    bool IsCurrentFurniture(std::uint32_t a_furniture) const;
    bool NeedsRefresh() const;
    StationType GetStationType() const;
    std::span<const ContainerSource> GetSources() const;
    const ContainerSource* GetSourceForIndex(std::int32_t a_index) const;
    std::int32_t GetEntryCount() const;
    std::int32_t GetItemCount(std::uint32_t a_formID) const;
    bool HasUnprotectedMaterials(std::uint32_t a_formID, std::int32_t a_required) const;
    void BeginEnumeration();
    bool IncludeEntry(std::uint32_t a_formID, bool a_keepInstances);

  private:
    std::uint32_t furniture_ = 0;
    StationType station_ = StationType::kUnknown;
    bool needsRefresh_ = false;
    std::vector<ContainerSource> sources_;
    std::unordered_map<std::uint32_t, std::int32_t> itemCounts_;
    std::unordered_set<std::uint32_t> questItems_;
    std::unordered_set<std::uint32_t> seenItems_;
};
}
