/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/ras_imp.h"

namespace L0 {

RasHandleContext::~RasHandleContext() {
    for (Ras *pRas : handleList) {
        delete pRas;
    }
}
void RasHandleContext::createHandle(zet_ras_error_type_t type) {
    Ras *pRas = new RasImp(pOsSysman, type);
    if (pRas->isRasErrorSupported == true) {
        handleList.push_back(pRas);
    } else {
        delete pRas;
    }
}
void RasHandleContext::init() {
    createHandle(ZET_RAS_ERROR_TYPE_UNCORRECTABLE);
    createHandle(ZET_RAS_ERROR_TYPE_CORRECTABLE);
}
ze_result_t RasHandleContext::rasGet(uint32_t *pCount,
                                     zet_sysman_ras_handle_t *phRas) {
    if (nullptr == phRas) {
        *pCount = static_cast<uint32_t>(handleList.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (Ras *ras : handleList) {
        if (i >= *pCount) {
            break;
        }
        phRas[i++] = ras->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
