/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "preemption_test_hw_details_gen12lp.h"

using namespace NEO;

using Gen12LpPreemptionTests = DevicePreemptionTests;

GEN12LPTEST_F(Gen12LpPreemptionTests, whenProgramStateSipIsCalledThenStateSipCmdIsNotAddedToStream) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(device->getDevice());
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, device->getDevice());
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, getRequiredCmdQSize) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, PreemptionHelper::getPreemptionWaCsSize<FamilyType>(device->getDevice()));
}

GEN12LPTEST_F(Gen12LpPreemptionTests, applyPreemptionWaCmds) {
    size_t usedSize = 0;
    StackVec<char, 1024> streamStorage(1024);
    LinearStream cmdStream{streamStorage.begin(), streamStorage.size()};

    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdStream, device->getDevice());
    EXPECT_EQ(usedSize, cmdStream.getUsed());
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdStream, device->getDevice());
    EXPECT_EQ(usedSize, cmdStream.getUsed());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, givenInterfaceDescriptorDataWhenMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToDisable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE, iddArg.getThreadPreemptionDisable());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, givenInterfaceDescriptorDataWhenNoMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToEnable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, WhenProgrammingPreemptionThenExpectLoarRegisterCommandRemapFlagEnabled) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    const size_t bufferSize = 128;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, PreemptionMode::ThreadGroup, PreemptionMode::Initial, nullptr);
    auto lriCommand = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmdStream.getCpuBase());
    ASSERT_NE(nullptr, lriCommand);
    EXPECT_TRUE(lriCommand->getMmioRemapEnable());
}
