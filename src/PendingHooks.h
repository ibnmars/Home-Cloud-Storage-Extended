// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#pragma once

#include <MinHook.h>
#undef GetObject

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace HCSE {
class PendingHooks {
  public:
    PendingHooks() {
        addresses_.reserve(8);
        const auto status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            throw std::runtime_error(MH_StatusToString(status));
        }
    }
    PendingHooks(const PendingHooks&) = delete;
    PendingHooks& operator=(const PendingHooks&) = delete;
    ~PendingHooks() {
        if (!committed_) {
            for (auto* address : addresses_) {
                MH_DisableHook(address);
                MH_RemoveHook(address);
            }
        }
    }

    template <class Function> void Create(std::uintptr_t a_address, Function a_hook, Function& a_original) {
        const auto status = MH_CreateHook(reinterpret_cast<void*>(a_address), reinterpret_cast<void*>(a_hook),
                                          reinterpret_cast<void**>(&a_original));
        if (status != MH_OK) {
            throw std::runtime_error(MH_StatusToString(status));
        }
        addresses_.push_back(reinterpret_cast<void*>(a_address));
    }
    void Enable() {
        for (auto* address : addresses_) {
            const auto status = MH_EnableHook(address);
            if (status != MH_OK) {
                throw std::runtime_error(MH_StatusToString(status));
            }
        }
    }
    void Commit() { committed_ = true; }

  private:
    std::vector<void*> addresses_;
    bool committed_ = false;
};
}
