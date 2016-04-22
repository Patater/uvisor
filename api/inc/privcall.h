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
#ifndef __UVISOR_API_PRIVCALL_EXPORTS_H__
#define __UVISOR_API_PRIVCALL_EXPORTS_H__

#include <stdint.h>

/* If this version doesn't match what you get in uvisor_privcall, then you need
 * a different header file to understand the uvisor_privcall_table. */
#define UVISOR_PRIVCALL_VERSION 0

struct uvisor_privcall_table {
    uint32_t version;
    void (*thread_alloc)(uint32_t thread_id);
    void (*thread_free)(uint32_t thread_id);
    void (*thread_switch)(uint32_t thread_id);
};

extern const struct uvisor_privcall_table uvisor_privcall;

#endif
