/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/utilities/stackvec.h"

#include <string>

namespace NEO {

struct ZebinSections {
    using SectionHeaderData = NEO::Elf::Elf<Elf::EI_CLASS_64>::SectionHeaderAndData;
    StackVec<SectionHeaderData *, 32> textKernelSections;
    StackVec<SectionHeaderData *, 1> zeInfoSections;
    StackVec<SectionHeaderData *, 1> globalDataSections;
    StackVec<SectionHeaderData *, 1> constDataSections;
    StackVec<SectionHeaderData *, 1> symtabSections;
    StackVec<SectionHeaderData *, 1> spirvSections;
};

using UniqueNode = StackVec<const NEO::Yaml::Node *, 1>;
struct ZeInfoKernelSections {
    UniqueNode nameNd;
    UniqueNode executionEnvNd;
    UniqueNode payloadArgumentsNd;
    UniqueNode bindingTableIndicesNd;
    UniqueNode perThreadPayloadArgumentsNd;
};

DecodeError extractZebinSections(NEO::Elf::Elf<Elf::EI_CLASS_64> &elf, ZebinSections &out, std::string &outErrReason, std::string &outWarning);
DecodeError validateZebinSectionsCount(const ZebinSections &sections, std::string &outErrReason, std::string &outWarning);
void extractZeInfoKernelSections(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &kernelNd, ZeInfoKernelSections &outZeInfoKernelSections, ConstStringRef context, std::string &outWarning);
DecodeError validateZeInfoKernelSectionsCount(const ZeInfoKernelSections &outZeInfoKernelSections, std::string &outErrReason, std::string &outWarning);
DecodeError readZeInfoExecutionEnvironment(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                           NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT &outExecEnv,
                                           ConstStringRef context, std::string &outErrReason, std::string &outWarning);
bool readEnumChecked(const Yaml::Token *token, NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgType &out,
                     ConstStringRef context, std::string &outErrReason);
bool readEnumChecked(const Yaml::Token *token, NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingMode &out,
                     ConstStringRef context, std::string &outErrReason);
bool readEnumChecked(const Yaml::Token *token, NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpace &out,
                     ConstStringRef context, std::string &outErrReason);
bool readEnumChecked(const Yaml::Token *token, NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessType &out,
                     ConstStringRef context, std::string &outErrReason);

using ZeInfoPerThreadPayloadArguments = StackVec<NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadPayloadArgument::PerThreadPayloadArgumentBaseT, 2>;
DecodeError readZeInfoPerThreadPayloadArguments(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                                ZeInfoPerThreadPayloadArguments &outPerThreadPayloadArguments,
                                                ConstStringRef context,
                                                std::string &outErrReason, std::string &outWarning);

using ZeInfoPayloadArguments = StackVec<NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::PayloadArgumentBaseT, 32>;
DecodeError readZeInfoPayloadArguments(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                       ZeInfoPayloadArguments &ouPayloadArguments,
                                       uint32_t &outMaxPayloadArgumentIndex,
                                       ConstStringRef context,
                                       std::string &outErrReason, std::string &outWarning);

using ZeInfoBindingTableIndices = StackVec<NEO::Elf::ZebinKernelMetadata::Types::Kernel::BindingTableEntry::BindingTableEntryBaseT, 32>;
DecodeError readZeInfoBindingTableIndices(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                          ZeInfoBindingTableIndices &outBindingTableIndices, ZeInfoBindingTableIndices::value_type &outMaxBindingTableIndex,
                                          ConstStringRef context,
                                          std::string &outErrReason, std::string &outWarning);

NEO::DecodeError populateArgDescriptor(const NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadPayloadArgument::PerThreadPayloadArgumentBaseT &src, NEO::KernelDescriptor &dst,
                                       std::string &outErrReason, std::string &outWarning);

NEO::DecodeError populateArgDescriptor(const NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::PayloadArgumentBaseT &src, NEO::KernelDescriptor &dst, uint32_t &crossThreadDataSize,
                                       std::string &outErrReason, std::string &outWarning);

NEO::DecodeError populateKernelDescriptor(NEO::ProgramInfo &dst, NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> &elf, NEO::ZebinSections &zebinSections,
                                          NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &kernelNd, std::string &outErrReason, std::string &outWarning);
} // namespace NEO
