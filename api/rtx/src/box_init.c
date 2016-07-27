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
#include "api/inc/pool_queue_exports.h"
#include "api/inc/rpc_exports.h"
#include "api/inc/uvisor_semaphore.h"
#include "mbed_interface.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <string.h>

extern RtxBoxIndex * const __uvisor_ps;

void __uvisor_initialize_rpc_queues(void)
{
    UvisorBoxIndex * const index = &__uvisor_ps->index;

    uvisor_pool_slot_t i;

    uvisor_rpc_outgoing_message_queue_t * rpc_outgoing_msg_queue = (uvisor_rpc_outgoing_message_queue_t *) index->rpc_outgoing_message_queue;

    /* Initialize the outgoing RPC message queue. */
    if (uvisor_pool_queue_init(&rpc_outgoing_msg_queue->queue,
                               rpc_outgoing_msg_queue->messages,
                               sizeof(*rpc_outgoing_msg_queue->messages),
                               UVISOR_RPC_OUTGOING_MESSAGE_SLOTS,
                               UVISOR_POOL_QUEUE_BLOCKING)) {
        uvisor_error(USER_NOT_ALLOWED);
    }

    /* Initialize all the result semaphores. */
    for (i = 0; i < UVISOR_RPC_OUTGOING_MESSAGE_SLOTS; i++) {
        UvisorSemaphore * semaphore = &rpc_outgoing_msg_queue->messages[i].semaphore;
        if (__uvisor_semaphore_init(semaphore, 1)) {
            uvisor_error(USER_NOT_ALLOWED);
        }

        /* Semaphores are created with their value initialized to count. We
         * want the semaphore to start at zero. Decrement the semaphore, so it
         * starts with a value of zero. This will allow the first pend to
         * block. */
        if (__uvisor_semaphore_pend(semaphore, 0)) {
            uvisor_error(USER_NOT_ALLOWED);
        }
    }
}

/* This function is called by uVisor in unprivileged mode. On this OS, we
 * create box main threads for the box. */
void __uvisor_lib_box_init(void * lib_config)
{
    osThreadId thread_id;
    osThreadDef_t * flash_thread_def = lib_config;
    osThreadDef_t thread_def;

    __uvisor_initialize_rpc_queues();

    /* Copy thread definition from flash to RAM. The thread definition is most
     * likely in flash, so we need to copy it to box-local RAM before we can
     * modify it. */
    memcpy(&thread_def, flash_thread_def, sizeof(thread_def));

    /* Note that the box main thread stack is separate from the box stack. This
     * is because the thread must be created to use a different stack than the
     * stack osCreateThread() is called from, as context information is saved
     * to the thread stack by the call to osCreateThread(). */
    /* Allocate memory for the main thread from the process heap (which is
     * private to the process). This memory is never freed, even if the box's
     * main thread exits. */
    thread_def.stack_pointer = malloc_p(thread_def.stacksize);

    if (thread_def.stack_pointer == NULL) {
        /* No process heap memory available */
        mbed_die();
    }

    thread_id = osThreadCreate(&thread_def, NULL);

    if (thread_id == NULL) {
        /* Failed to create thread */
        mbed_die();
    }
}
