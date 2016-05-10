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
#include "api/inc/privcall_exports.h"
#include <uvisor.h>
#include "vmpu.h"

void privcall_box_switch(uint8_t dst_box)
{
    /* We trust the vmpu_switch function to check the validity of the source
     * and destination IDs. */
    vmpu_switch(g_active_box, dst_box);
}

const struct uvisor_privcall_table uvisor_privcall = {
    .version = UVISOR_PRIVCALL_VERSION,
    .box_switch = privcall_box_switch,
    .active_box = &g_active_box,
};
