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
#include <map>
#include <cstdlib>
#include <cstring>

#ifdef CMSIS_OS_RTX
/* RTX expects message queue pools have room for 4 (32-bit) words of metadata in
 * addition to the number of words the message queue needs to hold. */
#define RESULT_POOL_SIZE_BYTES ((1 + 4) * sizeof(void *))

/* Internal-only result type */
/* XXX Only the target function's box context (callee) should be allowed to
 * post to this. But, that's not good enough. That target box is not really
 * trusted, yet we need to trust that it will eventually set a return code, not
 * give us back too many return codes, and the wrong not set nonsense return
 * codes; some other task in the target box could be writing to all queue ids
 * (32-bit easy to brute force) in a loop nonsense return codes: if one doesn't
 * call into that box, they don't have a problem. But, when they do, they have
 * to have some sort of trust in how the return codes get posted back. */
typedef struct {
    osMessageQDef_t result_q;
    osMessageQId result_q_id;

    /* XXX How big the item in the queue is. Used by uVisor to determine how
     * much to copy into a buffer. We'll do this later. */
    size_t result_size;

    /* RTX expects a pre-allocated pool. (It doesn't allocate one in
     * osMessageCreate.) Make a pre-allocated pool available by making it part
     * of the result type. */
    uint8_t pool[RESULT_POOL_SIZE_BYTES];
} uvisor_rpc_result_internal_t;
#else
#error "Unknown how to make the internal result queue type for this OS"
#endif

STATIC_ASSERT(sizeof(uvisor_rpc_result_t) >= sizeof(uvisor_rpc_result_internal_t),
              external_result_object_not_big_enough_for_internal_result_object);

#ifdef CMSIS_OS_RTX
UVISOR_EXTERN int rpc_init_result(uvisor_rpc_result_t * result)
{
    uvisor_rpc_result_internal_t * r = (uvisor_rpc_result_internal_t *) result;

    /* RTX expects pool is zero-initialized. */
    memset(r->pool, 0, RESULT_POOL_SIZE_BYTES);

    /* Result queues always hold just one 32-bit word. */
    r->result_q.queue_sz = 1;

    /* Point the RTX q to the pre-allocated pool. */
    r->result_q.pool = r->pool;

    /* Create the message queue */
    r->result_q_id = osMessageCreate(&r->result_q, NULL);

    return 0;
}
#else
#error "Unknown how to init a uvisor_rpc_result_t for this OS"
#endif

/* -------------------------------- */

#ifdef CMSIS_OS_RTX
/* Internal-only callee type */
/* XXX Only the target function's box context (callee) should be allowed to
 * post to this. But, that's not good enough. That target box is not really
 * trusted, yet we need to trust that it will eventually set a return code, not
 * give us back too many return codes, and the wrong not set nonsense return
 * codes; some other task in the target box could be writing to all queue ids
 * (32-bit easy to brute force) in a loop nonsense return codes: if one doesn't
 * call into that box, they don't have a problem. But, when they do, they have
 * to have some sort of trust in how the return codes get posted back. */
typedef struct {
    osMailQDef_t callee_q;
    osMailQId callee_q_id;

    /* On RTX, mail queues contain a pool that contains two pointers. One
     * points to memory for storing pointers to items allocated in the queue.
     * The other points to memory for storing items allocated in the queue. */
    void * pool[2];
} uvisor_rpc_callee_queue_internal_t;
#else
#error "Unknown how to make the internal result queue type for this OS"
#endif

STATIC_ASSERT(sizeof(uvisor_rpc_callee_queue_t) >= sizeof(uvisor_rpc_callee_queue_internal_t),
              external_callee_queue_not_big_enough_for_internal_callee_queue);

/* Global mapping from function pointers to rpc destination queues. TODO make
 * this private to uvisor lib. TODO make the callee queue contain box id
 * information or something, so we can do box-specific rules to prevent rpc or
 * whatnot. */
/* XXX Can't use maps between boxes because nodes in it are allocated with malloc which is now
 * box private. */
//std::map<const TFN_Ptr, uvisor_rpc_callee_queue_internal_t *> fp_to_queue_map;
/* XXX TODO Make this dynamic or per-box or something like that. Also, make it
 * have nicer lookup properties than linear search. */
