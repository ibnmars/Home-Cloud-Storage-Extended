// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "CraftingHooks.h"
#include "HorseIntegration.h"
#include "Storage.h"
#include "Version.h"

#include <Windows.h>
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>

#include "SKSE/SKSE.h"

namespace HCSE {
namespace {
void InitializeLogging() {
    const auto directory = SKSE::log::log_directory();
    if (!directory) {
        SKSE::stl::report_and_fail("Could not determine the SKSE log directory");
    }
    std::filesystem::create_directories(*directory);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        (*directory / fmt::format("{}.log", Version::kID)).string(), true);
    auto log = std::make_shared<spdlog::logger>("HCSE", std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
    SKSE::log::info("{} {} on Skyrim {}", Version::kName, Version::kVersion, REL::Module::get().version().string());
}

void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message) {
    switch (a_message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        Storage::Initialize();
        CraftingHooks::OnDataLoaded();
        HorseIntegration::Initialize();
        break;
    case SKSE::MessagingInterface::kPreLoadGame:
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        CraftingHooks::ResetSession();
        HorseIntegration::ResetInput();
        break;
    default:
        break;
    }
}
}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    HCSE::InitializeLogging();
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(HCSE::OnSKSEMessage)) {
        SKSE::log::critical("Could not register the SKSE message listener");
        return false;
    }
    if (!HCSE::CraftingHooks::Install()) {
        SKSE::log::critical("Cloud crafting disabled; see the preceding hook error");
        MessageBoxA(nullptr,
                    "Home Cloud Storage - Extended could not install its crafting hooks.\n"
                    "Check SKSE, Address Library and HomeCloudStorageExtended.log.",
                    "Home Cloud Storage - Extended", MB_OK | MB_ICONWARNING);
    }
    return true;
}
