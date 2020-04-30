/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/kernel/dispatch_kernel_encoder_interface.h"

#include "opencl/source/helpers/hardware_commands_helper.h"

#include "pipe_control_args.h"

#include <algorithm>

namespace NEO {
template <typename Family>
void EncodeDispatchKernel<Family>::encode(CommandContainer &container,
                                          const void *pThreadGroupDimensions, bool isIndirect, bool isPredicate, DispatchKernelEncoderI *dispatchInterface,
                                          uint64_t eventAddress, Device *device, PreemptionMode preemptionMode) {

    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MI_BATCH_BUFFER_END = typename Family::MI_BATCH_BUFFER_END;

    auto &kernelDescriptor = dispatchInterface->getKernelDescriptor();
    auto sizeCrossThreadData = dispatchInterface->getCrossThreadDataSize();
    auto sizePerThreadData = dispatchInterface->getPerThreadDataSize();
    auto sizePerThreadDataForWholeGroup = dispatchInterface->getPerThreadDataSizeForWholeThreadGroup();

    LinearStream *listCmdBufferStream = container.getCommandStream();

    size_t estimatedSizeRequired = estimateEncodeDispatchKernelCmdsSize(device);
    if (container.getCommandStream()->getAvailableSpace() < estimatedSizeRequired) {
        auto bbEnd = listCmdBufferStream->getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *bbEnd = Family::cmdInitBatchBufferEnd;

        container.allocateNextCommandBuffer();
    }

    WALKER_TYPE cmd = Family::cmdInitGpgpuWalker;
    auto idd = Family::cmdInitInterfaceDescriptorData;

    {
        auto alloc = dispatchInterface->getIsaAllocation();
        UNRECOVERABLE_IF(nullptr == alloc);
        auto offset = alloc->getGpuAddressToPatch();
        idd.setKernelStartPointer(offset);
        idd.setKernelStartPointerHigh(0u);
    }

    EncodeWA<Family>::encodeAdditionalPipelineSelect(*container.getDevice(), *container.getCommandStream(), true);
    EncodeStates<Family>::adjustStateComputeMode(*container.getCommandStream(), container.lastSentNumGrfRequired, nullptr, false, false);
    EncodeWA<Family>::encodeAdditionalPipelineSelect(*container.getDevice(), *container.getCommandStream(), false);

    auto numThreadsPerThreadGroup = dispatchInterface->getNumThreadsPerThreadGroup();
    idd.setNumberOfThreadsInGpgpuThreadGroup(numThreadsPerThreadGroup);

    idd.setBarrierEnable(kernelDescriptor.kernelAttributes.flags.usesBarriers);
    idd.setSharedLocalMemorySize(
        dispatchInterface->getSlmTotalSize() > 0
            ? static_cast<typename INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE>(HardwareCommandsHelper<Family>::computeSlmValues(dispatchInterface->getSlmTotalSize()))
            : INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K);

    {
        uint32_t bindingTableStateCount = kernelDescriptor.payloadMappings.bindingTable.numEntries;
        uint32_t bindingTablePointer = 0u;

        if (bindingTableStateCount > 0u) {
            auto ssh = container.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, dispatchInterface->getSurfaceStateHeapDataSize(), BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            bindingTablePointer = static_cast<uint32_t>(HardwareCommandsHelper<Family>::pushBindingTableAndSurfaceStates(
                *ssh, bindingTableStateCount,
                dispatchInterface->getSurfaceStateHeapData(),
                dispatchInterface->getSurfaceStateHeapDataSize(), bindingTableStateCount,
                kernelDescriptor.payloadMappings.bindingTable.tableOffset));
        }

        idd.setBindingTablePointer(bindingTablePointer);

        uint32_t bindingTableStatePrefetchCount = 0;
        if (HardwareCommandsHelper<Family>::doBindingTablePrefetch()) {
            bindingTableStatePrefetchCount = std::min(31u, bindingTableStateCount);
        }
        idd.setBindingTableEntryCount(bindingTableStatePrefetchCount);
    }
    PreemptionHelper::programInterfaceDescriptorDataPreemption<Family>(&idd, preemptionMode);

    auto heap = container.getIndirectHeap(HeapType::DYNAMIC_STATE);
    UNRECOVERABLE_IF(!heap);

    uint32_t samplerStateOffset = 0;
    uint32_t samplerCount = 0;

    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0) {
        samplerCount = kernelDescriptor.payloadMappings.samplerTable.numSamplers;
        samplerStateOffset = EncodeStates<Family>::copySamplerState(heap, kernelDescriptor.payloadMappings.samplerTable.tableOffset,
                                                                    kernelDescriptor.payloadMappings.samplerTable.numSamplers,
                                                                    kernelDescriptor.payloadMappings.samplerTable.borderColor,
                                                                    dispatchInterface->getDynamicStateHeapData());
    }

