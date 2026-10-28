// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "CraftingSession.h"
#include "Storage.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(a_condition)                                                                                             \
    do {                                                                                                               \
        if (!(a_condition)) {                                                                                          \
            throw std::runtime_error(#a_condition);                                                                    \
        }                                                                                                              \
    } while (false)
using namespace HCSE;

void CheckMaterials() {
    std::array sources{SourceInventory{1, 1, {{100, 2, false, {}}}}, SourceInventory{2, 1, {{100, 4, false, {}}}}};
    CraftingSession session;
    session.Begin(10, StationType::kCrafting, sources);
    CHECK(session.IsActive() && session.IsCurrentFurniture(10));
    CHECK(!session.IsCurrentFurniture(11));
    CHECK(session.GetEntryCount() == 2 && session.GetItemCount(100) == 6);
    CHECK(session.GetItemCount(200) == 0);
    CHECK(session.GetSourceForIndex(0)->refHandle == 1);
    CHECK(session.GetSourceForIndex(1)->refHandle == 2);
    CHECK(!session.GetSourceForIndex(-1) && !session.GetSourceForIndex(2));
    auto plan = PlanItemRemoval(sources, 100, 5, 0);
    CHECK(plan.size() == 2 && plan[0].refHandle == 1 && plan[0].count == 2);
    CHECK(plan[1].refHandle == 2 && plan[1].count == 3);
    CHECK(PlanItemRemoval(sources, 100, 0, 0).empty());
    CHECK(PlanItemRemoval(sources, 200, 1, 0).empty());
    session.InventoryChanged(100, 5);
    CHECK(session.NeedsRefresh() && session.GetItemCount(100) == 1);
    sources[0].items[0].count = 0;
    sources[0].entryCount = 0;
    sources[1].items[0].count = 1;
    session.Refresh(sources);
    CHECK(!session.NeedsRefresh() && session.GetEntryCount() == 1);
    CHECK(session.GetSourceForIndex(0)->refHandle == 2);
    CHECK(session.GetItemCount(100) == 1);
    plan = PlanItemRemoval(sources, 100, 2, 0);
    CHECK(plan.size() == 1 && plan[0].count == 1 && plan[0].refHandle == 2);
    sources[1].items[0].count = 0;
    sources[1].entryCount = 0;
    session.Refresh(sources);
    CHECK(session.GetItemCount(100) == 0 && session.GetEntryCount() == 0);
    CHECK(PlanItemRemoval(sources, 100, 1, 0).empty());
    session.Reset();
    CHECK(!session.IsActive() && session.GetSources().empty());
}

void CheckQuestItems() {
    std::array sources{SourceInventory{1, 1, {{100, 2, true, {}}}}, SourceInventory{2, 1, {{100, 1, false, {}}}}};
    CraftingSession session;
    session.Begin(10, StationType::kCrafting, sources);
    CHECK(session.GetItemCount(100) == 1);
    CHECK(!session.HasUnprotectedMaterials(100, 2));
    CHECK(!session.HasUnprotectedMaterials(100, -2));
    CHECK(session.HasUnprotectedMaterials(100, 1));
    auto plan = PlanItemRemoval(sources, 100, 3, 0);
    CHECK(plan.size() == 1 && plan[0].refHandle == 2 && plan[0].count == 1);
    sources[1].items[0].count = 2;
    session.Refresh(sources);
    CHECK(session.HasUnprotectedMaterials(100, 2));
    CHECK(!session.HasUnprotectedMaterials(100, std::numeric_limits<std::int32_t>::min()));
    sources[1].items[0].isQuestObject = true;
    session.Refresh(sources);
    CHECK(session.GetItemCount(100) == 0);
    CHECK(PlanItemRemoval(sources, 100, 1, 0).empty());
}

void CheckInstances() {
    std::array sources{SourceInventory{1, 1, {{100, 1, false, {}}}},
                       SourceInventory{2, 1, {{100, 2, false, {{0xAB, 1}, {0xCD, 1}}}}}};
    auto plan = PlanItemRemoval(sources, 100, 1, 0xAB);
    CHECK(plan.size() == 1 && plan[0].refHandle == 2 && plan[0].extraList == 0xAB);
    CHECK(PlanItemRemoval(sources, 100, 1, 0xFF).empty());
    plan = PlanItemRemoval(sources, 100, 2, 0xAB);
    CHECK(plan.size() == 1 && plan[0].count == 1);
    sources[1].items[0].instances.erase(sources[1].items[0].instances.begin());
    CHECK(PlanItemRemoval(sources, 100, 1, 0xAB).empty());
    CHECK(!PlanItemRemoval(sources, 100, 1, 0xCD).empty());
    sources[1].items[0].isQuestObject = true;
    CHECK(PlanItemRemoval(sources, 100, 1, 0xCD).empty());
    CraftingSession session;
    session.Begin(10, StationType::kEnchanting, sources);
    CHECK(session.IncludeEntry(100, true));
    CHECK(session.IncludeEntry(100, true));
    session.BeginEnumeration();
    CHECK(session.IncludeEntry(100, false));
    CHECK(!session.IncludeEntry(100, false));
    session.BeginEnumeration();
    CHECK(session.IncludeEntry(100, false));
}

void CheckSourcesAndBounds() {
    using Storage::ContainerCandidate;
    std::array candidates{ContainerCandidate{1, 11, true}, ContainerCandidate{2, 22, false},
                          ContainerCandidate{3, 0, true}, ContainerCandidate{1, 11, true},
                          ContainerCandidate{4, 44, true}};
    const auto selected = Storage::SelectContainers(candidates);
    CHECK(selected.size() == 2 && selected[0] == 11 && selected[1] == 44);
    CHECK(Storage::SelectContainers({}).empty());
    const auto maximum = std::numeric_limits<std::int32_t>::max();
    std::array counts{maximum, 5, -7};
    CHECK(SumItemCounts(counts) == maximum);
    std::array sources{SourceInventory{1, maximum, {{100, maximum, false, {}}}},
                       SourceInventory{2, 10, {{100, 10, false, {}}}}, SourceInventory{1, 1, {{100, 10, false, {}}}},
                       SourceInventory{0, 1, {}}};
    CraftingSession session;
    session.Begin(10, StationType::kAlchemy, sources);
    CHECK(session.GetSources().size() == 2);
    CHECK(session.GetEntryCount() == maximum && session.GetItemCount(100) == maximum);
    CHECK(session.GetSourceForIndex(maximum - 1)->refHandle == 1);
    CHECK(!session.GetSourceForIndex(maximum));
}

int main() {
    try {
        CheckMaterials();
        CheckQuestItems();
        CheckInstances();
        CheckSourcesAndBounds();
        std::cout << "Crafting rules: counts, priorities, quest guards, instances, refresh and bounds passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
