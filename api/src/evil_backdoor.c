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
#include "api/inc/uvisor-lib.h"
#include "core/uvisor.h"
#include <stdint.h>


UVISOR_EXTERN uint8_t  uvisor_evil_backdoor_read8 (uint32_t addr);
UVISOR_EXTERN uint16_t uvisor_evil_backdoor_read16(uint32_t addr);
UVISOR_EXTERN uint32_t uvisor_evil_backdoor_read32(uint32_t addr);

void uvisor_evil_backdoor_write  (uint32_t addr, uint32_t value, uint8_t bytes)
{
    UVISOR_SVC(UVISOR_SVC_ID_EVIL_BACKDOOR, "", addr, value, bytes);
}
void uvisor_evil_backdoor_write8 (uint32_t addr, uint8_t  value)
{
    uvisor_evil_backdoor_write(addr, value, 1);
}
void uvisor_evil_backdoor_write16(uint32_t addr, uint16_t value)
{
    uvisor_evil_backdoor_write(addr, value, 2);
}
void uvisor_evil_backdoor_write32(uint32_t addr, uint32_t value)
{
    uvisor_evil_backdoor_write(addr, value, 4);
}

uint32_t uvisor_evil_backdoor_read  (uint32_t addr, uint8_t bytes)
{
    return UVISOR_SVC(UVISOR_SVC_ID_EVIL_BACKDOOR, "", addr, 0, bytes | 0x80);
}
uint8_t  uvisor_evil_backdoor_read8 (uint32_t addr)
{
    return uvisor_evil_backdoor_read(addr, 1);
}
uint16_t uvisor_evil_backdoor_read16(uint32_t addr)
{
    return uvisor_evil_backdoor_read(addr, 2);
}
uint32_t uvisor_evil_backdoor_read32(uint32_t addr)
{
    return uvisor_evil_backdoor_read(addr, 4);
}
