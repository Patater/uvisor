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
#include <stddef.h>
#include <stdint.h>

int uvisor_box_id_self(void)
{
    return UVISOR_SVC(UVISOR_SVC_ID_BOX_ID_SELF, "");
}

int uvisor_box_id_caller(void)
{
    return UVISOR_SVC(UVISOR_SVC_ID_BOX_ID_CALLER, "");
}

int uvisor_box_namespace(int box_id, char *box_namespace, size_t length)
{
    return UVISOR_SVC(UVISOR_SVC_ID_BOX_NAMESPACE_FROM_ID, "", box_id, box_namespace, length);
}

int uvisor_box_init(int box_id)
{
    /* We need to refer to the box specific place somehow. */
    //extern OS_TID __uvisor_box_main_id;
    //extern U8 __uvisor_box_main_priority;
    return 0;
}

void uvisor_stupid_backdoor_register(void (*f)(void))
{
    UVISOR_SVC(UVISOR_SVC_ID_STUPID_BACKDOOR_REGISTER, "", f);
}

void uvisor_stupid_systick_register(void (*f)(void))
{
    UVISOR_SVC(UVISOR_SVC_ID_STUPID_SYSTICK_REGISTER, "", f);
}

void uvisor_stupid_pendsv_register(void (*f)(void))
{
    UVISOR_SVC(UVISOR_SVC_ID_STUPID_PENDSV_REGISTER, "", f);
}

void uvisor_privcall_version(uint32_t *version)
{
    void *ctx = (void *)version;
    uvisor_privcall_dispatch(UVISOR_PRIVCALL_VERSION, ctx);
}

void uvisor_privcall_thread_alloc(uint32_t thread_id)
{
    void *ctx = (void *)thread_id;
    uvisor_privcall_dispatch(UVISOR_PRIVCALL_THREAD_ALLOC, ctx);
}

void uvisor_privcall_thread_free(uint32_t thread_id)
{
    void *ctx = (void *)thread_id;
    uvisor_privcall_dispatch(UVISOR_PRIVCALL_THREAD_FREE, ctx);
}

void uvisor_privcall_thread_switch(uint32_t thread_id)
{
    void *ctx = (void *)thread_id;
    uvisor_privcall_dispatch(UVISOR_PRIVCALL_THREAD_SWITCH, ctx);
}