static TFN_Ptr * g_fn_array; /* entry number to Function pointer */
static uvisor_rpc_callee_queue_internal_t ** g_callee_queue_array; /* entry number to internal callee queue */
static osMailQId * g_callee_queue_id_array; /* entry number to callee queue id */
static size_t g_fn_entries;
static const size_t max_num_rpc_target_functions = 16;

#ifdef CMSIS_OS_RTX

/* Return the number of bytes needed for a pool that would back the pointer
 * storage for a mail queue. */
static size_t rtx_mail_queue_q_size(size_t max_num_items)
{
    /* The queue size in number of items */
    size_t queue_sz = max_num_items;

    return 4U + (4U + queue_sz) * sizeof(void *);
    /* From svcMessageCreate
     * rt_mbx_init(queue_def->pool, (uint16_t)(4U*(queue_def->queue_sz + 4U)));
     */
}

/* Return how many bytes a pool must be able to hold in order to hold the
 * specified number of items as well as any overhead that must also be stored
 * in the pool. */
UVISOR_EXTERN size_t rpc_pool_size_for_callee_queue(size_t max_num_items)
{

    /* This size information is obtained from the CMSIS RTOS macro `osMailQDef`
     * (mbed-os/core/rtos/rtx/TARGET_CORTEX_M/cmsis_os.h). The size information
     * is internal RTX implementation detail that we have to get into here in
     * order to be able to dynamically allocate queues (instead of just using
     * the macros CMSIS RTOS provides for statically allocating RTX queues). */
    size_t q_q_size = rtx_mail_queue_q_size(max_num_items);
    //size_t q_m_size = 3 + ((sizeof(uvisor_rpc_message_t) + 3) / 4) * queue_sz;
    /* XXX Even though this is what the macro does, this isn't large enough!
     * The sizeof(struct OS_BM) maybe used to be 3, but now is 12! Actually, we
     * didn't account for the OS_BM overhead which is per-entry (per-block) and
     * not one fixed one.
     */
    //size_t q_m_size = 3 + ((item_sz + 3) / 4) * queue_sz;

    /* Numbers obtained from svcMailCreate */
    /*    blk_sz = (queue_def->item_sz + 3U) & (uint32_t)~3U;
     *    _init_box(pool, sizeof(struct OS_BM) + (queue_def->queue_sz * blk_sz), blk_sz);
     */
    /* The queue size in number of items */
    size_t queue_sz = max_num_items;
    size_t item_sz = sizeof(uvisor_rpc_message_t);
    size_t blk_sz = (item_sz + 3U) & (uint32_t)~3U;
    size_t box_size = 12 + queue_sz * blk_sz;
    size_t q_m_size = box_size;

    return q_q_size + q_m_size;
}

static void map_function_to_queue(const TFN_Ptr fn, uvisor_rpc_callee_queue_internal_t *q)
{
    /* XXX What about duplicates? What about out-of-memory in the map? This is
     * a potential DOS attack to register too many functions in this map from
     * some malicious box, preventing a nice box from registering its
     * functions. There should probably be a per-box limit to functions that
     * can be registered for. Perhaps there should be a map per box, with boxes
     * searched through linearly (because there won't be many boxes in a
     * system) for both add and look up. There should also be a deinit function
     * to unregister the callee queue's functions before the queue might be
     * destroyed, however we need to be robust enough to handle the case where
     * a malicious box deletes its queue without unregistering it, so maybe we
     * don't need a deinit function. */

    //fp_to_queue_map.insert(std::pair<const TFN_Ptr, uvisor_rpc_callee_queue_internal_t *>(fn_ptr_array[i], q));

    /* XXX TODO Watch out for re-entrancy problems here. The entry has to be
     * allocated atomically (atomic read and increment). */
    size_t entry = g_fn_entries++;

    /* Check that entry is within bounds of the arrays here before accessing
     * the arrays. */
    if (entry >= max_num_rpc_target_functions)
    {
        /* XXX TODO Better error handling instead of ignoring. */
        return;
    }
    g_fn_array[entry] = fn;
    g_callee_queue_array[entry] = q;
    g_callee_queue_id_array[entry] = q->callee_q_id;
}

/* XXX If the mapping from function pointers to queues is private to uvisor
 * lib, then this will bus fault. What needs to happen is, instead, that the
 * mapping can be read from any box, but only written to from the box private
 * to uvisor lib. Otherwise, incurring a box-switch overhead just to find the
 * RPC target sort of defeats the purpose of local delivery. */
