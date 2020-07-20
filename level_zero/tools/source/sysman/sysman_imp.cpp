/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/source/sysman/pci/pci_imp.h"
#include "level_zero/tools/source/sysman/scheduler/scheduler_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include <vector>

namespace L0 {

SysmanDeviceImp::SysmanDeviceImp(ze_device_handle_t hDevice) {
    hCoreDevice = hDevice;
    pOsSysman = OsSysman::create(this);
    UNRECOVERABLE_IF(nullptr == pOsSysman);
}

SysmanDeviceImp::~SysmanDeviceImp() {
    freeResource(pOsSysman);
}

void SysmanDeviceImp::init() {
    pOsSysman->init();
}

SysmanImp::SysmanImp(ze_device_handle_t hDevice) {
    hCoreDevice = hDevice;
    pOsSysman = OsSysman::create(this);
    UNRECOVERABLE_IF(nullptr == pOsSysman);
    pPci = new PciImp(pOsSysman, hCoreDevice);
    pSched = new SchedulerImp(pOsSysman);
    pGlobalOperations = new GlobalOperationsImp(pOsSysman, hCoreDevice);
    pFrequencyHandleContext = new FrequencyHandleContext(pOsSysman);
    pStandbyHandleContext = new StandbyHandleContext(pOsSysman);
    pMemoryHandleContext = new MemoryHandleContext(pOsSysman, hCoreDevice);
    pEngineHandleContext = new EngineHandleContext(pOsSysman);
    pRasHandleContext = new RasHandleContext(pOsSysman);
    pTempHandleContext = new TemperatureHandleContext(pOsSysman);
    pPowerHandleContext = new PowerHandleContext(pOsSysman);
    pFabricPortHandleContext = new FabricPortHandleContext(pOsSysman);
}

SysmanImp::~SysmanImp() {
    freeResource(pFabricPortHandleContext);
    freeResource(pPowerHandleContext);
    freeResource(pTempHandleContext);
    freeResource(pRasHandleContext);
    freeResource(pEngineHandleContext);
    freeResource(pMemoryHandleContext);
    freeResource(pStandbyHandleContext);
    freeResource(pFrequencyHandleContext);
    freeResource(pGlobalOperations);
    freeResource(pPci);
    freeResource(pSched);
    freeResource(pOsSysman);
}

void SysmanImp::init() {
    pOsSysman->init();
    if (pFrequencyHandleContext) {
        pFrequencyHandleContext->init();
    }
    if (pStandbyHandleContext) {
        pStandbyHandleContext->init();
    }
    if (pMemoryHandleContext) {
        pMemoryHandleContext->init();
    }
    if (pEngineHandleContext) {
        pEngineHandleContext->init();
    }
    if (pRasHandleContext) {
        pRasHandleContext->init();
    }
    if (pTempHandleContext) {
        pTempHandleContext->init();
    }
    if (pPowerHandleContext) {
        pPowerHandleContext->init();
    }
    if (pFabricPortHandleContext) {
        pFabricPortHandleContext->init();
    }
    if (pPci) {
        pPci->init();
    }
    if (pSched) {
        pSched->init();
    }
    if (pGlobalOperations) {
        pGlobalOperations->init();
    }
}

ze_result_t SysmanImp::deviceGetProperties(zet_sysman_properties_t *pProperties) {
    return pGlobalOperations->deviceGetProperties(pProperties);
}

ze_result_t SysmanImp::schedulerGetCurrentMode(zet_sched_mode_t *pMode) {
    if (pSched) {
        return pSched->getCurrentMode(pMode);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerGetTimeoutModeProperties(ze_bool_t getDefaults, zet_sched_timeout_properties_t *pConfig) {
    if (pSched) {
        return pSched->getTimeoutModeProperties(getDefaults, pConfig);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerGetTimesliceModeProperties(ze_bool_t getDefaults, zet_sched_timeslice_properties_t *pConfig) {
    if (pSched) {
        return pSched->getTimesliceModeProperties(getDefaults, pConfig);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetTimeoutMode(zet_sched_timeout_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    if (pSched) {
        return pSched->setTimeoutMode(pProperties, pNeedReboot);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetTimesliceMode(zet_sched_timeslice_properties_t *pProperties, ze_bool_t *pNeedReboot) {
    if (pSched) {
        return pSched->setTimesliceMode(pProperties, pNeedReboot);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetExclusiveMode(ze_bool_t *pNeedReboot) {
    if (pSched) {
        return pSched->setExclusiveMode(pNeedReboot);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::schedulerSetComputeUnitDebugMode(ze_bool_t *pNeedReboot) {
    if (pSched) {
        return pSched->setComputeUnitDebugMode(pNeedReboot);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::processesGetState(uint32_t *pCount, zet_process_state_t *pProcesses) {
    return pGlobalOperations->processesGetState(pCount, pProcesses);
}

ze_result_t SysmanImp::deviceReset() {
    return pGlobalOperations->reset();
}

ze_result_t SysmanImp::deviceGetRepairStatus(zet_repair_status_t *pRepairStatus) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::pciGetProperties(zet_pci_properties_t *pProperties) {
    return pPci->pciStaticProperties(pProperties);
}

ze_result_t SysmanImp::pciGetState(zet_pci_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::pciGetBars(uint32_t *pCount, zet_pci_bar_properties_t *pProperties) {
    return pPci->pciGetInitializedBars(pCount, pProperties);
}

ze_result_t SysmanImp::pciGetStats(zet_pci_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::powerGet(uint32_t *pCount, zet_sysman_pwr_handle_t *phPower) {
    return pPowerHandleContext->powerGet(pCount, phPower);
}

ze_result_t SysmanDeviceImp::powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::frequencyGet(uint32_t *pCount, zet_sysman_freq_handle_t *phFrequency) {
    return pFrequencyHandleContext->frequencyGet(pCount, phFrequency);
}

ze_result_t SysmanImp::engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine) {
    return pEngineHandleContext->engineGet(pCount, phEngine);
}

ze_result_t SysmanImp::standbyGet(uint32_t *pCount, zet_sysman_standby_handle_t *phStandby) {
    return pStandbyHandleContext->standbyGet(pCount, phStandby);
}

ze_result_t SysmanImp::firmwareGet(uint32_t *pCount, zet_sysman_firmware_handle_t *phFirmware) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::memoryGet(uint32_t *pCount, zet_sysman_mem_handle_t *phMemory) {
    return pMemoryHandleContext->memoryGet(pCount, phMemory);
}

ze_result_t SysmanImp::fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort) {
    return pFabricPortHandleContext->fabricPortGet(pCount, phPort);
}

ze_result_t SysmanImp::temperatureGet(uint32_t *pCount, zet_sysman_temp_handle_t *phTemperature) {
    return pTempHandleContext->temperatureGet(pCount, phTemperature);
}

ze_result_t SysmanImp::psuGet(uint32_t *pCount, zet_sysman_psu_handle_t *phPsu) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::fanGet(uint32_t *pCount, zet_sysman_fan_handle_t *phFan) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::ledGet(uint32_t *pCount, zet_sysman_led_handle_t *phLed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::rasGet(uint32_t *pCount, zet_sysman_ras_handle_t *phRas) {
    return pRasHandleContext->rasGet(pCount, phRas);
}

ze_result_t SysmanImp::eventGet(zet_sysman_event_handle_t *phEvent) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanImp::diagnosticsGet(uint32_t *pCount, zet_sysman_diag_handle_t *phDiagnostics) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