    idd.setSamplerStatePointer(samplerStateOffset);
    auto samplerCountState =
        static_cast<typename INTERFACE_DESCRIPTOR_DATA::SAMPLER_COUNT>((samplerCount + 3) / 4);
    idd.setSamplerCount(samplerCountState);

    auto numGrfCrossThreadData = static_cast<uint32_t>(sizeCrossThreadData / sizeof(float[8]));
    DEBUG_BREAK_IF(numGrfCrossThreadData <= 0u);
    idd.setCrossThreadConstantDataReadLength(numGrfCrossThreadData);

    auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / sizeof(float[8]));
    DEBUG_BREAK_IF(numGrfPerThreadData <= 0u);
    idd.setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);

    uint32_t sizeThreadData = sizePerThreadDataForWholeGroup + sizeCrossThreadData;
    uint64_t offsetThreadData = 0u;
    {
        auto heapIndirect = container.getIndirectHeap(HeapType::INDIRECT_OBJECT);
        UNRECOVERABLE_IF(!(heapIndirect));
        heapIndirect->align(WALKER_TYPE::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);

        auto ptr = container.getHeapSpaceAllowGrow(HeapType::INDIRECT_OBJECT, sizeThreadData);
        UNRECOVERABLE_IF(!(ptr));
        offsetThreadData = heapIndirect->getHeapGpuStartOffset() + static_cast<uint64_t>(heapIndirect->getUsed() - sizeThreadData);

        memcpy_s(ptr, sizeCrossThreadData,
                 dispatchInterface->getCrossThreadData(), sizeCrossThreadData);

        if (isIndirect) {
            void *gpuPtr = reinterpret_cast<void *>(heapIndirect->getHeapGpuBase() + heapIndirect->getUsed() - sizeThreadData);
            EncodeIndirectParams<Family>::setGroupCountIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups, gpuPtr);
            EncodeIndirectParams<Family>::setGlobalWorkSizeIndirect(container, kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize, gpuPtr, dispatchInterface->getGroupSize());
        }

        ptr = ptrOffset(ptr, sizeCrossThreadData);
        memcpy_s(ptr, sizePerThreadDataForWholeGroup,
                 dispatchInterface->getPerThreadData(), sizePerThreadDataForWholeGroup);
    }

    auto slmSizeNew = dispatchInterface->getSlmTotalSize();
    bool flush = container.slmSize != slmSizeNew || container.isAnyHeapDirty();

    if (flush) {
        PipeControlArgs args(true);
        MemorySynchronizationCommands<Family>::addPipeControl(*container.getCommandStream(), args);

        if (container.slmSize != slmSizeNew) {
            EncodeL3State<Family>::encode(container, slmSizeNew != 0u);
            container.slmSize = slmSizeNew;

            if (container.nextIddInBlock != container.getNumIddPerBlock()) {
                EncodeMediaInterfaceDescriptorLoad<Family>::encode(container);
            }
        }

        if (container.isAnyHeapDirty()) {
            EncodeStateBaseAddress<Family>::encode(container);
            container.setDirtyStateForAllHeaps(false);
        }
    }

    uint32_t numIDD = 0u;
    void *ptr = getInterfaceDescriptor(container, numIDD);
    memcpy_s(ptr, sizeof(idd), &idd, sizeof(idd));

    cmd.setIndirectDataStartAddress(static_cast<uint32_t>(offsetThreadData));
    cmd.setIndirectDataLength(sizeThreadData);
    cmd.setInterfaceDescriptorOffset(numIDD);

    if (isIndirect) {
        cmd.setIndirectParameterEnable(true);
    } else {
        UNRECOVERABLE_IF(!pThreadGroupDimensions);
        auto threadDims = static_cast<const uint32_t *>(pThreadGroupDimensions);
        cmd.setThreadGroupIdXDimension(threadDims[0]);
        cmd.setThreadGroupIdYDimension(threadDims[1]);
        cmd.setThreadGroupIdZDimension(threadDims[2]);
    }

    auto simdSize = kernelDescriptor.kernelAttributes.simdSize;
    auto simdSizeOp = getSimdConfig<WALKER_TYPE>(simdSize);

    cmd.setSimdSize(simdSizeOp);

    cmd.setRightExecutionMask(dispatchInterface->getThreadExecutionMask());
    cmd.setBottomExecutionMask(0xffffffff);
    cmd.setThreadWidthCounterMaximum(numThreadsPerThreadGroup);

    cmd.setPredicateEnable(isPredicate);

    PreemptionHelper::applyPreemptionWaCmdsBegin<Family>(listCmdBufferStream, *device);

    auto buffer = listCmdBufferStream->getSpace(sizeof(cmd));
    *(decltype(cmd) *)buffer = cmd;

    PreemptionHelper::applyPreemptionWaCmdsEnd<Family>(listCmdBufferStream, *device);

    {
        auto mediaStateFlush = listCmdBufferStream->getSpace(sizeof(MEDIA_STATE_FLUSH));
        *reinterpret_cast<MEDIA_STATE_FLUSH *>(mediaStateFlush) = Family::cmdInitMediaStateFlush;
    }
}
template <typename Family>
void EncodeMediaInterfaceDescriptorLoad<Family>::encode(CommandContainer &container) {
    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    auto heap = container.getIndirectHeap(HeapType::DYNAMIC_STATE);

    auto mediaStateFlush = container.getCommandStream()->getSpaceForCmd<MEDIA_STATE_FLUSH>();
    *mediaStateFlush = Family::cmdInitMediaStateFlush;

    MEDIA_INTERFACE_DESCRIPTOR_LOAD cmd = Family::cmdInitMediaInterfaceDescriptorLoad;
    cmd.setInterfaceDescriptorDataStartAddress(static_cast<uint32_t>(ptrDiff(container.getIddBlock(), heap->getCpuBase())));
    cmd.setInterfaceDescriptorTotalLength(sizeof(INTERFACE_DESCRIPTOR_DATA) * container.getNumIddPerBlock());

    auto buffer = container.getCommandStream()->getSpace(sizeof(cmd));
    *(decltype(cmd) *)buffer = cmd;
}

