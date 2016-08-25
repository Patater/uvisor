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
#include "semaphore.h"
#include "api/inc/export_table_exports.h"
#include "api/inc/rpc_gateway_exports.h"
#include "api/inc/pool_queue_exports.h"
#include "api/inc/rpc_exports.h"
#include "api/inc/svc_exports.h"
#include "api/inc/vmpu_exports.h"
#include "context.h"
#include "halt.h"
#include "vmpu.h"

/* By default a maximum of 16 threads are allowed. This can only be overridden
 * by the porting engineer for the current platform. */
#ifndef UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT
#define UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT ((uint32_t) 16)
#endif

/* Per thread we store the pointer to the allocator and the process id that
 * this thread belongs to. */
typedef struct {
    void * allocator;
    int process_id;
} UvisorThreadContext;

static UvisorThreadContext thread[UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT];


static int thread_ctx_valid(UvisorThreadContext * context)
{
    /* Check if context pointer points into the array. */
    if ((uint32_t) context < (uint32_t) &thread ||
        ((uint32_t) &thread + sizeof(thread)) <= (uint32_t) context) {
        return 0;
    }
    /* Check if the context is aligned exactly to a context. */
    return (((uint32_t) context - (uint32_t) thread) % sizeof(UvisorThreadContext)) == 0;
}

static void * thread_create(int id, void * c)
{
    (void) id;
    UvisorThreadContext * context = c;
    const UvisorBoxIndex * const index =
            (UvisorBoxIndex * const) *(__uvisor_config.uvisor_box_context);
    /* Search for a free slot in the tasks meta data. */
    uint32_t ii = 0;
    for (; ii < UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT; ii++) {
        if (thread[ii].allocator == NULL) {
            break;
        }
    }
    if (ii < UVISOR_EXPORT_TABLE_THREADS_MAX_COUNT) {
        /* Remember the process id for this thread. */
        thread[ii].process_id = g_active_box;
        /* Fall back to the process heap if ctx is NULL. */
        thread[ii].allocator = context ? context : index->box_heap;
        return &thread[ii];
    }
    return context;
}

static void thread_destroy(void * c)
{
    UvisorThreadContext * context = c;
    if (context == NULL) {
        return;
    }

    /* Only if TID is valid and destruction status is zero. */
    if (thread_ctx_valid(context) && context->allocator && (context->process_id == g_active_box)) {
        /* Release this slot. */
        context->allocator = NULL;
    } else {
        /* FIXME: This should be a debug only assertion, not present in release
         * builds, to prevent a malicious box from taking down the entire
         * system by fiddling with one of its thread contexts or destroying
         * another box's thread. */
        HALT_ERROR(SANITY_CHECK_FAILED,
            "thread context (%08x) is invalid!\n",
            context);
    }
}

/* Wake up all the potential handlers for this RPC target. Return number of
 * handlers posted to. */
static int wake_up_handlers_for_target(const TFN_Ptr function, int box_id)
{
    int num_posted = 0;

    UvisorBoxIndex * index = (UvisorBoxIndex *) g_context_current_states[box_id].bss;
    uvisor_pool_t * fn_group_pool = &index->rpc_fn_group_pool->pool;
    uvisor_rpc_fn_group_t * fn_group_array = (uvisor_rpc_fn_group_t *) fn_group_pool->array;

    /* Wake up all known waiters for this function. Search for the function in
     * all known function groups. We have to search through all function groups
     * (not just those currently waiting for messages) because we want the RTOS
     * to be able to pick the highest priority waiter to schedule to run. Some
     * waiters will wake up and find they have nothing to do if a higher
     * priority waiter already took care of handling the incoming RPC. */
    uvisor_pool_slot_t i;
    for (i = 0; i < fn_group_pool->num; i++) {
        /* If the entry in the pool is allocated: */
        if (fn_group_pool->management_array[i].dequeued.state != UVISOR_POOL_SLOT_IS_FREE) {
            /* Look for the function in this function group. */
            uvisor_rpc_fn_group_t * fn_group = &fn_group_array[i];

            /* It is possible that the slot has been allocated for the
             * fn_group, but not yet initialized. If the slot is not ready,
             * ignore. */
            if (fn_group->state != UVISOR_RPC_FN_GROUP_STATE_READY) {
                continue;
            }

            TFN_Ptr const * fn_ptr_array = fn_group->fn_ptr_array;
            uvisor_pool_slot_t j;

            for (j = 0; j < fn_group->fn_count; j++) {
                /* If function is found: */
                if (fn_ptr_array[j] == function) {
                    /* Wake up the waiter. */
                    semaphore_post(&fn_group->semaphore);
                    ++num_posted;
                }
            }
        }
    }

    return num_posted;
}

