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
#include "api/inc/pool_queue_exports.h"
#include <string.h>

int uvisor_pool_init(uvisor_pool_t * pool, void * array, size_t stride, size_t num, int blocking)
{
    uvisor_pool_slot_t i;
    pool->array = array;
    pool->stride = stride;
    pool->num = num;
    pool->blocking = blocking;
    pool->num_allocated = 0;
    pool->first_free = 0;

    for (i = 0; i < num; i++) {
        pool->management_array[i].dequeued.next = i + 1;
        pool->management_array[i].dequeued.state = UVISOR_POOL_SLOT_IS_FREE;
    }
    pool->management_array[num - 1].dequeued.next = UVISOR_POOL_SLOT_INVALID;

    uvisor_mutex_init(&pool->mutex);

    if (pool->blocking) {
        uvisor_semaphore_init(&pool->semaphore, num);
    }

    return 0;
}

int uvisor_pool_queue_init(uvisor_pool_queue_t * pool_queue, void * array, size_t stride, size_t num, int blocking)
{
    uvisor_pool_init(&pool_queue->pool, array, stride, num, blocking);
    pool_queue->head = UVISOR_POOL_SLOT_INVALID;
    pool_queue->tail = UVISOR_POOL_SLOT_INVALID;

    return 0;
}

/* Remove an element from the front of the free list. */
static uvisor_pool_slot_t pool_alloc(uvisor_pool_t * pool)
{
    uvisor_pool_slot_t fresh = pool->first_free;
    if (pool->first_free != UVISOR_POOL_SLOT_INVALID) {
        uvisor_pool_queue_entry_t * first_free;
        first_free = &pool->management_array[pool->first_free];

        pool->first_free = first_free->dequeued.next;

        /* This assignment isn't strictly necessary, but for debugging
         * purposes, it may help us find bugs sooner. */
        pool->management_array[fresh].dequeued.next = UVISOR_POOL_SLOT_INVALID;

        /* This assignment is necessary still, as it helps prevents double
         * frees from the pool. */
        pool->management_array[fresh].dequeued.state = UVISOR_POOL_SLOT_IS_DEQUEUED;

        ++pool->num_allocated;
    }

    return fresh;
}

/* Add an element to the back of the queue. */
static void enqueue(uvisor_pool_queue_t * pool_queue, uvisor_pool_slot_t slot)
{
    uvisor_pool_queue_entry_t * slot_entry = &pool_queue->pool.management_array[slot];

    /* If this is the first allocated slot. */
    if (pool_queue->head == UVISOR_POOL_SLOT_INVALID) {
        pool_queue->head = slot;
        uvisor_pool_queue_entry_t * head_entry = &pool_queue->pool.management_array[pool_queue->head];
        head_entry->queued.prev = UVISOR_POOL_SLOT_INVALID;
    } else {
        /* This is not the first allocated slot, so we have to update where
         * tail's prev next points to. */
        uvisor_pool_queue_entry_t * tail_entry = &pool_queue->pool.management_array[pool_queue->tail];
        tail_entry->queued.next = slot;
        slot_entry->queued.prev = pool_queue->tail;
    }

    /* Add slot to the end of the queue. */
    slot_entry->queued.next = UVISOR_POOL_SLOT_INVALID;
    pool_queue->tail = slot;
}

uvisor_pool_slot_t uvisor_pool_allocate(uvisor_pool_t * pool, uint32_t timeout_ms)
{
    if (pool->blocking) {
        int status;
        status = uvisor_semaphore_pend(&pool->semaphore, timeout_ms);
        if (status) {
            /* We may have timed out waiting for a slot. */
            return UVISOR_POOL_SLOT_INVALID;
        }
    }

    uvisor_mutex_acquire(&pool->mutex, UVISOR_WAIT_FOREVER);
    uvisor_pool_slot_t fresh = pool_alloc(pool);
    uvisor_mutex_release(&pool->mutex);

    return fresh;
}

void uvisor_pool_queue_enqueue(uvisor_pool_queue_t * pool_queue, uvisor_pool_slot_t slot)
{
    uvisor_mutex_acquire(&pool_queue->pool.mutex, UVISOR_WAIT_FOREVER);
    if (slot != UVISOR_POOL_SLOT_INVALID) {
        enqueue(pool_queue, slot);
    }
    uvisor_mutex_release(&pool_queue->pool.mutex);
}

static void pool_free(uvisor_pool_t * pool, uvisor_pool_slot_t slot)
{
    uvisor_pool_queue_entry_t * slot_entry = &pool->management_array[slot];

    slot_entry->dequeued.next = pool->first_free;
    slot_entry->dequeued.state = UVISOR_POOL_SLOT_IS_FREE;

    pool->first_free = slot;

    --pool->num_allocated;
}

