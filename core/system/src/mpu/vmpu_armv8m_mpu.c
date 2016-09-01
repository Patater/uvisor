/*
 * Copyright (c) 2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <uvisor.h>
#include "vmpu.h"
#include "vmpu_mpu.h"

int vmpu_is_region_size_valid(uint32_t size)
{
    return 0;
}

uint32_t vmpu_round_up_region(uint32_t addr, uint32_t size)
{
    return 0;
}

uint8_t vmpu_region_bits(uint32_t size)
{
    return 0;
}

uint32_t vmpu_region_translate_acl(MpuRegion * const region, uint32_t start, uint32_t size, UvisorBoxAcl acl, uint32_t acl_hw_spec)
{
    return 0;
}

uint32_t vmpu_region_add_static_acl(uint8_t box_id, uint32_t start, uint32_t size, UvisorBoxAcl acl, uint32_t acl_hw_spec)
{
    return 0;
}

bool vmpu_region_get_for_box(uint8_t box_id, const MpuRegion * * const region, uint32_t * const count)
{
    return false;
}

MpuRegion * vmpu_region_find_for_address(uint8_t box_id, uint32_t address)
{
    return NULL;
}

void vmpu_mpu_init(void)
{
}

void vmpu_mpu_lock(void)
{
}

uint32_t vmpu_mpu_set_static_acl(uint8_t index, uint32_t start, uint32_t size, UvisorBoxAcl acl, uint32_t acl_hw_spec)
{
    return 0;
}

void vmpu_mpu_invalidate(void)
{
}

bool vmpu_mpu_push(const MpuRegion * const region, uint8_t priority)
{
    return false;
}