static int callee_box_id(const TRPCGateway * gateway)
{
    return (uint32_t *)gateway->box_ptr - __uvisor_config.cfgtbl_ptr_start;
}

static int put_it_back(uvisor_pool_queue_t * queue, uvisor_pool_slot_t slot)
{
    int status;
    status = uvisor_pool_queue_try_enqueue(queue, slot);
    if (status) {
        /* XXX It is bad to take down the entire system. It is also bad
         * to lose messages due to not being able to put them back in
         * the queue. However, if we could dequeue the slot
         * we should have no trouble enqueuing the slot here. */
        /* XXX TODO Make this a debug-only halt */
        HALT_ERROR(SANITY_CHECK_FAILED, "We were able to dequeue an RPC message, but weren't able to put the message back.");
    }

    /* Note that we don't have to modify the message in the queue, since it'll
     * still be valid. Nobody else will have run at the same time that could
     * have messed it up. */

     return status;
}

/* Return true iff gateway is valid. */
static int is_valid_rpc_gateway(const TRPCGateway * const gateway)
{
    /* as sanity (not security) check, box_ptr needs to point within the box
     * config table to a box id less than g_vmpu_box_count. */
    //.box_ptr  = (uint32_t) &box_name ## _cfg_ptr

    /* Gateway needs to be entirely in flash. */
    /* Gateway needs to have good magic. */
    /* Gateway needs good jmp instruction. */
    /* Gateway needs to point to functions in flash (caller and target) */

    /* XXX Not secure */
    return 1;
}

/* Return true if and only if the queue is entirely within the box specified by
 * the provided box_id. */
static int is_valid_queue(uvisor_pool_queue_t * queue, int box_id)
{
    uint32_t bss_start = g_context_current_states[box_id].bss;
    uint32_t bss_end = bss_start + g_context_current_states[box_id].bss_size;

    uint32_t queue_start = (uint32_t) queue;
    uint32_t queue_end = queue_start + sizeof(*queue);
    int queue_is_valid = queue_start >= bss_start && queue_end <= bss_end;

    uint32_t pool_start = (uint32_t) queue->pool;
    uint32_t pool_end = pool_start + sizeof(*queue->pool);
    int pool_is_valid = pool_start >= bss_start && pool_end <= bss_end;

    /* XXX A malicious box could lie about its own pool size to make it pretend to
     * fit within box bss. */
    uint32_t man_array_start = (uint32_t) queue->pool->management_array;
    uint32_t man_array_end = man_array_start + sizeof(*queue->pool->management_array) * queue->pool->num;
    int man_array_is_valid = man_array_start >= bss_start && man_array_end <= bss_end;

    /* XXX A malicious box could lie about its own pool size to make it pretend to
     * fit within box bss. */
    uint32_t array_start = (uint32_t) queue->pool->array;
    uint32_t array_end = array_start + queue->pool->stride * queue->pool->num;
    int array_is_valid = array_start >= bss_start && array_end <= bss_end;

    return queue_is_valid && pool_is_valid && man_array_is_valid && array_is_valid;
}

