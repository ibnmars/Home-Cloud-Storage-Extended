// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace HCSE::HorseIntegration {
inline std::int32_t GetHotkeyValue(float a_value, std::int32_t a_limit, std::int32_t a_fallback) {
    return a_value >= 0.0F && a_value < static_cast<float>(a_limit) ? static_cast<std::int32_t>(a_value) : a_fallback;
}

inline bool IsHorseInventoryEvent(std::string_view a_script, std::string_view a_event) {
    return a_script == "SimplestHorses_Quest" && a_event == "OnKeyUp";
}

class InventoryRequest {
  public:
    void Capture(bool a_modifier) { pending_.store(a_modifier); }
    bool Consume(bool a_matchingCall) { return a_matchingCall && pending_.exchange(false); }
    void Reset() { pending_.store(false); }

  private:
    std::atomic_bool pending_ = false;
};

void Initialize();
void ResetInput();
}
