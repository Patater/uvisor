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
#ifndef __UVISOR_API_RPC_H__
#define __UVISOR_API_RPC_H__

#include "cmsis_os.h"

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(*x))

/* XXX TODO Add magic cookie to make this some verifiable list of function
 * targets in flash. All functions in this list must belong to the box that
 * lists them. */
#define UVISOR_BOX_RPC_DECL(box_name, max_num_outstanding_inbound_rpc, ...) \
    const TFN_Ptr box_name##_fn_array[] = { \
        __VA_ARGS__, \
    }; \
    osMailQDef(box_name##_rpc_receive_q, max_num_outstanding_inbound_rpc, uvisor_rpc_message_t); \
    osMailQId(box_name##_rpc_receive_q_id);

#define UVISOR_BOX_RPC_HANDLE(box_name, timeout) \
    rpc_fncall_waitfor(box_name##_rpc_receive_q_id, timeout);

#define UVISOR_BOX_RPC_CALL(box_name, fn_name, p0, p1, p2, p3) \
    return rpc_fncall(box_name##_rpc_receive_q_id, fn_name, p0, p1, p2, p3);

#define UVISOR_BOX_RPC_CALL_ASYNC(box_name, callback, timeout, fn_name, p0, p1, p2, p3) \
    return rpc_fncall_async(box_name##_rpc_receive_q_id, callback, timeout, fn_name, p0, p1, p2, p3);

#define UVISOR_BOX_RPC_INIT(box_name) \
    box_name##_rpc_receive_q_id = osMailCreate(osMailQ(box_name##_rpc_receive_q), NULL);

typedef int (*TFN_Ptr)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef int (*TFN_RPC_Callback)(int);

/* Private for use only by uVisor lib, but the size of this type must be known
 * publicly for the UVISOR_BOX_RPC_DECL macro to work. */
typedef struct {
    /* XXX TODO Think about what memory is accessible when and by whom. uVisor
     * has to manage moving memory around between contexts. We want to minimize
     * SVCs to uVisor, but we probably need one SVC to initiate the call (send
     * parameters to callee) and one to finish it (send back return code to
     * caller), and then there are also a bunch of RTOS SVCs. */

    /* XXX These are set by the caller, read by the callee */
    TFN_Ptr fn;
    uint32_t p0;
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;

    /* The caller reads from this this queue. The callee writes to this queue.
     * */
    osMessageQId result_q_id;
} uvisor_rpc_message_t;

typedef struct {
    /* This is the queue to wait on for a result. */
    osMessageQDef_t *q; /* We have to include the queue def so that we
                           can delete it when the result queue is no longer
                           needed. */
    osMessageQId q_id;
} uvisor_rpc_result_t;

UVISOR_EXTERN int rpc_fncall(osMailQId dest_mail_q_id,
                             const TFN_Ptr fn, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);

/* Return a message queue id that the caller can wait on to get a result.
 * XXX This is pure mechanism: the caller can choose to make a thread to wait
 * on result or use any thread they want whenever they feel like. We, as
 * implementers of RPC, don't make the decision for the caller. The decision is
 * best made where all the information is available to make the decision. If we
 * need to wrap that with an abstraction to make the user not have to think,
 * then we'll provide a separate wrapper to do so, but goal #1 is a stable and
 * functional API. (An API with too much policy in it is bound to be unstable,
 * as users request different policies.) */
/* Notice that we don't even need to specify any sort of timeout stuff here.
 * All that policy is decided by the user. */
UVISOR_EXTERN uvisor_rpc_result_t rpc_fncall_async(osMailQId dest_mail_q_id,
                                                   const TFN_Ptr fn, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);

UVISOR_EXTERN int rpc_fncall_waitfor(osMailQId mail_q_id, uint32_t timeout_ms);

/* XXX TODO Re-word this.
 * Wait for the result of a previously started asynchronous rpc. After this
 * call, ret will contain the return value of the rpc. The return value of this
 * function may indicate that there was a timeout with non-zero. */
UVISOR_EXTERN int rpc_fncall_wait(uvisor_rpc_result_t *result, uint32_t timeout_ms, int *ret);

#endif
