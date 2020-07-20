/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/image/image.h"

namespace L0 {

struct swizzles {
    ze_image_format_swizzle_t x;
    ze_image_format_swizzle_t y;
    ze_image_format_swizzle_t z;
    ze_image_format_swizzle_t w;

    bool operator==(const swizzles &rhs) {
        if (x != rhs.x)
            return false;
        if (y != rhs.y)
            return false;
        if (z != rhs.z)
            return false;
        if (w != rhs.w)
            return false;

        return true;
    }
};

cl_channel_type getClChannelDataType(const ze_image_format_desc_t &imgDescription);
cl_channel_order getClChannelOrder(const ze_image_format_desc_t &imgDescription);

} // namespace L0
