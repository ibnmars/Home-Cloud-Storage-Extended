// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "HorseIntegration.h"
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(a_condition)                                                                                             \
    do {                                                                                                               \
        if (!(a_condition)) {                                                                                          \
            throw std::runtime_error(#a_condition);                                                                    \
        }                                                                                                              \
    } while (false)
int main() {
    using namespace HCSE::HorseIntegration;
    try {
        CHECK(GetHotkeyValue(42, 282, -1) == 42);
        CHECK(GetHotkeyValue(276, 282, -1) == 276);
        CHECK(GetHotkeyValue(-1, 282, 42) == 42);
        CHECK(GetHotkeyValue(282, 282, 42) == 42);
        CHECK(GetHotkeyValue(std::numeric_limits<float>::quiet_NaN(), 282, 42) == 42);
        CHECK(GetHotkeyValue(std::numeric_limits<float>::infinity(), 282, 42) == 42);
        CHECK(IsHorseInventoryEvent("SimplestHorses_Quest", "OnKeyUp"));
        CHECK(!IsHorseInventoryEvent("OtherQuest", "OnKeyUp"));
        CHECK(!IsHorseInventoryEvent("SimplestHorses_Quest", "OnUpdate"));
        InventoryRequest request;
        CHECK(!request.Consume(true));
        request.Capture(true);
        CHECK(!request.Consume(false));
        CHECK(request.Consume(true));
        CHECK(!request.Consume(true));
        request.Capture(true);
        request.Capture(false);
        CHECK(!request.Consume(true));
        request.Capture(true);
        request.Reset();
        CHECK(!request.Consume(true));
        std::cout << "Horse integration rules: keys, caller scoping and modifier capture passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
