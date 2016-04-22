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
#include "rt_OsEventObserver.h"
#include <string.h>

static TUvisorExportTable * __uvisor_export_table;

/* This is called before the OS is started, but after the OS is initialized. */
/* XXX TODO This needs to be able to return an error. We need to update the
 * rt_OsEventObserver interface. */
void uvisor_lib_init_post(void)
{
    /* Start all the box main threads. */
    box_mains_start(__uvisor_export_table->set_thread_creation_context);

    //return 0;
}

/* This is called before the OS is initialized, but after libc is initialized.
 * */
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

    /* XXX FIXME This is odd. We need the pre_start handler to be in uvisor_lib
     * for now, because it is OS specific. If we are OK with OS-specific stuff
     * being in uVisor, then it can go into uVisor. I think we must be OK with
     * OS-specifc (at least CMSIS-OS-specific) stuff being in uVisor, because
     * we want all privileged code to be part of the verifiable uVisor binary.
     * XXX TODO Ideally the os_event_observer is located only in flash. We
     * should verify that the os_event_observer is in flash when we register it
     * in RTX. */
    static OsEventObserver os_event_observer;
    memcpy(&os_event_observer, &__uvisor_export_table->os_event_observer, sizeof(os_event_observer));
    os_event_observer.pre_start = uvisor_lib_init_post;

    osRegisterForOsEvents(&os_event_observer);

    return 0;
}