static uvisor_rpc_callee_queue_internal_t * get_queue_for_function(const TFN_Ptr fn)
{
    //return fp_to_queue_map[fn];

    /* Find the entry with linear search */
    for (size_t i = 0; i < max_num_rpc_target_functions; i++) {
        /* If found: */
        if (g_fn_array[i] == fn) {
            /* Return the queue. */
            return g_callee_queue_array[i];
        }
    }

    /* The entry for the given function pointer wasn't found. */
    return NULL;
}

#if 0
static osMailQId get_queue_id_for_function(const TFN_Ptr fn)
{
    //return fp_to_queue_map[fn];

    /* Find the entry with linear search */
    for (size_t i = 0; i < max_num_rpc_target_functions; i++) {
        /* If found: */
        if (g_fn_array[i] == fn) {
            /* Return the queue. */
            return g_callee_queue_id_array[i];
        }
    }

    /* The entry for the given function pointer wasn't found. */
    return NULL;
}
#endif

UVISOR_EXTERN int rpc_init_memory_funtime(void)
{
    /* XXX Bad idea.
     * Map memories should be allocated in a uvisor lib local box instead, but
     * we don't have one of those yet... TODO Make a uvisor lib local box. If
     * we do that, how would the memories be written to? We'd have to use RPC
     * to write to them... Each box needs its own map memory that it can write
     * to. */
    static bool rpc_map_initialized = false;
    if (!rpc_map_initialized)
    {
        /* XXX Until we can allocate in uvisor lib's private box some memory
         * that is readable by everybody, but writable only by uvisor lib,
         * allocate in box 0. */
        g_fn_array = (TFN_Ptr *) malloc(max_num_rpc_target_functions * sizeof(*g_fn_array));
        g_callee_queue_array = (uvisor_rpc_callee_queue_internal_t **) malloc(max_num_rpc_target_functions * sizeof(*g_callee_queue_array));
        g_callee_queue_id_array = (osMailQId *) malloc(max_num_rpc_target_functions * sizeof(*g_callee_queue_id_array));
        g_fn_entries = 0;

        rpc_map_initialized = true;
    }

    return 0;
}

/* Do a big hack to get RPC memories initialized (and hacky box 0 allocator). Ideally, if we need this at
 * all, we'd call it from uvisor_lib_init. */
SecureAllocator box_0_allocator;
class HackyMagicSuperFuntime
{
public:
    HackyMagicSuperFuntime()
    {
        rpc_init_memory_funtime();

        box_0_allocator = secure_allocator_create_with_pool(some_memory, sizeof(some_memory));
    }

    uint8_t some_memory[6000];
};

HackyMagicSuperFuntime way_too_funtime;

UVISOR_EXTERN void * malloc_0(size_t bytes)
{
    return secure_malloc(box_0_allocator, bytes);
}

UVISOR_EXTERN void free_0(void * ptr)
{
    secure_free(box_0_allocator, ptr);
}

