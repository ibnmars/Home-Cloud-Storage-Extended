// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "CraftingSession.h"

#include <algorithm>
#include <limits>

namespace HCSE {
namespace {
std::int32_t AddCounts(std::int32_t a_left, std::int32_t a_right) {
    return static_cast<std::int32_t>(std::min<std::int64_t>(static_cast<std::int64_t>(a_left) + std::max(0, a_right),
                                                            std::numeric_limits<std::int32_t>::max()));
}
}

std::int32_t SumItemCounts(std::span<const std::int32_t> a_counts) {
    std::int32_t total = 0;
    for (auto count : a_counts) {
        total = AddCounts(total, count);
    }
    return total;
}

std::vector<ItemRemoval> PlanItemRemoval(std::span<const SourceInventory> a_sources, std::uint32_t a_formID,
                                         std::int32_t a_count, std::uintptr_t a_extraList) {
    std::vector<ItemRemoval> result;
    std::unordered_set<std::uint32_t> seenSources;
    for (const auto& source : a_sources) {
        if (a_count <= 0) {
            break;
        }
        if (!source.refHandle || !seenSources.insert(source.refHandle).second) {
            continue;
        }
        const auto item = std::ranges::find(source.items, a_formID, &InventoryItem::formID);
        if (item == source.items.end() || item->isQuestObject || item->count <= 0) {
            continue;
        }
        auto available = item->count;
        if (a_extraList) {
            const auto instance = std::ranges::find(item->instances, a_extraList, &InventoryInstance::extraList);
            if (instance == item->instances.end()) {
                continue;
            }
            available = std::min(available, instance->count);
        }
        const auto count = std::min(a_count, available);
        if (count > 0) {
            result.push_back({source.refHandle, count, a_extraList});
            a_count -= count;
        }
        if (a_extraList) {
            break;
        }
    }
    return result;
}

void CraftingSession::Begin(std::uint32_t a_furniture, StationType a_station,
                            std::span<const SourceInventory> a_sources) {
    Reset();
    furniture_ = a_furniture;
    station_ = a_station;
    Refresh(a_sources);
}

void CraftingSession::Refresh(std::span<const SourceInventory> a_sources) {
    sources_.clear();
    itemCounts_.clear();
    questItems_.clear();
    seenItems_.clear();
    std::unordered_set<std::uint32_t> seenSources;
    std::int32_t index = 0;
    for (const auto& source : a_sources) {
        if (!source.refHandle || !seenSources.insert(source.refHandle).second) {
            continue;
        }
        const auto count = std::min(std::max(0, source.entryCount), std::numeric_limits<std::int32_t>::max() - index);
        sources_.push_back({source.refHandle, count, index});
        index += count;
        for (const auto& item : source.items) {
            if (item.isQuestObject) {
                questItems_.insert(item.formID);
            } else if (item.count > 0) {
                itemCounts_[item.formID] = AddCounts(itemCounts_[item.formID], item.count);
            }
        }
    }
    needsRefresh_ = false;
}

void CraftingSession::Reset() {
    furniture_ = 0;
    station_ = StationType::kUnknown;
    needsRefresh_ = false;
    sources_.clear();
    itemCounts_.clear();
    questItems_.clear();
    seenItems_.clear();
}

void CraftingSession::InventoryChanged(std::uint32_t a_formID, std::int32_t a_removed) {
    if (a_removed <= 0) {
        return;
    }
    if (auto item = itemCounts_.find(a_formID); item != itemCounts_.end()) {
        item->second = std::max(0, item->second - a_removed);
    }
    needsRefresh_ = true;
}

bool CraftingSession::IsActive() const {
    return furniture_ != 0;
}
bool CraftingSession::IsCurrentFurniture(std::uint32_t a_furniture) const {
    return IsActive() && furniture_ == a_furniture;
}
bool CraftingSession::NeedsRefresh() const {
    return needsRefresh_;
}
StationType CraftingSession::GetStationType() const {
    return station_;
}
std::span<const ContainerSource> CraftingSession::GetSources() const {
    return sources_;
}

const ContainerSource* CraftingSession::GetSourceForIndex(std::int32_t a_index) const {
    for (const auto& source : sources_) {
        if (a_index >= source.startIndex && a_index - source.startIndex < source.entryCount) {
            return &source;
        }
    }
    return nullptr;
}

std::int32_t CraftingSession::GetEntryCount() const {
    return sources_.empty() ? 0 : sources_.back().startIndex + sources_.back().entryCount;
}

std::int32_t CraftingSession::GetItemCount(std::uint32_t a_formID) const {
    const auto item = itemCounts_.find(a_formID);
    return item == itemCounts_.end() ? 0 : item->second;
}

bool CraftingSession::HasUnprotectedMaterials(std::uint32_t a_formID, std::int32_t a_required) const {
    const auto count = a_required < 0 ? -static_cast<std::int64_t>(a_required) : a_required;
    return !questItems_.contains(a_formID) || GetItemCount(a_formID) >= count;
}

void CraftingSession::BeginEnumeration() {
    seenItems_.clear();
}

bool CraftingSession::IncludeEntry(std::uint32_t a_formID, bool a_keepInstances) {
    return a_keepInstances || seenItems_.insert(a_formID).second;
}
}