static void drain_message_queue(void)
{
    /* XXX This implementation is dumb and simple and slow and not secure. */

    UvisorBoxIndex * caller_index = (UvisorBoxIndex *) *__uvisor_config.uvisor_box_context;
    uvisor_pool_queue_t * caller_queue = &caller_index->rpc_outgoing_message_queue->queue;
    uvisor_rpc_message_t * caller_array = (uvisor_rpc_message_t *) caller_queue->pool->array;
    int caller_box = g_active_box;
    int first_slot = -1;

    /* Verify that the caller queue is entirely in caller box BSS. We check the
     * entire queue instead of just the message we are interested in, because
     * we want to validate the queue before we attempt any operations on it,
     * like dequeing. */
    if (!is_valid_queue(caller_queue, caller_box))
    {
        /* The caller queue is messed up. This shouldn't happen in a
         * non-malicious system. XXX Think what we want to do in this case. */
        HALT_ERROR(SANITY_CHECK_FAILED, "Caller's outgoing queue is not valid");
        return;
    }

    /* For each message in the queue: */
    do {
        uvisor_pool_slot_t caller_slot;

        /* NOTE: We only dequeue the message from the queue. We don't free
         * the message from the pool. The caller will free the message from the
         * pool after finish waiting for the RPC to finish. */
        caller_slot = uvisor_pool_queue_try_dequeue_first(caller_queue);
        if (caller_slot >= caller_queue->pool->num) {
            /* The queue is empty or busy. */
            break;
        }

        /* If we have seen this slot before, stop processing the queue. */
        if (first_slot == -1) {
            first_slot = caller_slot;
        } else if (caller_slot == first_slot) {
            put_it_back(caller_queue, caller_slot);

            /* Stop looping, because the system needs to continue running so
             * the callee messages can get processed to free up more room.
             * */
            break;
        }

        /* We would like to finish processing all messages in the queue, even
         * if one can't be delivered now. We currently just stop when we can't
         * deliver one message and never attempt the rest. */

        uvisor_rpc_message_t * caller_msg = &caller_array[caller_slot];

        /* Validate the gateway */
        const TRPCGateway * const gateway = caller_msg->gateway;
        if (!is_valid_rpc_gateway(gateway)) {
            /* Don't put it back. Move on to next items. */
            /* When will it be freed? Maybe we have to free it here. */
            /* This should never happen on a non-malicious system. */
            /* XXX TODO Make this a debug-only halt */
            HALT_ERROR(SANITY_CHECK_FAILED, "RPC gateway is not valid");
            continue;
        }

        /* Look up the callee box. */
        const int callee_box = callee_box_id(gateway);
        if (callee_box <= 0) {
            put_it_back(caller_queue, caller_slot);
            continue;
        }

        UvisorBoxIndex * callee_index = (UvisorBoxIndex *) g_context_current_states[callee_box].bss;
        uvisor_pool_queue_t * callee_queue = &callee_index->rpc_incoming_message_queue->todo_queue;
        uvisor_rpc_message_t * callee_array = (uvisor_rpc_message_t *) callee_queue->pool->array;

        /* Verify that the callee queue is entirely in callee box BSS. We check the
         * entire queue instead of just the message we are interested in, because
         * we want to validate the queue before we attempt any operations on it,
         * like allocating. */
        if (!is_valid_queue(callee_queue, callee_box))
        {
            /* The caller queue is messed up. This shouldn't happen in a
             * non-malicious system. XXX Think what we want to do in this case. */
            HALT_ERROR(SANITY_CHECK_FAILED, "Callee's incoming queue is not valid");
            return;
        }

        /* Place the message into the callee box queue. */
        uvisor_pool_slot_t callee_slot = uvisor_pool_queue_try_allocate(callee_queue);

        /* If the queue is not busy and there is space in the callee queue: */
        if (callee_slot < callee_queue->pool->num)
        {
            int status;
            uvisor_rpc_message_t * callee_msg = &callee_array[callee_slot];

            /* Deliver the message. */
            callee_msg->p0 = caller_msg->p0;
            callee_msg->p1 = caller_msg->p1;
            callee_msg->p2 = caller_msg->p2;
            callee_msg->p3 = caller_msg->p3;
            callee_msg->gateway = caller_msg->gateway;
            /* Set the ID of the calling box in the message. */
            callee_msg->other_box_id = caller_box;
            callee_msg->cookie = caller_msg->cookie;
            callee_msg->state = UVISOR_RPC_MESSAGE_STATE_SENT;

            caller_msg->other_box_id = callee_box;
            caller_msg->state = UVISOR_RPC_MESSAGE_STATE_SENT;

            /* Enqueue the message */
            status = uvisor_pool_queue_try_enqueue(callee_queue, callee_slot);
            /* We should always be able to enqueue, since we were able to
             * allocate the slot. Nobody else should have been able to run and
             * take the spin lock. */
            if (status) {
                /* XXX It is bad to take down the entire system. It is also bad
                 * to keep the allocated slot around. However, if we couldn't
                 * enqueue the slot, we'll have a hard time freeing it, since
                 * that requires the same lock. */
                HALT_ERROR(SANITY_CHECK_FAILED, "We were able to get the callee RPC slot allocated, but couldn't enqueue the message.");
            }

            /* Poke anybody waiting on calls to this target function. If nobody
             * is waiting, the item will remain in the incoming queue. The
             * first time a rpc_fncall_waitfor is called for a function group,
             * rpc_fncall_waitfor will check to see if there are any messages
             * it can handle from before the function group existed. */
            wake_up_handlers_for_target((TFN_Ptr)gateway->target, callee_box);
        }

        /* If there was no room in the callee queue: */
        if (callee_slot >= callee_queue->pool->num)
        {
            /* Put the message back into the caller queue. This applies
             * backpressure on the caller when the callee is too busy. Note
             * that no data needs to be copied; only the caller queue's
             * management array is modified. */
            put_it_back(caller_queue, caller_slot);
        }
    } while (1);
}