UVISOR_EXTERN int rpc_init_callee_queue(uvisor_rpc_callee_queue_t * queue, uint8_t * pool, size_t pool_size,
                                        size_t max_num_items,
                                        const TFN_Ptr fn_ptr_array[], size_t fn_count)
{
    uvisor_rpc_callee_queue_internal_t * q = (uvisor_rpc_callee_queue_internal_t *) queue;

    /* Make sure the pool is big enough */
    if (pool_size < rpc_pool_size_for_callee_queue(max_num_items)) {
        /* XXX Error code improve, please. */
        /* Not enough room in provided pool to store the desired number of
         * items and the necessary RTX overhead */
        return -1;
    }

    memset(&q->callee_q, 0, sizeof(q->callee_q));

    /* RTX expects pool is zero-initialized */
    memset(pool, 0, pool_size);

    /* Point the RTX callee_q to the user-allocated pool.
     * The memory for the items in the queue comes directly after the memory
     * for the pointers to items in the queue (including any overhead). */
    void * const q_q_mem = pool;
    void * const q_m_mem = &pool[rtx_mail_queue_q_size(max_num_items)];
    q->pool[0] = q_q_mem;
    q->pool[1] = q_m_mem;

    q->callee_q.queue_sz = max_num_items;
    q->callee_q.item_sz = sizeof(uvisor_rpc_message_t);
    q->callee_q.pool = q->pool;

    /* Create the mail queue */
    q->callee_q_id = osMailCreate(&q->callee_q, NULL);
    /* How to check for errors here? Seems I got back a bogus callee_q_id... SHould
     * be NULL if I get an error, but it isn't NULL. Does this allocate
     * anything on a pool? Does that allocation overflow someplace it
     * shouldn't? This should be equal to the address of the pool. */
    /* _init_box fucks up the queue_def->pool (q->callee_q.pool) */
    /* This is RTX an specific check for making sure the queue didn't get
     * clobbered. */
    if ((void *) q->callee_q_id != (void *) q->pool) {
        /* 0x130e12fc */
        /* Something very bad happened in osMailCreate. */
        return -1;
    }

    /* This is RTX an specific check for making sure the queue didn't get
     * clobbered in another place. */
    if (q->callee_q.queue_sz != max_num_items)
    {
        /* Something very bad happened in osMailCreate. */
        return -1;
    }

    /* Register all functions this queue will handle with uVisor lib, so that
     * it knows where to route RPC messages to. */
    for (size_t i = 0; i < fn_count; i++) {
        map_function_to_queue(fn_ptr_array[i], q);
    }

    return 0;
}
#else
#error "Unknown how to init a uvisor_rpc_callee_queue_t for this OS"
#endif

/* -------------------------------- */

/* Note: This function runs in caller context. */
/* Note: This (XXX TODO should) run in callee context. The msg is assumed to be available in
 * callee context. */
UVISOR_EXTERN int uvisor_rpc_message_local(uvisor_rpc_message_t * msg)
{
    uvisor_rpc_message_t * qmsg;
    uvisor_rpc_callee_queue_internal_t * q = get_queue_for_function(msg->fn);
    if (q == NULL) {
        /* Ignore calls to functions that are not RPC targets */
        return -1;
    }

    /* XXX We can't read the callee queue for this function, because it is in
     * callee box context. We need to store the callee_q_id in a publicly
     * readable place, for now. If only uVisor needed to know which queue to
     * use for which function, that'd also be fine. */
    const osMailQId dest_mail_q_id = q->callee_q_id;

    /* XXX The pool for the mail should be in box private memory, so this needs
     * some rework. uVisor needs to do the data copy on behalf of the sender if
     * the firewall rules pass for this caller. */
    /* XXX Not sure if we want to wait forever on the destination queue to have
     * room. I think we do, as it's the waiting for the result return value
     * that the user specified a timeout for. However, we might not want to
     * wait forever in all cases. Maybe this should be 0 timeout instead. -- We
     * don't want to wait forever because a malicious callee box could block
     * calls into it forever, even if the async call is supposed to timeout. */
    /* XXX This doesn't work because the allocated memory comes from the callee box
     * context. */
    qmsg = (uvisor_rpc_message_t *) osMailAlloc(dest_mail_q_id, 0);
    if (!qmsg) {
        /* XXX TODO better error message for no room in destination queue */
        return -1;
    }
    qmsg->fn = msg->fn;
    qmsg->p0 = msg->p0;
    qmsg->p1 = msg->p1;
    qmsg->p2 = msg->p2;
    qmsg->p3 = msg->p3;
    qmsg->result_q_id = msg->result_q_id; /* TODO make uVisor populate this, so it can be trusted. */

    /* XXX Why did we want to have send queues within the caller process for
     * RPC? Maybe because this function wouldn't be able to use the receive
     * queue in the callee process directly. (XXX Yes, that's a benefit. Also a
     * benenfit is no uVisor context switch overhead for sending messages,
     * because would only put items in the queue when in privileged mode, which
     * means we need uVisor to use the privileged queue editing function
     * directly [add to export table thingy]). For now, this is ok because
     * prototyping. After we add uVisor firewall, of course we can't use the
     * queue directly. */ /* XXX TODO Make sure uVisor gets the thread switch
     * callback even when the thread won't be switched (thread to switch to is
     * identical to thread currently running); this allows the mail to get
     * delivered when the current thread is the highest priority ready thread
     * (maybe not highest priority overall, but highest priority ready thread)
     * */
    osMailPut(dest_mail_q_id, qmsg);

    return 0;
}