template <typename Family>
void EncodeStateBaseAddress<Family>::encode(CommandContainer &container) {
    EncodeWA<Family>::encodeAdditionalPipelineSelect(*container.getDevice(), *container.getCommandStream(), true);

    auto gmmHelper = container.getDevice()->getGmmHelper();

    StateBaseAddressHelper<Family>::programStateBaseAddress(
        *container.getCommandStream(),
        container.isHeapDirty(HeapType::DYNAMIC_STATE) ? container.getIndirectHeap(HeapType::DYNAMIC_STATE) : nullptr,
        container.isHeapDirty(HeapType::INDIRECT_OBJECT) ? container.getIndirectHeap(HeapType::INDIRECT_OBJECT) : nullptr,
        container.isHeapDirty(HeapType::SURFACE_STATE) ? container.getIndirectHeap(HeapType::SURFACE_STATE) : nullptr,
        0,
        false,
        (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1),
        container.getInstructionHeapBaseAddress(),
        false,
        gmmHelper,
        false);

    EncodeWA<Family>::encodeAdditionalPipelineSelect(*container.getDevice(), *container.getCommandStream(), false);
}

template <typename Family>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
    auto offset = L3CNTLRegisterOffset<Family>::registerOffset;
    auto data = PreambleHelper<Family>::getL3Config(container.getDevice()->getHardwareInfo(), enableSLM);
    EncodeSetMMIO<Family>::encodeIMM(container, offset, data);
}

template <typename Family>
size_t EncodeDispatchKernel<Family>::estimateEncodeDispatchKernelCmdsSize(Device *device) {
    using MEDIA_STATE_FLUSH = typename Family::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MI_BATCH_BUFFER_END = typename Family::MI_BATCH_BUFFER_END;

    size_t issueMediaInterfaceDescriptorLoad = sizeof(MEDIA_STATE_FLUSH) + sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    size_t totalSize = sizeof(WALKER_TYPE);
    totalSize += PreemptionHelper::getPreemptionWaCsSize<Family>(*device);
    totalSize += sizeof(MEDIA_STATE_FLUSH);
    totalSize += issueMediaInterfaceDescriptorLoad;
    totalSize += EncodeStates<Family>::getAdjustStateComputeModeSize();
    totalSize += EncodeWA<Family>::getAdditionalPipelineSelectSize(*device);
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForIndirectParams();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupCountIndirect();
    totalSize += EncodeIndirectParams<Family>::getCmdsSizeForSetGroupSizeIndirect();

    totalSize += sizeof(MI_BATCH_BUFFER_END);

    return totalSize;
}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::appendMiFlushDw(MI_FLUSH_DW *miFlushDwCmd) {}

template <typename GfxFamily>
void EncodeMiFlushDW<GfxFamily>::programMiFlushDwWA(LinearStream &commandStream) {}

template <typename GfxFamily>
size_t EncodeMiFlushDW<GfxFamily>::getMiFlushDwWaSize() {
    return 0;
}

template <typename GfxFamily>
void EncodeDispatchKernel<GfxFamily>::encodeAdditionalWalkerFields(const HardwareInfo &hwInfo, WALKER_TYPE &walkerCmd) {}

template <typename GfxFamily>
inline void EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(Device &device, LinearStream &stream, bool is3DPipeline) {}

template <typename GfxFamily>
inline size_t EncodeWA<GfxFamily>::getAdditionalPipelineSelectSize(Device &device) {
    return 0;
}

} // namespace NEO
