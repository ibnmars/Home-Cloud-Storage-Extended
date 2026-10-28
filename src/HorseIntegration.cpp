// Copyright (C) 2026 IbnMars
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional permissions: see EXCEPTIONS.md.
#include "HorseIntegration.h"
#include "Forms.h"
#include "PendingHooks.h"
#include "Storage.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <array>

namespace HCSE::HorseIntegration {
namespace {
using NativeDispatch = bool (*)(const RE::BSScript::NF_util::NativeFunctionBase*, RE::BSScript::Variable&,
                                RE::BSScript::Internal::VirtualMachine&, RE::VMStackID, RE::BSScript::Variable&,
                                const RE::BSScript::StackFrame&);
RE::TESGlobal* horseHotkey = nullptr;
RE::TESGlobal* dakHotkey = nullptr;
RE::BSTSmartPointer<RE::BSScript::IFunction> openInventoryFunction;
NativeDispatch originalNativeDispatch = nullptr;
std::array<bool, SKSE::InputMap::kMaxMacros> heldKeys{};
InventoryRequest inventoryRequest;
constexpr std::size_t kMarshalAndDispatchIndex = 0x16;
struct HorseInputEventSink final : RE::BSTEventSink<RE::InputEvent*> {
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events,
                                          RE::BSTEventSource<RE::InputEvent*>*) override;
};
std::int32_t GetHotkey(const RE::TESGlobal* a_global, std::int32_t a_fallback) {
    return a_global ? GetHotkeyValue(a_global->value, SKSE::InputMap::kMaxMacros, a_fallback) : a_fallback;
}
std::uint32_t GetButtonKeycode(const RE::ButtonEvent& a_button) {
    switch (a_button.device.get()) {
    case RE::INPUT_DEVICE::kKeyboard:
        return a_button.GetIDCode();
    case RE::INPUT_DEVICE::kMouse:
        return a_button.GetIDCode() + SKSE::InputMap::kMacro_MouseButtonOffset;
    case RE::INPUT_DEVICE::kGamepad:
        return SKSE::InputMap::GamepadMaskToKeycode(a_button.GetIDCode());
    default:
        return SKSE::InputMap::kMaxMacros;
    }
}

bool IsHorseModifierPressed() {
    auto* ui = RE::UI::GetSingleton();
    auto* controls = RE::ControlMap::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!ui || ui->GameIsPaused() || !controls || controls->GetRuntimeData().textEntryCount > 0 || !player ||
        player->IsSprinting()) {
        return false;
    }

    const auto key = GetHotkey(dakHotkey, RE::BSKeyboardDevice::Keys::kLeftShift);
    if (key < SKSE::InputMap::kMacro_NumKeyboardKeys) {
        auto* devices = RE::BSInputDeviceManager::GetSingleton();
        auto* keyboard = devices ? devices->GetKeyboard() : nullptr;
        return keyboard && (keyboard->GetRuntimeData().curState[key] & 0x80) != 0;
    }
    return heldKeys[key];
}

bool IsHorseInventoryCall(const RE::BSScript::StackFrame& a_frame) {
    for (auto* caller = &a_frame; caller; caller = caller->previousFrame) {
        if (caller->owningFunction && IsHorseInventoryEvent(caller->owningFunction->GetObjectTypeName().c_str(),
                                                            caller->owningFunction->GetName().c_str())) {
            return true;
        }
    }
    return false;
}

bool OpenCloudStorage(RE::BSScript::Internal::VirtualMachine& a_vm) {
    auto storage = RE::TESObjectREFR::LookupByHandle(Storage::GetSharedContainerHandle());
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!storage || !player) {
        return false;
    }
    RE::BSScript::Variable storageValue;
    storageValue.Pack<RE::TESObjectREFR*>(storage.get());
    if (!storageValue.IsObject()) {
        return false;
    }
    auto object = storageValue.GetObject();
    if (!object) {
        return false;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    return a_vm.DispatchMethodCall(object, "Activate",
                                   RE::MakeFunctionArguments(static_cast<RE::TESObjectREFR*>(player), false), callback);
}

