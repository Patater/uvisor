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
#include "uvisor-lib/uvisor-lib.h"
#include "mbed_interface.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdlib.h>

/* This function is called by uVisor in unprivileged mode to create box main
 * threads. */
void __uvisor_lib_config_handler(uint32_t lib_config)
{
    osThreadId thread_id;
    osThreadDef_t * thread_def = (osThreadDef_t *)lib_config;

    /* Note that the box main thread stack is separate from the box stack. This
     * is because the thread must be created to use a different stack than the
     * stack osCreateThread() is called from, as context information is saved
     * to the thread stack by the call to osCreateThread(). */
    /* Allocate memory for the main thread from the process heap (which is
     * private to the process). This memory is never freed, even if the box's
     * main thread exits. */
    thread_def->stack_pointer = malloc_p(thread_def->stacksize);

    if (thread_def->stack_pointer == NULL) {
        /* No process heap memory available */
        mbed_die();
    }

    thread_id = osThreadCreate(thread_def, NULL);

    if (thread_id == NULL) {
        /* Failed to create thread */
        mbed_die();
    }
}
