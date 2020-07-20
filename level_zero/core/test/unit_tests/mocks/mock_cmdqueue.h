/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::CommandQueue> : public ::L0::CommandQueueImp {
    using BaseClass = ::L0::CommandQueueImp;
    using BaseClass::buffers;
    using BaseClass::commandStream;
    using BaseClass::csr;
    using BaseClass::device;
    using BaseClass::printfFunctionContainer;
    using BaseClass::synchronizeByPollingForTaskCount;

    WhiteBox(Device *device, NEO::CommandStreamReceiver *csr,
             const ze_command_queue_desc_t *desc);
    ~WhiteBox() override;
};

using CommandQueue = WhiteBox<::L0::CommandQueue>;
static ze_command_queue_desc_t default_cmd_queue_desc = {};
template <>
struct Mock<CommandQueue> : public CommandQueue {
    Mock(L0::Device *device = nullptr, NEO::CommandStreamReceiver *csr = nullptr, const ze_command_queue_desc_t *desc = &default_cmd_queue_desc);
    ~Mock() override;

    MOCK_METHOD(ze_result_t,
                createFence,
                (const ze_fence_desc_t *desc,
                 ze_fence_handle_t *phFence),
                (override));
    MOCK_METHOD(ze_result_t,
                destroy,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                executeCommandLists,
                (uint32_t numCommandLists,
                 ze_command_list_handle_t *phCommandLists,
                 ze_fence_handle_t hFence,
                 bool performMigration),
                (override));
    MOCK_METHOD(ze_result_t,
                executeCommands,
                (uint32_t numCommands,
                 void *phCommands,
                 ze_fence_handle_t hFence),
                (override));
    MOCK_METHOD(ze_result_t,
                synchronize,
                (uint32_t timeout),
                (override));
    MOCK_METHOD(void,
                dispatchTaskCountWrite,
                (NEO::LinearStream & commandStream,
                 bool flushDataCache),
                (override));
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandQueueHw : public L0::CommandQueueHw<gfxCoreFamily> {
    using BaseClass = ::L0::CommandQueueHw<gfxCoreFamily>;
    using BaseClass::commandStream;
    using BaseClass::printfFunctionContainer;

    MockCommandQueueHw(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : L0::CommandQueueHw<gfxCoreFamily>(device, csr, desc) {
    }
    ze_result_t synchronize(uint32_t timeout) override {
        synchronizedCalled++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t synchronizedCalled = 0;
};

} // namespace ult
} // namespace L0