static void drain_result_queue(void)
{
    /* XXX This implementation is dumb and simple and slow and not secure. */

    UvisorBoxIndex * callee_index = (UvisorBoxIndex *) *__uvisor_config.uvisor_box_context;
    uvisor_pool_queue_t * callee_queue = &callee_index->rpc_incoming_message_queue->done_queue;
    uvisor_rpc_message_t * callee_array = (uvisor_rpc_message_t *) callee_queue->pool->array;

    int callee_box = g_active_box;

    /* For each message in the queue: */
    do {
        uvisor_pool_slot_t callee_slot;

        /* Dequeue the first result message from the queue. */
        callee_slot = uvisor_pool_queue_try_dequeue_first(callee_queue);
        if (callee_slot >= callee_queue->pool->num) {
            /* The queue is empty or busy. */
            break;
        }

        /* XXX Check the memory locations are proper. */

        uvisor_rpc_message_t * callee_msg = &callee_array[callee_slot];

        /* Look up the origin message. This should have been remembered
         * by uVisor when it did the initial delivery. */
        uvisor_pool_slot_t caller_slot = uvisor_result_slot(callee_msg->cookie);


        /* Based on the origin message, look up the box to return the result to
         * (caller box). */
        const int caller_box = callee_msg->other_box_id;

        UvisorBoxIndex * caller_index = (UvisorBoxIndex *) g_context_current_states[caller_box].bss;
        uvisor_pool_queue_t * caller_queue = &caller_index->rpc_outgoing_message_queue->queue;
        uvisor_rpc_message_t * caller_array = (uvisor_rpc_message_t *) caller_queue->pool->array;
        uvisor_rpc_message_t * caller_msg = &caller_array[caller_slot];

        /* FIXME Verify that the callee box message is in callee box bss.
         * Verify that the destination box message is in destination box bss.
         * */

        /* Verify that the caller box is waiting for the callee box to complete
         * the RPC in this slot. */

        /* Other box ID must be same. */
        if (caller_msg->other_box_id != callee_box) {
            /* This shouldn't happen in a non-malicious system. */

            /* XXX Debug-only halt here. */
            HALT_ERROR(SANITY_CHECK_FAILED, "The caller isn't waiting for this box to complete it.");
            continue;
        }

        /* The caller must be waiting for a box to complete this slot. */
        if (caller_msg->state != UVISOR_RPC_MESSAGE_STATE_SENT)
        {
            /* This shouldn't happen in a non-malicious system. */

            /* XXX Debug-only halt here. */
            HALT_ERROR(SANITY_CHECK_FAILED, "The caller isn't waiting for any box to complete it.");
            continue;
        }

        /* The cookie must be same. */
        if (caller_msg->cookie != callee_msg->cookie) {
            /* This shouldn't happen in a non-malicious system. */

            /* XXX Debug-only halt here. */
            HALT_ERROR(SANITY_CHECK_FAILED, "The cookies didn't match.");

            continue;
        }

        /* Copy the result to the message in the caller box outgoing message
         * queue. */
        caller_msg->result = callee_msg->result;
        callee_msg->state = UVISOR_RPC_MESSAGE_STATE_IDLE;
        caller_msg->state = UVISOR_RPC_MESSAGE_STATE_DONE;

        /* Now that we've copied the result, we can free the message from the
         * callee queue. The callee (the one sending result messages) doesn't
         * care about the message after they post it to their outgoing result
         * queue. */
        callee_slot = uvisor_pool_queue_try_free(callee_queue, callee_slot);
        if (callee_slot >= callee_queue->pool->num) {
            /* The queue is empty or busy. This should never happen. */
            /* XXX It is bad to take down the entire system. It is also bad to
             * never free slots in the outgoing result queue. However, if we
             * could dequeue the slot we should have no trouble freeing the
             * slot here. */
            HALT_ERROR(SANITY_CHECK_FAILED, "We were able to dequeue a result message, but weren't able to free the result message.");
            break;
        }

        /* Post to the result semaphore, TODO ignoring errors. */
        int status;
        status = semaphore_post(&caller_msg->semaphore);
        if (status) {
            /* XXX The semaphore was bad. We shouldn't really bring down the entire
             * system if one box messes up its own semaphore. In a
             * non-malicious system, this should never happen. */
            /* XXX TODO Make this a debug-only halt */
            HALT_ERROR(SANITY_CHECK_FAILED, "We couldn't semaphore.");
        }
    } while (1);
}

