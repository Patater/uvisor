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
#ifndef __UVISOR_API_RPC_EXPORTS_H__
#define __UVISOR_API_RPC_EXPORTS_H__

#include "api/inc/pool_queue_exports.h"
#include "api/inc/uvisor_semaphore_exports.h"

typedef uint32_t (*TFN_Ptr)(uint32_t, uint32_t, uint32_t, uint32_t);

#define UVISOR_RESULT_SLOT_BITS 10
#define UVISOR_RESULT_SLOT_MASK ((1 << UVISOR_RESULT_SLOT_BITS) - 1)

#define UVISOR_RESULT_COUNTER_MASK (0xFFFFFFFFUL << UVISOR_RESULT_SLOT_BITS)
#define UVISOR_RESULT_COUNTER_INCREMENT (2 << UVISOR_RESULT_SLOT_BITS)

#define UVISOR_RESULT_INVALID_COUNTER (UVISOR_RESULT_COUNTER_MASK)

/* This is the token to wait on for the result of an asynchronous RPC. */
typedef uint32_t uvisor_rpc_result_t;

static inline uvisor_pool_slot_t uvisor_result_slot(uvisor_rpc_result_t result)
{
    return result & UVISOR_RESULT_SLOT_MASK;
}

static inline uint32_t uvisor_result_counter(uvisor_rpc_result_t result)
{
    return result & UVISOR_RESULT_COUNTER_MASK;
}

static inline uvisor_rpc_result_t uvisor_result_build(uint32_t counter, uvisor_pool_slot_t slot)
{
    return (counter & UVISOR_RESULT_COUNTER_MASK) | (slot & UVISOR_RESULT_SLOT_MASK);
}

typedef struct uvisor_rpc_message {
    /* NOTE: These are set by the caller, and read by the callee. */
    uint32_t p0;
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;

    TFN_Ptr function;

    int source_box;
    uint32_t gateway_address;

    /* The semaphore to post to when a result is ready */
    UvisorSemaphore semaphore;

    uint32_t result;

    /* This identifies to uVisor which RPC it should complete. uVisor
     * must verify this information of course, to see if this box is currently
     * being called into and is allowed to complete the RPC. */
    uvisor_rpc_result_t cookie;
} uvisor_rpc_message_t;

/* uvisor_rpc_result_t is just a slot number into a pool of result objects.
 * This is the result object, the real business thing.
 * This is only used for outgoing rpc results. */
typedef struct uvisor_rpc_result_obj
{
    uvisor_rpc_result_t cookie;
    uvisor_rpc_message_t * msg;

    /* The return value from the RPC target function */
    uint32_t value;
} uvisor_rpc_result_obj_t;

typedef struct uvisor_rpc_fn_group {
    /* A pointer to the function group */
    TFN_Ptr const * fn_ptr_array;
    size_t fn_count;
    int ready;

    /* The semaphore to wait on for this function group */
    UvisorSemaphore semaphore;
} uvisor_rpc_fn_group_t;

#define UVISOR_RPC_OUTGOING_MESSAGE_SLOTS (8)

#define UVISOR_RPC_INCOMING_MESSAGE_SLOTS (8)
#define UVISOR_RPC_OUTGOING_RESULT_SLOTS (8)

#define UVISOR_RPC_FN_GROUP_SLOTS (8)

#define UVISOR_RPC_OUTGOING_MESSAGE_TYPE(slots) \
    struct { \
        uvisor_pool_queue_t queue; \
        uvisor_pool_queue_entry_t entries[slots]; \
        uvisor_rpc_message_t messages[slots]; \
    }

/* XXX: For now the incoming is of the same type as outgoing,
 * even though we don't need the semaphore and result memory. */
#define UVISOR_RPC_INCOMING_MESSAGE_TYPE(slots) \
    UVISOR_RPC_OUTGOING_MESSAGE_TYPE(slots)

#define UVISOR_RPC_OUTGOING_RESULT_TYPE(slots) \
    struct { \
        uvisor_pool_queue_t queue; \
        uvisor_pool_queue_entry_t entries[slots]; \
        uvisor_rpc_result_obj_t results[slots]; \
    }

#define UVISOR_RPC_FN_GROUP_TYPE(slots) \
    struct { \
        uvisor_pool_t pool; \
        uvisor_pool_queue_entry_t entries[slots]; \
        uvisor_rpc_fn_group_t fn_groups[slots]; \
    }

typedef UVISOR_RPC_OUTGOING_MESSAGE_TYPE(UVISOR_RPC_OUTGOING_MESSAGE_SLOTS) uvisor_rpc_outgoing_message_queue_t;
typedef UVISOR_RPC_INCOMING_MESSAGE_TYPE(UVISOR_RPC_INCOMING_MESSAGE_SLOTS) uvisor_rpc_incoming_message_queue_t;
typedef UVISOR_RPC_OUTGOING_RESULT_TYPE(UVISOR_RPC_OUTGOING_RESULT_SLOTS) uvisor_rpc_outgoing_result_queue_t;
typedef UVISOR_RPC_FN_GROUP_TYPE(UVISOR_RPC_FN_GROUP_SLOTS) uvisor_rpc_fn_group_pool_t;

#endif
