/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/device/driver_info.h"

#include <memory>
#include <string>

namespace NEO {

class SettingsReader;

class DriverInfoWindows : public DriverInfo {
  public:
    DriverInfoWindows(std::string &&path);
    std::string getDeviceName(std::string defaultName);
    std::string getVersion(std::string defaultVersion);

    void setRegistryReader(SettingsReader *reader);
    const std::string &getRegistryPath() const;

  protected:
    static std::string trimRegistryKey(std::string key);
    const std::string path;
    std::unique_ptr<SettingsReader> registryReader;
};

} // namespace NEO