static void drain_outgoing_rpc_queues(void)
{
    drain_message_queue();
    drain_result_queue();
}

static void thread_switch(void * c)
{
    UvisorThreadContext * context = c;
    UvisorBoxIndex * index;

    /* Drain any outgoing RPC queues */
    drain_outgoing_rpc_queues();

    if (context == NULL) {
        return;
    }

    /* Only if TID is valid and the slot is used */
    if (!thread_ctx_valid(context) || context->allocator == NULL) {
        HALT_ERROR(SANITY_CHECK_FAILED,
            "thread context (%08x) is invalid!\n",
            context);
        return;
    }
    /* If the thread is inside another process, switch into it. */
    if (context->process_id != g_active_box) {
        context_switch_in(CONTEXT_SWITCH_UNBOUND_THREAD, context->process_id, 0, 0);
    }
    /* Copy the thread allocator into the (new) box index. */
    /* Note: The value in index is updated by context_switch_in, or is already
     *       the correct one if no switch needs to occur. */
    index = (UvisorBoxIndex *) *(__uvisor_config.uvisor_box_context);
    if (context->allocator) {
        /* If the active_heap is NULL, then the process heap needs to be
         * initialized yet. The initializer sets the active heap itself. */
        if (index->active_heap) {
            index->active_heap = context->allocator;
        }
    }
}

static void boxes_init(void)
{
    /* Tell uVisor to call the uVisor lib box_init function for each box with
     * each box's uVisor lib config. */

    /* This must be called from unprivileged mode in order for the recursive
     * gateway chaining to work properly. */
    UVISOR_SVC(UVISOR_SVC_ID_BOX_INIT_FIRST, "");
}

/* This table must be located at the end of the uVisor binary so that this
 * table can be exported correctly. Placing this table into the .export_table
 * section locates this table at the end of the uVisor binary. */
__attribute__((section(".export_table")))
const TUvisorExportTable __uvisor_export_table = {
    .magic = UVISOR_EXPORT_MAGIC,
    .version = UVISOR_EXPORT_VERSION,
    .os_event_observer = {
        .version = 0,
        .pre_start = boxes_init,
        .thread_create = thread_create,
        .thread_destroy = thread_destroy,
        .thread_switch = thread_switch,
    },
    .pool_queue = {
        .pool_init = uvisor_pool_init,
        .pool_queue_init = uvisor_pool_queue_init,
        .pool_allocate = uvisor_pool_allocate,
        .pool_queue_enqueue = uvisor_pool_queue_enqueue,
        .pool_free = uvisor_pool_free,
        .pool_queue_dequeue = uvisor_pool_queue_dequeue,
        .pool_queue_dequeue_first = uvisor_pool_queue_dequeue_first,
        .pool_queue_find_first = uvisor_pool_queue_find_first,
    },
    .size = sizeof(TUvisorExportTable)
};
