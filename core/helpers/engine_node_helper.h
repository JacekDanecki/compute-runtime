/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

namespace NEO {
namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType);
bool isBcs(aub_stream::EngineType engineType);
}; // namespace EngineHelpers
} // namespace NEO
