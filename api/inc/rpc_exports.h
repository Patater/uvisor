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

typedef struct uvisor_rpc_message {
    /* NOTE: These are set by the caller, and read by the callee. */
    uint32_t p0;
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;

    TFN_Ptr function;

    uint32_t source_box;
    uint32_t gateway_address;

    /* The semaphore to post to when a result is ready */
    uvisor_semaphore_t semaphore;

    uint32_t result;
} uvisor_rpc_message_t;

#define UVISOR_RPC_OUTGOING_MESSAGE_SLOTS (4)

#define UVISOR_RPC_OUTGOING_MESSAGE_TYPE(slots) \
    struct { \
        uvisor_pool_queue_t queue; \
        uvisor_pool_queue_entry_t entries[slots]; \
        uvisor_rpc_message_t messages[slots]; \
    }

typedef UVISOR_RPC_OUTGOING_MESSAGE_TYPE(UVISOR_RPC_OUTGOING_MESSAGE_SLOTS) uvisor_rpc_outgoing_message_queue_t;

#endif