bool HookNativeDispatch(const RE::BSScript::NF_util::NativeFunctionBase* a_function,
                        RE::BSScript::Variable& a_baseValue, RE::BSScript::Internal::VirtualMachine& a_vm,
                        RE::VMStackID a_stackID, RE::BSScript::Variable& a_result,
                        const RE::BSScript::StackFrame& a_frame) {
    if (a_function == openInventoryFunction.get() && IsHorseInventoryCall(a_frame) && a_baseValue.IsObject() &&
        a_baseValue.Unpack<RE::Actor*>() && inventoryRequest.Consume(true) && OpenCloudStorage(a_vm)) {
        a_result.SetNone();
        return true;
    }
    return originalNativeDispatch(a_function, a_baseValue, a_vm, a_stackID, a_result, a_frame);
}

RE::BSEventNotifyControl HorseInputEventSink::ProcessEvent(RE::InputEvent* const* a_events,
                                                           RE::BSTEventSource<RE::InputEvent*>*) {
    if (!a_events || !horseHotkey) {
        return RE::BSEventNotifyControl::kContinue;
    }
    for (auto* event = *a_events; event; event = event->next) {
        auto* button = event->AsButtonEvent();
        if (!button) {
            continue;
        }
        const auto key = GetButtonKeycode(*button);
        if (key >= heldKeys.size()) {
            continue;
        }
        heldKeys[key] = button->IsPressed();
        if (button->IsDown() && static_cast<std::int32_t>(key) == GetHotkey(horseHotkey, -1)) {
            inventoryRequest.Capture(IsHorseModifierPressed());
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}
}
void Initialize() {
    if (originalNativeDispatch) {
        return;
    }
    horseHotkey = Forms::LookupForm<RE::TESGlobal>(0xDD8, "SimplestHorses.esp");
    auto storage = RE::TESObjectREFR::LookupByHandle(Storage::GetSharedContainerHandle());
    if (!horseHotkey || !storage) {
        return;
    }

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto* input = RE::BSInputDeviceManager::GetSingleton();
    RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> actorType;
    if (!vm || !input || !vm->GetScriptObjectType("Actor", actorType) || !actorType || !actorType->IsLinkedValid()) {
        SKSE::log::error("Could not initialize the Simplest Horses integration");
        return;
    }

    auto* functions = actorType->GetMemberFuncIter();
    for (std::uint32_t i = 0; i < actorType->GetNumMemberFuncs(); ++i) {
        const auto& function = functions[i].func;
        if (function && function->GetName() == "OpenInventory" && function->GetIsNative() && !function->GetIsStatic() &&
            function->GetParamCount() == 1 &&
            function->GetReturnType().GetRawType() == RE::BSScript::TypeInfo::RawType::kNone) {
            RE::BSFixedString parameterName;
            RE::BSScript::TypeInfo parameterType;
            function->GetParam(0, parameterName, parameterType);
            if (!parameterType.IsBool()) {
                continue;
            }
            openInventoryFunction = function;
            break;
        }
    }
    if (!openInventoryFunction) {
        SKSE::log::error("Could not resolve Actor.OpenInventory for Simplest Horses");
        return;
    }

    auto* native = static_cast<RE::BSScript::NF_util::NativeFunctionBase*>(openInventoryFunction.get());
    if (!native->HasStub() || native->GetIsLatent()) {
        SKSE::log::error("Actor.OpenInventory has an unsupported native binding");
        return;
    }
    const auto* vtable = *reinterpret_cast<std::uintptr_t**>(native);
    // CommonLib's NativeFunctionBase declares MarshalAndDispatch at vtable slot 0x16.
    auto* target = reinterpret_cast<void*>(vtable[kMarshalAndDispatchIndex]);
    try {
        PendingHooks hooks;
        hooks.Create(reinterpret_cast<std::uintptr_t>(target), &HookNativeDispatch, originalNativeDispatch);
        hooks.Enable();
        hooks.Commit();
    } catch (const std::exception& error) {
        originalNativeDispatch = nullptr;
        SKSE::log::error("Could not install Simplest Horses inventory hook: {}", error.what());
        return;
    }

    dakHotkey = Forms::LookupForm<RE::TESGlobal>(0x804, "Dynamic Activation Key.esp");
    static HorseInputEventSink horseInputEventSink;
    input->AddEventSink(&horseInputEventSink);
    SKSE::log::info("Simplest Horses integration enabled; modifier: {}",
                    dakHotkey ? "Dynamic Activation Key" : "Left Shift");
}
void ResetInput() {
    heldKeys.fill(false);
    inventoryRequest.Reset();
}
}