static void dequeue(uvisor_pool_queue_t * pool_queue, uvisor_pool_slot_t slot)
{
    uvisor_pool_queue_entry_t * slot_entry = &pool_queue->pool.management_array[slot];

    if (pool_queue->head == pool_queue->tail) {
        pool_queue->head = UVISOR_POOL_SLOT_INVALID;
        pool_queue->tail = UVISOR_POOL_SLOT_INVALID;
    } else if (pool_queue->head == slot) {
        uvisor_pool_queue_entry_t * next_entry = &pool_queue->pool.management_array[slot_entry->queued.next];
        next_entry->queued.prev = UVISOR_POOL_SLOT_INVALID;
        pool_queue->head = slot_entry->queued.next;
    } else if (pool_queue->tail == slot) {
        uvisor_pool_queue_entry_t * prev_entry = &pool_queue->pool.management_array[slot_entry->queued.prev];
        prev_entry->queued.next = UVISOR_POOL_SLOT_INVALID;
        pool_queue->tail = slot_entry->queued.prev;
    } else {
        uvisor_pool_queue_entry_t * next_entry = &pool_queue->pool.management_array[slot_entry->queued.next];
        uvisor_pool_queue_entry_t * prev_entry = &pool_queue->pool.management_array[slot_entry->queued.prev];
        prev_entry->queued.next = slot_entry->queued.next;
        next_entry->queued.prev = slot_entry->queued.prev;
    }

    slot_entry->dequeued.next = UVISOR_POOL_SLOT_INVALID;
    slot_entry->dequeued.state = UVISOR_POOL_SLOT_IS_DEQUEUED;
}

uvisor_pool_slot_t uvisor_pool_free(uvisor_pool_t * pool, uvisor_pool_slot_t slot)
{
    /* TODO refactor with uvisor_pool_queue_dequeue */
    if (slot >= pool->num) {
        return UVISOR_POOL_SLOT_INVALID;
    }

    uvisor_pool_queue_entry_t * slot_entry = &pool->management_array[slot];
    uvisor_mutex_acquire(&pool->mutex, UVISOR_WAIT_FOREVER);
    uvisor_pool_slot_t state = slot_entry->dequeued.state;
    if (state == UVISOR_POOL_SLOT_IS_FREE) {
        /* Already freed. Return. */
        uvisor_mutex_release(&pool->mutex);
        return state;
    }

    pool_free(pool, slot);
    uvisor_mutex_release(&pool->mutex);
    if (pool->blocking) {
        uvisor_semaphore_post(&pool->semaphore);
    }

    return slot;
}

uvisor_pool_slot_t uvisor_pool_queue_dequeue(uvisor_pool_queue_t * pool_queue, uvisor_pool_slot_t slot)
{
    /* TODO refactor with uvisor_pool_free */
    if (slot >= pool_queue->pool.num) {
        return UVISOR_POOL_SLOT_INVALID;
    }

    uvisor_pool_queue_entry_t * slot_entry = &pool_queue->pool.management_array[slot];
    uvisor_mutex_acquire(&pool_queue->pool.mutex, UVISOR_WAIT_FOREVER);
    uvisor_pool_slot_t state = slot_entry->dequeued.state;
    if (state == UVISOR_POOL_SLOT_IS_FREE || state == UVISOR_POOL_SLOT_IS_DEQUEUED) {
        /* Already dequeued or freed. Return. */
        uvisor_mutex_release(&pool_queue->pool.mutex);
        return state;
    }

    /* If the queue is non-empty: */
    if (pool_queue->head != UVISOR_POOL_SLOT_INVALID) {
        /* Dequeue the slot. */
        dequeue(pool_queue, slot);
    }
    uvisor_mutex_release(&pool_queue->pool.mutex);

    return slot;
}

uvisor_pool_slot_t uvisor_pool_queue_dequeue_first(uvisor_pool_queue_t * pool_queue)
{
    uvisor_pool_slot_t slot;

    uvisor_mutex_acquire(&pool_queue->pool.mutex, UVISOR_WAIT_FOREVER);
    slot = pool_queue->head;

    /* If the queue is non-empty: */
    if (pool_queue->head != UVISOR_POOL_SLOT_INVALID) {
        /* Dequeue the slot. */
        dequeue(pool_queue, slot);
    }
    uvisor_mutex_release(&pool_queue->pool.mutex);

    return slot;
}

uvisor_pool_slot_t uvisor_pool_queue_find_first(uvisor_pool_queue_t * pool_queue,
                                                TQueryFN_Ptr query_fn, void * context)
{
    uvisor_pool_slot_t slot;

    uvisor_mutex_acquire(&pool_queue->pool.mutex, UVISOR_WAIT_FOREVER);
    /* Walk the queue, looking for the first slot that matches the query. */
    slot = pool_queue->head;
    while (slot != UVISOR_POOL_SLOT_INVALID)
    {
        uvisor_pool_queue_entry_t * entry = &pool_queue->pool.management_array[slot];

        /* NOTE: The query function is called with the queue mutex held, so be
         * careful. */
        int query_result = query_fn(slot, context);

        if (query_result) {
            uvisor_mutex_release(&pool_queue->pool.mutex);
            return slot;
        }

        slot = entry->queued.next;
    }
    uvisor_mutex_release(&pool_queue->pool.mutex);

    /* We didn't find a match. */
    return UVISOR_POOL_SLOT_INVALID;
}