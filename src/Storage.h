// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#pragma once

#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

namespace HCSE::Storage {
struct ContainerCandidate {
    std::uint32_t formID;
    std::uint32_t refHandle;
    bool isContainerReference;
};

inline std::vector<std::uint32_t> SelectContainers(std::span<const ContainerCandidate> a_candidates) {
    std::vector<std::uint32_t> result;
    std::unordered_set<std::uint32_t> seen;
    for (const auto& candidate : a_candidates) {
        if (candidate.isContainerReference && candidate.refHandle && seen.insert(candidate.formID).second) {
            result.push_back(candidate.refHandle);
        }
    }
    return result;
}

bool Initialize();
bool IsReady();
std::uint32_t GetSharedContainerHandle();
std::vector<std::uint32_t> GetCraftingContainerHandles();
}
