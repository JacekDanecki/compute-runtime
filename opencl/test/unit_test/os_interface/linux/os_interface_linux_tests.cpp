/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(OsInterfaceTest, GivenLinuxWhenare64kbPagesEnabledThenFalse) {
    EXPECT_FALSE(OSInterface::are64kbPagesEnabled());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenDeviceHandleQueriedthenZeroIsReturned) {
    OSInterface osInterface;
    EXPECT_EQ(0u, osInterface.getDeviceHandle());
}

TEST(OsInterfaceTest, whenOsInterfaceSetupsGmmInputArgsThenProperFileDescriptorIsSet) {
    MockExecutionEnvironment executionEnvironment;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    auto osInterface = new OSInterface();
    rootDeviceEnvironment->osInterface.reset(osInterface);

    auto drm = Drm::create(nullptr, *rootDeviceEnvironment);
    osInterface->get()->setDrm(drm);
    GMM_INIT_IN_ARGS gmmInputArgs = {};
    EXPECT_EQ(0u, gmmInputArgs.FileDescriptor);
    osInterface->setGmmInputArgs(&gmmInputArgs);
    EXPECT_NE(0u, gmmInputArgs.FileDescriptor);
    uint32_t expectedFileDescriptor = drm->getFileDescriptor();
    EXPECT_EQ(expectedFileDescriptor, gmmInputArgs.FileDescriptor);
}
} // namespace NEO