/* Note: This function runs in caller context. */
static int rpc_fncall_async_timeout(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn,
                                    uvisor_rpc_result_t * result, uint32_t timeout_ms)
{
    int status;
    uvisor_rpc_result_internal_t * r = (uvisor_rpc_result_internal_t *) result;

    /* TODO - check the type of the call target.
            - if call target is not a known RPC target, do a normal local
              function call. (async will have to use the queue still, perhaps)
            - uVisor firewall
            - target queue from lib config
    */

    /* Note: This could be on stack, because uVisor will make a copy for remote
     * delivery in the callee box. However, for local delivery, we need to not
     * use the stack so that the message remains allocated for the local
     * callee. The local callee would need to free the message. This is fine,
     * because there is no inter-box trusting going on about the freeing; each
     * box frees and allocates only its own memory. */
    uvisor_rpc_message_t * msg = (uvisor_rpc_message_t *) malloc(sizeof(*msg));

    msg->fn = fn;
    msg->p0 = p0;
    msg->p1 = p1;
    msg->p2 = p2;
    msg->p3 = p3;
    msg->result_q_id = r->result_q_id; /* TODO make uVisor verify that this caller box is the owner of this result queue */

    /* XXX TODO Implement this function. Make it copy the message to the callee
     * box context, verify the ownership of the result queue, and deprivilege
     * to call uvisor_rpc_message_local, and execute the uVisor firewall (to
     * prevent any allocation in the callee box if firewall rules fail). When
     * is the msg freed? In uvisor_rpc_message_local, a uVisor lib function.
     * What happens if the msg is not freed? then the callee box's own heap
     * fills up and its the callee box's own fault. */ /* XXX TODO need ability
    to allocate memory from uVisor privileged
    mode in a target box */
    //uvisor_rpc_msg_send(&msg);
    /* XXX TODO implement in uVisor. uVisor can't
    know from the target box if the target box could put it in the queue or
    not... figure out how to deal with that. When we do real IPC with real
    blocking semantics and such, we don't need to deal with this problem
    because we'll have just one IPC queue and uVisor itself will know if the
    message was able to go in or not. */
    status = uvisor_rpc_message_local(msg);

    free(msg);

    return status;
}

/* Note: This function runs in caller context. */
UVISOR_EXTERN int rpc_fncall_async(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn,
                                   uvisor_rpc_result_t * result)
{
    /* Asynchronous callers want to not wait for memory allocations in the
     * callee queue. They want to see the result fail right away. So, call
     * rpc_fncall_async_timeout with a 0 timeout. */
    static const uint32_t async_timeout_ms = 0;
    return rpc_fncall_async_timeout(p0, p1, p2, p3, fn, result, async_timeout_ms);
}


/* Note: This function runs in caller context. */
UVISOR_EXTERN uint32_t rpc_fncall(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn)
{
    int status;
    /* XXX TODO Allocate result queue on stack here. */
    //uvisor_rpc_result_t result;
again:
    uvisor_rpc_result_t *result = (uvisor_rpc_result_t *) malloc_0(sizeof(*result));
    if (!result)
    {
        /* No memory, but no way to fail. Try again. */
        goto again;
    }
    rpc_init_result(result);

    /* Wait forever for a non-error, valid result from rpc_fncall_async_timeout. */
    for (;;) {
        /* Synchronous callers want to wait for memory allocations in the
         * callee queue. They can't tolerate the memory allocation failing, as
         * there is no way to report the error. They also don't want to spin
         * retrying the allocation forever. So, call rpc_fncall_async_timeout with
         * an infinite timeout. */
        static const uint32_t sync_timeout_ms = osWaitForever;
        status = rpc_fncall_async_timeout(p0, p1, p2, p3, fn, result, sync_timeout_ms);

        if (!status) {
            /* The async call was successful. */
            break;
        }

        /* Something bad happened. We have no choice but to try again. */
    }

    /* Wait forever for a non-error, valid result from rpc_fncall_wait */
    for (;;) {
        uint32_t ret;
        int status = rpc_fncall_wait(result, osWaitForever, &ret);
        if (!status) {
            free_0(result);
            return ret;
        }
    }
}

