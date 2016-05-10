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
#include "api/inc/privcall.h" /* XXX Not sure if this should be renamed to
privcall_exports.h or if it should get added to uvisor-lib.h or
uvisor-exports.h or whatnot */

/* XXX This only works when thread IDs are compact in representation and are
 * not a 32-bit hash or pointer. */
#define UVISOR_MAX_THREADS 256
static uint8_t thread_to_box_map[UVISOR_MAX_THREADS];

static void privcall_thread_alloc(uint32_t thread_id)
{
    DPRINTF("PRIVCALL alloc thread %d in box %d\r\n", thread_id, g_active_box);

    /* Allocate resources in uVisor for thread. */
    /* Maybe can use our Tier-1 allocator? It's per-process and would prevent a
     * rougue process from stealing the ability to create more threads from
     * other processes when memory runs out. For now, we only allow the maximum
     * number of RTX threads, which happens to be 16 for now. */

    if (thread_id >= UVISOR_MAX_THREADS) {
        DPRINTF("\tthread id %d too big\r\n", thread_id);
        return;
    }

    /* Record which box_id this thread is for, so we can know when to change
     * the MPU context. */
    thread_to_box_map[thread_id] = g_active_box;
}

static void privcall_thread_free(uint32_t thread_id)
{
    DPRINTF("PRIVCALL free thread %d in box %d\r\n", thread_id, g_active_box);

    if (thread_id >= UVISOR_MAX_THREADS) {
        DPRINTF("\tthread id %d too big\r\n", thread_id);
        return;
    }

    /* Free resources in uVisor for thread. */
}

/* XXX TODO write this function */
void stupid_cx_switch(uint8_t dst_box, uint32_t thread_id)
{
    vmpu_switch(g_active_box, dst_box);
}

static void privcall_thread_switch(uint32_t thread_id)
{
    /* Switch to resources in uVisor for thread. */

    /* There is one of these for every thread. There is also one of these per
     * box, for handling interrupts. */

    DPRINTF("PRIVCALL switching to thread %d from box %d\r\n", thread_id, g_active_box);

    if (thread_id >= UVISOR_MAX_THREADS) {
        DPRINTF("\tthread id %d too big\r\n", thread_id);
        return;
    }

    uint8_t dst_box = thread_to_box_map[thread_id];

    DPRINTF("\tthread %d is in box %d\r\n", thread_id, dst_box);

    if (g_active_box != dst_box)
    {
        /* Switch to next thread context and next MPU context. */
        stupid_cx_switch(dst_box, thread_id);
    }

    /* Switch to next thread context. */
    g_active_tid = thread_id;
}

const struct uvisor_privcall_table uvisor_privcall = {
    .version = UVISOR_PRIVCALL_VERSION,
    .thread_alloc = privcall_thread_alloc,
    .thread_free = privcall_thread_free,
    .thread_switch = privcall_thread_switch,
};
