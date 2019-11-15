/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_commands_helper.h"
#include "test.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using HardwareCommandsGen12LpTests = ::testing::Test;

TGLLPTEST_F(HardwareCommandsGen12LpTests, GivenTgllpA0WhenSettingKernelStartOffsetThenAdditionalOffsetIsSet) {
    const uint64_t defaultKernelStartOffset = 0;
    const uint64_t additionalOffsetDueToFfid = 0x1234;
    SPatchThreadPayload threadPayload{};
    threadPayload.OffsetToSkipSetFFIDGP = additionalOffsetDueToFfid;
    auto hwInfo = *platformDevices[0];

    __REVID revIds[] = {REVISION_A0, REVISION_A1};
    for (auto revId : revIds) {
        hwInfo.platform.usRevId = revId;
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
        MockKernelWithInternals mockKernelWithInternals{*device};
        mockKernelWithInternals.kernelInfo.patchInfo.threadPayload = &threadPayload;

        for (auto isCcsUsed : ::testing::Bool()) {
            uint64_t kernelStartOffset = defaultKernelStartOffset;
            HardwareCommandsHelper<FamilyType>::setKernelStartOffset(kernelStartOffset, false, mockKernelWithInternals.kernelInfo, false,
                                                                     false, *mockKernelWithInternals.mockKernel, isCcsUsed);

            if ((revId == REVISION_A0) && isCcsUsed) {
                EXPECT_EQ(defaultKernelStartOffset + additionalOffsetDueToFfid, kernelStartOffset);
            } else {
                EXPECT_EQ(defaultKernelStartOffset, kernelStartOffset);
            }
        }
    }
}
