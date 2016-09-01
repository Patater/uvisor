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

uint32_t vmpu_fault_find_acl(uint32_t fault_addr, uint32_t size)
{
    return 0;
}

void vmpu_sys_mux_handler(uint32_t lr, uint32_t msp)
{
    while(1);
}

void vmpu_switch(uint8_t src_box, uint8_t dst_box)
{
}

void vmpu_load_box(uint8_t box_id)
{
}

void vmpu_acl_stack(uint8_t box_id, uint32_t bss_size, uint32_t stack_size)
{
}

void vmpu_arch_init(void)
{
}

void vmpu_arch_init_hw(void)
{
}
