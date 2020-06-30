/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/mocks/mock_command_stream_receiver.h"

#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using CommandQueueCreate = Test<DeviceFixture>;

TEST_F(CommandQueueCreate, whenCreatingCommandQueueThenItIsInitialized) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false);
    ASSERT_NE(nullptr, commandQueue);

    L0::CommandQueueImp *commandQueueImp = reinterpret_cast<L0::CommandQueueImp *>(commandQueue);

    EXPECT_EQ(csr.get(), commandQueueImp->getCsr());
    EXPECT_EQ(device, commandQueueImp->getDevice());
    EXPECT_EQ(0u, commandQueueImp->getTaskCount());

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOrdinalThenQueueIsCreatedOnlyIfOrdinalIsLessThanNumOfAsyncComputeEngines) {
    ze_device_properties_t deviceProperties;
    ze_result_t res = device->getProperties(&deviceProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto expectedNumOfComputeEngines = NEO::HwHelper::getEnginesCount(device->getNEODevice()->getHardwareInfo());
    EXPECT_EQ(expectedNumOfComputeEngines, deviceProperties.numAsyncComputeEngines);

    ze_command_queue_desc_t desc = {};
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
    desc.ordinal = deviceProperties.numAsyncComputeEngines;

    ze_command_queue_handle_t commandQueue = {};
    res = device->createCommandQueue(&desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);

    std::vector<ze_command_queue_handle_t> commandQueueVector;
    for (uint32_t i = 0; i < expectedNumOfComputeEngines; i++) {
        desc.ordinal = i;
        res = device->createCommandQueue(&desc, &commandQueue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        EXPECT_NE(nullptr, commandQueue);

        commandQueueVector.push_back(commandQueue);
    }

    for (uint32_t i = 0; i < expectedNumOfComputeEngines; i++) {
        for (uint32_t j = 0; j < expectedNumOfComputeEngines; j++) {
            if (i == j) {
                continue;
            }
            EXPECT_NE(static_cast<CommandQueue *>(commandQueueVector[i])->getCsr(),
                      static_cast<CommandQueue *>(commandQueueVector[j])->getCsr());
        }
    }

    for (auto commandQueue : commandQueueVector) {
        L0::CommandQueue::fromHandle(commandQueue)->destroy();
    }
}

TEST_F(CommandQueueCreate, givenOrdinalWhenQueueIsCreatedThenCorrectEngineIsSelected) {
    ze_device_properties_t deviceProperties;
    ze_result_t res = device->getProperties(&deviceProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto numOfComputeEngines = NEO::HwHelper::getEnginesCount(device->getNEODevice()->getHardwareInfo());

    ze_command_queue_desc_t desc = {};
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;

    ze_command_queue_handle_t commandQueue = {};

    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);

    for (uint32_t i = 0; i < numOfComputeEngines; i++) {
        desc.ordinal = i;
        res = device->createCommandQueue(&desc, &commandQueue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
        ASSERT_NE(nullptr, commandQueue);

        EXPECT_EQ(device->getNEODevice()->getEngine(hwHelper.getComputeEngineIndexByOrdinal(*defaultHwInfo, i)).commandStreamReceiver,
                  static_cast<CommandQueue *>(commandQueue)->getCsr());

        L0::CommandQueue::fromHandle(commandQueue)->destroy();
    }
}

using CommandQueueSBASupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

struct MockMemoryManagerCommandQueueSBA : public MemoryManagerMock {
    MockMemoryManagerCommandQueueSBA(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    MOCK_METHOD1(getInternalHeapBaseAddress, uint64_t(uint32_t rootDeviceIndex));
};

struct CommandQueueProgramSBATest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
        }

        memoryManager = new ::testing::NiceMock<MockMemoryManagerCommandQueueSBA>(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, rootDeviceIndex);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    MockMemoryManagerCommandQueueSBA *memoryManager = nullptr;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

HWTEST2_F(CommandQueueProgramSBATest, whenCreatingCommandQueueThenItIsInitialized, CommandQueueSBASupport) {
    ze_command_queue_desc_t desc = {};
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false);

    uint32_t alignedSize = 4096u;
    NEO::LinearStream child(commandQueue->commandStream->getSpace(alignedSize), alignedSize);

    EXPECT_CALL(*memoryManager, getInternalHeapBaseAddress(rootDeviceIndex))
        .Times(1);

    commandQueue->programGeneralStateBaseAddress(0u, child);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingNonCopyBlitCommandListThenWrongCommandListStatusReturned) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_COPY_ONLY,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, false));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingCopyBlitCommandListThenSuccessReturned) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_COPY_ONLY,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    auto defaultCsr = neoDevice->getDefaultEngine().commandStreamReceiver;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          defaultCsr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    commandQueue->destroy();
}

