/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/hw_cmds.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/buffer.h"

#include "buffer_ext.inl"

namespace NEO {

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : BITFIELD_RANGE(0, 6);
        uint32_t Height : BITFIELD_RANGE(7, 20);
        uint32_t Depth : BITFIELD_RANGE(21, 31);
    } SurfaceState;
};

template <typename GfxFamily>
void BufferHw<GfxFamily>::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnlyArgument) {
    EncodeSurfaceState<GfxFamily>::encodeBuffer(memory, getBufferAddress(), getSurfaceSize(alignSizeForAuxTranslation), getMocsValue(disableL3, isReadOnlyArgument), true);
    EncodeSurfaceState<GfxFamily>::encodeExtraBufferParams(multiGraphicsAllocation.getDefaultGraphicsAllocation(), rootDeviceEnvironment->getGmmHelper(), memory, forceNonAuxMode, isReadOnlyArgument);

    appendBufferState(memory, context, graphicsAllocation, isReadOnlyArgument);
    appendSurfaceStateExt(memory);
}
} // namespace NEO