/* Note: This function runs in callee context. */
UVISOR_EXTERN int rpc_fncall_waitfor(uvisor_rpc_callee_queue_t * queue, uint32_t timeout_ms)
{
    uvisor_rpc_callee_queue_internal_t * q = (uvisor_rpc_callee_queue_internal_t *) queue;
    osEvent event;

    /* XXX We aren't smartly waiting only for those functions specified in the
     * array, but for any inbound RPC. This should be thought through better.
     * As currently implemented, we completely ignore the fn_ptr_array and the
     * fn_count. uVisor will not enqueue RPC messages that we didn't declare as
     * valid RPC targets, so there is no need for additional checks here to
     * make sure the function is in our list; but this also assumes that there
     * is only one call to rpc_fncall_waitfor in a process, or that all calls
     * to rpc_fncall_waitfor use the same function list. If we want to be
     * smarter and only wake up if the fncall_waitfor, we may have to create a
     * queue per rpc_fncall_waitfor call (or, worst case, every target function);
     * but I don't think RTX can wait on multiple queues right now (that'd have
     * to be serial); RTX can wait for multiple events, but there are only 32
     * available and it'd suck to burn through them for implementing uVisor
     * RPC. Anyway, think this stuff through more later. */

    /* This scheduling algorithm is dumb for now. We only process up to 1 RPC
     * at a time, even if more are outstanding. Call rpc_fncall_waitfor
     * multiple times if you want to process multiple RPC messages. There is
     * also no parallel dispatching to multiple threads. */
    event = osMailGet(q->callee_q_id, timeout_ms);
    if (event.status == osEventMail)
    {
        int result;
        osStatus status;
        uvisor_rpc_message_t * msg;

        msg = (uvisor_rpc_message_t *) event.value.p;

        /* Note that the uVisor_RPC_Message is placed by uVisor into this
         * queue. Nobody else is allow to put stuff into the queue. So, we can
         * assume the queue things are good and can just dispatch now. (no need
         * to check msg->fn for non-NULL). TODO prevent others from enqueueing
         * into the rpc queue (uVisor queue firewall). One idea to do this is
         * to make the queue in box-private memory, and to only if the firewall
         * passes, depriv to target box context to enqueue the item and return.
         * This is a lot of overhead, but we can optimize that out later (by
         * perhaps a privileged mode handler provided by RTX to uVisor that can
         * enqueue items) */

        /* Dispatch the RPC. */
        result = msg->fn(msg->p0, msg->p1, msg->p2, msg->p3); /*
        Not sure we need this 4 register sort of interface for target RPC
        functions. A single void * argument should work just fine, but doesn't
        allow clients freedom over calling convention (maybe they want more
        stuff passed by registers than a single void * would allow, for some
        reason). */

        /* Notify the caller. Don't wait for any room in the caller's queue;
         * fail to post the return value if the caller's queue has trouble. */
        static const uint32_t result_timeout_ms = 0;
        /* XXX The result q may be disappeared now. We don't really have a way
         * to tell if it safe to call this function or not yet. Without
         * checking before calling this, we could inadvertently use-after-free.
         * */
        status = osMessagePut(msg->result_q_id, result, result_timeout_ms);
        (void) status; /* Ignore result, since we don't care about the caller's
                          result queue being broken. */

        /* XXX TODO If the result queue memory has been freed (maybe after a
         * timeout happens) before the callee has a chance to put the item in the
         * queue, we get a bus fault (access to unauthorized memory) due to use of
         * the queue after it has been freed. Fix this. We don't want a caller to
         * be able to crash the system or crash the destination box. */

        /* Now that the function has returned, free the item. It is good that
         * the caller doesn't have to trust that the callee will do this.
         * uVisor lib does it for the caller. uVisor lib has to be trusted...
         * */
        osMailFree(q->callee_q_id, msg);
        return 0;
    }

    return event.status;
}

UVISOR_EXTERN int rpc_fncall_wait(uvisor_rpc_result_t * result, uint32_t timeout_ms, uint32_t * ret)
{
    osEvent event;
    uvisor_rpc_result_internal_t * r = (uvisor_rpc_result_internal_t *) result;
    int status = -1;

    event = osMessageGet(r->result_q_id, timeout_ms);
    if (event.status == osEventMessage) {
        *ret = (uint32_t) event.value.p;
        status = 0;
    }

    return status;
}