using CommandQueueDestroySupport = IsAtLeastProduct<IGFX_SKYLAKE>;
using CommandQueueDestroy = Test<DeviceFixture>;

HWTEST2_F(CommandQueueDestroy, whenCommandQueueDestroyIsCalledPrintPrintfOutputIsCalled, CommandQueueDestroySupport) {
    ze_command_queue_desc_t desc = {};
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = new MockCommandQueueHw<gfxCoreFamily>(device, csr.get(), &desc);
    commandQueue->initialize(false);

    Mock<Kernel> kernel;
    commandQueue->printfFunctionContainer.push_back(&kernel);

    EXPECT_EQ(0u, kernel.printPrintfOutputCalledTimes);
    commandQueue->destroy();
    EXPECT_EQ(1u, kernel.printPrintfOutputCalledTimes);
}

using CommandQueueCommands = Test<DeviceFixture>;
HWTEST_F(CommandQueueCommands, givenCommandQueueWhenExecutingCommandListsThenHardwareContextIsProgrammedAndGlobalAllocationResident) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0);
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));
    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto globalFence = csr.getGlobalFenceAllocation();
    if (globalFence) {
        bool found = false;
        for (auto alloc : csr.copyOfAllocations) {
            if (alloc == globalFence) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(csr.programHardwareContextCalled);
    commandQueue->destroy();
}

using CommandQueueIndirectAllocations = Test<ModuleFixture>;
HWTEST_F(CommandQueueIndirectAllocations, givenCommandQueueWhenExecutingCommandListsThenExpectedIndirectAllocationsAddedToResidencyContainer) {
    const ze_command_queue_desc_t desc = {
        ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT,
        ZE_COMMAND_QUEUE_FLAG_NONE,
        ZE_COMMAND_QUEUE_MODE_DEFAULT,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
        0};

    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0);
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          &csr,
                                                          &desc,
                                                          true);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, true));

    void *deviceAlloc = nullptr;
    auto result = device->getDriverHandle()->allocDeviceMem(device->toHandle(), ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT, 16384u, 4096u, &deviceAlloc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(deviceAlloc)->gpuAllocation;
    ASSERT_NE(nullptr, gpuAlloc);

    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
    EXPECT_TRUE(kernel->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed);

    ze_group_count_t groupCount{1, 1, 1};
    result = commandList->appendLaunchKernel(kernel->toHandle(),
                                             &groupCount,
                                             nullptr,
                                             0,
                                             nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                               std::end(commandList->commandContainer.getResidencyContainer()),
                               gpuAlloc);
    EXPECT_EQ(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    itorEvent = std::find(std::begin(commandList->commandContainer.getResidencyContainer()),
                          std::end(commandList->commandContainer.getResidencyContainer()),
                          gpuAlloc);
    EXPECT_NE(itorEvent, std::end(commandList->commandContainer.getResidencyContainer()));

    device->getDriverHandle()->getSvmAllocsManager()->freeSVMAlloc(deviceAlloc);
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0