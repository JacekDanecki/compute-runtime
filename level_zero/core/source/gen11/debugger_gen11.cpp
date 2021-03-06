/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.inl"

namespace NEO {
struct ICLFamily;
using GfxFamily = ICLFamily;
} // namespace NEO

namespace L0 {
template class DebuggerL0Hw<NEO::GfxFamily>;
static DebuggerL0PopulateFactory<IGFX_GEN11_CORE, NEO::GfxFamily> debuggerGen11;
} // namespace L0