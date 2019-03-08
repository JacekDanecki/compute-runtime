/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"

#include "runtime/aub/aub_center.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/source_level_debugger/source_level_debugger.h"

namespace OCLRT {
ExecutionEnvironment::ExecutionEnvironment() = default;
ExecutionEnvironment::~ExecutionEnvironment() = default;
extern CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment);

void ExecutionEnvironment::initAubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        aubCenter.reset(new AubCenter(pHwInfo, localMemoryEnabled, aubFileName, csrType));
    }
}
void ExecutionEnvironment::initGmm(const HardwareInfo *hwInfo) {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(hwInfo));
    }
}
bool ExecutionEnvironment::initializeCommandStreamReceiver(const HardwareInfo *pHwInfo, uint32_t deviceIndex, uint32_t deviceCsrIndex) {
    if (deviceIndex + 1 > commandStreamReceivers.size()) {
        commandStreamReceivers.resize(deviceIndex + 1);
    }

    if (deviceCsrIndex + 1 > commandStreamReceivers[deviceIndex].size()) {
        commandStreamReceivers[deviceIndex].resize(deviceCsrIndex + 1);
    }
    if (this->commandStreamReceivers[deviceIndex][deviceCsrIndex]) {
        return true;
    }
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver(createCommandStream(pHwInfo, *this));
    if (!commandStreamReceiver) {
        return false;
    }
    if (HwHelper::get(pHwInfo->pPlatform->eRenderCoreFamily).isPageTableManagerSupported(*pHwInfo)) {
        commandStreamReceiver->createPageTableManager();
    }
    commandStreamReceiver->setDeviceIndex(deviceIndex);
    this->commandStreamReceivers[deviceIndex][deviceCsrIndex] = std::move(commandStreamReceiver);
    return true;
}
void ExecutionEnvironment::initializeMemoryManager(bool enable64KBpages, bool enableLocalMemory) {
    if (this->memoryManager) {
        return;
    }

    int32_t setCommandStreamReceiverType = CommandStreamReceiverType::CSR_HW;
    if (DebugManager.flags.SetCommandStreamReceiver.get() >= 0) {
        setCommandStreamReceiverType = DebugManager.flags.SetCommandStreamReceiver.get();
    }

    switch (setCommandStreamReceiverType) {
    case CommandStreamReceiverType::CSR_TBX:
    case CommandStreamReceiverType::CSR_TBX_WITH_AUB:
        memoryManager = std::make_unique<TbxMemoryManager>(enable64KBpages, enableLocalMemory, *this);
        break;
    case CommandStreamReceiverType::CSR_AUB:
        memoryManager = std::make_unique<OsAgnosticMemoryManager>(enable64KBpages, enableLocalMemory, *this);
        break;
    case CommandStreamReceiverType::CSR_HW:
    case CommandStreamReceiverType::CSR_HW_WITH_AUB:
    default:
        memoryManager = MemoryManager::createMemoryManager(enable64KBpages, enableLocalMemory, *this);
        break;
    }
    DEBUG_BREAK_IF(!this->memoryManager);
}
void ExecutionEnvironment::initSourceLevelDebugger(const HardwareInfo &hwInfo) {
    if (hwInfo.capabilityTable.sourceLevelDebuggerSupported) {
        sourceLevelDebugger.reset(SourceLevelDebugger::create());
    }
    if (sourceLevelDebugger) {
        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == SipKernel::getSipKernelType(hwInfo.pPlatform->eRenderCoreFamily, true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);
    }
}
GmmHelper *ExecutionEnvironment::getGmmHelper() const {
    return gmmHelper.get();
}
CompilerInterface *ExecutionEnvironment::getCompilerInterface() {
    if (this->compilerInterface.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->compilerInterface.get() == nullptr) {
            this->compilerInterface.reset(CompilerInterface::createInstance());
        }
    }
    return this->compilerInterface.get();
}
BuiltIns *ExecutionEnvironment::getBuiltIns() {
    if (this->builtins.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->builtins.get() == nullptr) {
            this->builtins = std::make_unique<BuiltIns>();
        }
    }
    return this->builtins.get();
}

EngineControl *ExecutionEnvironment::getEngineControlForSpecialCsr() {
    EngineControl *engine = nullptr;
    if (specialCommandStreamReceiver.get()) {
        engine = memoryManager->getRegisteredEngineForCsr(specialCommandStreamReceiver.get());
    }
    return engine;
}

} // namespace OCLRT
