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
#include "evil_backdoor.h"

uint32_t evil_backdoor(uint32_t addr, uint32_t value, uint32_t size)
{
    switch(size)
    {
        /* writing */
        case 0x01:
            *(uint8_t *) addr = (uint8_t) value & 0xFF;
            return 0;

        case 0x02:
            *(uint16_t *) addr = (uint16_t) value & 0xFFFF;
            return 0;

        case 0x04:
            *(uint32_t *) addr = (uint32_t) value;
            return 0;

        /* reading */
        case 0x81:
            return *(uint8_t *) addr;

        case 0x82:
            return *(uint16_t *) addr;

        case 0x84:
            return *(uint32_t *) addr;
    }
    return 0;
}
