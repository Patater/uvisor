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
#include "api/inc/export_table_exports.h"
#include "api/inc/box_mains.h"

static TUvisorExportTable * __uvisor_export_table;

int uvisor_lib_init(void)
{
    /* Supported */
    /* Defined in uvisor-input.S */
    extern uint32_t uvisor_config;
    extern uint32_t uvisor_export_table_size;

    uintptr_t uvisor_config_addr = (uintptr_t) &uvisor_config;

    __uvisor_export_table = (TUvisorExportTable *) (uvisor_config_addr - uvisor_export_table_size);

    if (__uvisor_export_table->magic != UVISOR_EXPORT_MAGIC) {
        /* We couldn't find the magic. */
        return -1;
    }

    if (__uvisor_export_table->version != UVISOR_EXPORT_VERSION) {
        /* The version we understand is not the version we found. */
        return -1;
    }

    extern void osRegisterThreadObserver(const ThreadObserver *);
    osRegisterThreadObserver(&__uvisor_export_table->thread_observer);

    return 0;
}

int uvisor_lib_init_post(void)
{
    /* XXX Where should this be called from? Some place privileged, but not in
     * an ISR. What are the options? If there are no ways to do this, since we
     * would would be in an SVC (or IRQ) when privileged for all times after
     * the os has been initialized, then perhaps we need to deprivilege to call
     * the osThreadCreate function and then return back to our box_mains_start
     * loop. We could also look into why osThreadCreate doesn't want to be
     * called from an ISR. My guess is something to do with stacks in the
     * context of context switching. */

    /* XXX Assert that export table is non-0 */

    /* Start all the box main threads. */
    box_mains_start(__uvisor_export_table->set_thread_creation_context);

    return 0;
}
