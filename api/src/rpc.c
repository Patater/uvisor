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
#include "api/inc/rpc.h"
#include "api/inc/rpc_exports.h"
#include "api/inc/rpc_gateway.h"
#include "api/inc/vmpu_exports.h"
#include "api/inc/pool_queue_exports.h"
#include "api/inc/error.h"
#include "api/inc/uvisor_semaphore.h"
#include <string.h>

extern UvisorBoxIndex * __uvisor_ps;


static uvisor_pool_queue_t * outgoing_message_queue(void)
{
    return __uvisor_ps->rpc_outgoing_message_queue;
}

static uvisor_rpc_message_t * outgoing_message_array(void)
{
    return (uvisor_rpc_message_t *) outgoing_message_queue()->pool.array;
}

/* Place a message into the outgoing queue. `timeout_ms` is how long to wait
 * for a slot in the outgoing queue before giving up. `msg_slot` is set to the
 * slot of the message that was allocated. Returns non-zero on failure. */
static int send_outgoing_rpc(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn, uint32_t timeout_ms,
                             uvisor_rpc_result_t * cookie)
{
    static uint32_t shared_result_counter = 0;
    uint32_t counter;
    uvisor_rpc_message_t * msg;
    uvisor_pool_slot_t slot;

    /* Claim a slot in the outgoing RPC queue. */
    slot = uvisor_pool_queue_allocate(outgoing_message_queue(), timeout_ms);
    if (slot >= outgoing_message_queue()->pool.num) {
        /* No slots available in incoming queue. We asked for a free slot but
         * didn't get one. */
        return -1;
    }

    /* Atomically increment the counter. */
    counter = __sync_add_and_fetch(&shared_result_counter, UVISOR_RESULT_COUNTER_INCREMENT);

    /* Populate the message */
    msg = &outgoing_message_array()[slot];
    msg->p0 = p0;
    msg->p1 = p1;
    msg->p2 = p2;
    msg->p3 = p3;
    msg->function = fn;
    msg->cookie = uvisor_result_build(counter, slot);

    /* Put the slot into the queue. */
    uvisor_pool_queue_enqueue(outgoing_message_queue(), slot);

    /* Notify the caller of this function of the slot that was allocated for
     * sending this RPC message. */
    *cookie = msg->cookie;

    return 0;
}

/* Wait up to `timeout_ms` for the RPC in `msg_slot` to complete. Return 0 if
 * the RPC completed. */
static int wait_for_rpc_result(uvisor_pool_slot_t msg_slot, uint32_t timeout_ms)
{
    return __uvisor_semaphore_pend(&outgoing_message_array()[msg_slot].semaphore, timeout_ms);
}

static void free_outgoing_msg(uvisor_pool_slot_t msg_slot)
{
    /* We are done with the outgoing RPC message now. uVisor dequeued the RPC
     * message on our behalf when uVisor sent the message to the destination,
     * so we can just free without dequeueing first. */
    if (outgoing_message_queue()->pool.management_array[msg_slot].dequeued.state != UVISOR_POOL_SLOT_IS_DEQUEUED) {
        uvisor_error(USER_NOT_ALLOWED);
    }
    uvisor_pool_queue_free(outgoing_message_queue(), msg_slot);
}

uint32_t rpc_fncall_sync(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn)
{
    int status;
    uint32_t result_value;
    uvisor_rpc_result_t cookie;
    uvisor_pool_slot_t msg_slot;

    /* The synchronous RPC calling function has no way to fail, so it must
     * infinitely retry operations until the RPC succeeds. */

    /* Loop until sending the RPC message succeeds. */
    do {
        /* Because this is the sync function, we use wait forever to wait for an
         * available message slot. */
        status = send_outgoing_rpc(p0, p1, p2, p3, fn, UVISOR_WAIT_FOREVER, &cookie);
    } while (status);
    msg_slot = uvisor_result_slot(cookie);

    /* Loop until sending the RPC message succeeds. */
    do {
        /* We also (because this is the sync function) wait forever for a result. */
        status = wait_for_rpc_result(msg_slot, UVISOR_WAIT_FOREVER);
    } while (status);

    /* This message result is valid now, because we woke up with a non-fatal
     * status. */
    result_value = outgoing_message_array()[msg_slot].result;

    free_outgoing_msg(msg_slot);

    return result_value;
}

/* Start an asynchronous RPC. After this call successfully completes, the
 * caller can, at any time in any thread, wait on the result object to get the
 * result of the call. */
uvisor_rpc_result_t rpc_fncall_async(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn)
{
    int status;
    uvisor_rpc_result_t cookie;

    /* Don't wait any length of time for an outgoing message slot. If there is
     * no slot available, return immediately with a non-zero status. */
    status = send_outgoing_rpc(p0, p1, p2, p3, fn, 0, &cookie);
    if (status) {
        return status;
    }

    return cookie;
}

int rpc_fncall_wait(uvisor_rpc_result_t result, uint32_t timeout_ms, uint32_t * ret)
{
    int status;
    uvisor_pool_slot_t const msg_slot = uvisor_result_slot(result);
    uvisor_rpc_result_t const invalid = uvisor_result_build(UVISOR_RESULT_INVALID_COUNTER, msg_slot);

    /* If the cookie is invalid, this message is already being waited for.
     * Otherwise this is the first wait and we can proceed. */
    uvisor_rpc_result_t cookie = __sync_val_compare_and_swap(
            &outgoing_message_array()[msg_slot].cookie,
            result,
            invalid);
    if (cookie != result) {
        return -1;
    }

    status = wait_for_rpc_result(msg_slot, timeout_ms);

    if (status) {
        return status;
    }

    /* The message result is valid now, because we woke up with a non-fatal
     * status. */
    *ret = outgoing_message_array()[msg_slot].result;

    free_outgoing_msg(msg_slot);

    return 0;
}