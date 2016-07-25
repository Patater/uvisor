#include "uvisor.h"
#include "api/inc/pool_queue_exports.h"
#include "uvisor.h"
#include <string.h>

/* Only build tests for debug builds. */
#ifndef NDEBUG

#define NUM_ELEMENTS 4
static uint8_t pool_queue_mem[sizeof(uvisor_pool_queue_t) + sizeof(uvisor_pool_queue_entry_t) * NUM_ELEMENTS];
static uint32_t array[4];

static uvisor_pool_queue_t * get_queue_for_test(void)
{
    uvisor_pool_queue_t * pool_queue = (uvisor_pool_queue_t *) pool_queue_mem;

    static size_t const stride = sizeof(*array);
    static size_t const num = sizeof(array) / sizeof(*array);
    static size_t const blocking = 0;

    /* Fill with crap */
    memset(pool_queue_mem, 0xA5, sizeof(pool_queue_mem));
    memset(array, 0x00, sizeof(array));

    uvisor_pool_queue_init(pool_queue, array, stride, num, blocking);

    return pool_queue;
}

static uvisor_pool_t * get_pool_for_test(void)
{
    /* Note that pool_queue_mem is is bigger than necessary for just a pool,
     * but it's okay to use. */
    uvisor_pool_t * pool = (uvisor_pool_t *) pool_queue_mem;

    size_t const stride = sizeof(*array);
    size_t const num = sizeof(array) / stride;
    static size_t const blocking = 0;

    /* Fill with crap */
    memset(pool_queue_mem, 0xA5, sizeof(pool_queue_mem));
    memset(array, 0x00, sizeof(array));

    uvisor_pool_init(pool, array, stride, num, blocking);

    return pool;
}

static void queue_sane_after_init(void)
{
    uint8_t pool_queue_mem[sizeof(uvisor_pool_queue_t) + sizeof(uvisor_pool_queue_entry_t) * NUM_ELEMENTS];
    uvisor_pool_queue_t * pool_queue = (uvisor_pool_queue_t *) pool_queue_mem;

    uint32_t array[4];
    static size_t const stride = sizeof(*array);
    static size_t const num = sizeof(array) / sizeof(*array);
    static size_t const blocking = 0;

    /* Fill with crap */
    memset(pool_queue_mem, 0xA5, sizeof(array));

    uvisor_pool_queue_init(pool_queue, array, stride, num, blocking);

    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);
}

static void queue_allocate_1(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot;

    assert(pool_queue->pool.num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot = uvisor_pool_queue_allocate(pool_queue, timeout_ms);

    assert(slot == 0);
    assert(pool_queue->pool.num_allocated == 1);

    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);

    uvisor_pool_queue_enqueue(pool_queue, slot);

    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 0);
    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == UVISOR_POOL_SLOT_INVALID);
}

static void queue_allocate_2(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot1;

    static const uint32_t timeout_ms = 0;
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    slot1 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot1);

    assert(slot1 == 1);
    assert(pool_queue->pool.num_allocated == 2);

    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 1);
    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 1);
    assert(pool_queue->pool.management_array[1].queued.prev == 0);
    assert(pool_queue->pool.management_array[1].queued.next == UVISOR_POOL_SLOT_INVALID);
}

static void queue_allocate_3(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot2;

    static const uint32_t timeout_ms = 0;
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    slot2 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot2);

    assert(slot2 == 2);
    assert(pool_queue->pool.num_allocated == 3);

    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 2);
    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 1);
    assert(pool_queue->pool.management_array[1].queued.prev == 0);
    assert(pool_queue->pool.management_array[1].queued.next == 2);
    assert(pool_queue->pool.management_array[2].queued.prev == 1);
    assert(pool_queue->pool.management_array[2].queued.next == UVISOR_POOL_SLOT_INVALID);
}

static uvisor_pool_queue_t * queue_allocate_4(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot3;

    static const uint32_t timeout_ms = 0;
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    slot3 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot3);

    assert(slot3 == 3);
    assert(pool_queue->pool.num_allocated == 4);

    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 3);
    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 1);
    assert(pool_queue->pool.management_array[1].queued.prev == 0);
    assert(pool_queue->pool.management_array[1].queued.next == 2);
    assert(pool_queue->pool.management_array[2].queued.prev == 1);
    assert(pool_queue->pool.management_array[2].queued.next == 3);
    assert(pool_queue->pool.management_array[3].queued.prev == 2);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    return pool_queue;
}

static void queue_allocate_5(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot4;

    static const uint32_t timeout_ms = 0;
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    slot4 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot4);

    assert(slot4 == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.num_allocated == 4);

    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 3);
    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 1);
    assert(pool_queue->pool.management_array[1].queued.prev == 0);
    assert(pool_queue->pool.management_array[1].queued.next == 2);
    assert(pool_queue->pool.management_array[2].queued.prev == 1);
    assert(pool_queue->pool.management_array[2].queued.next == 3);
    assert(pool_queue->pool.management_array[3].queued.prev == 2);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);
}

static void queue_allocate_0_free_too_many(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot;
    uvisor_pool_slot_t freed;

    assert(pool_queue->pool.num_allocated == 0);

    for (slot = 0; slot < 20; slot++) {
        freed = uvisor_pool_queue_dequeue(pool_queue, slot);
        if (slot < pool_queue->pool.num) {
            /* Is already free */
            assert(freed == UVISOR_POOL_SLOT_IS_FREE);
        } else {
            /* Out of range */
            assert(freed == UVISOR_POOL_SLOT_INVALID);
        }
        assert(pool_queue->pool.first_free == 0);
        assert(pool_queue->pool.num_allocated == 0);
        assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
        assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);

        assert(pool_queue->pool.management_array[0].dequeued.next == 1);
        assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
        assert(pool_queue->pool.management_array[1].dequeued.next == 2);
        assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
        assert(pool_queue->pool.management_array[2].dequeued.next == 3);
        assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
        assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
        assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    }
}

static void queue_allocate_1_free_1(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot;

    assert(pool_queue->pool.num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot);

    assert(slot == 0);
    assert(pool_queue->pool.num_allocated == 1);

    uvisor_pool_queue_dequeue(pool_queue, slot);
    assert(pool_queue->pool.first_free == 1);
    assert(pool_queue->pool.num_allocated == 1);
    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);

    uvisor_pool_queue_free(pool_queue, slot);
    assert(pool_queue->pool.first_free == 0);
    assert(pool_queue->pool.num_allocated == 0);
    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);

    assert(pool_queue->pool.management_array[0].dequeued.next == 1);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].dequeued.next == 2);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
}

static void queue_allocate_1_double_free(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot;
    uvisor_pool_slot_t again;

    assert(pool_queue->pool.num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot);

    assert(slot == 0);
    assert(pool_queue->pool.num_allocated == 1);

    uvisor_pool_queue_dequeue(pool_queue, slot);
    uvisor_pool_queue_free(pool_queue, slot);
    uvisor_pool_queue_dequeue(pool_queue, slot);
    again = uvisor_pool_queue_free(pool_queue, slot);

    assert(again == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.first_free == slot);
    assert(pool_queue->pool.num_allocated == 0);
    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);

    assert(pool_queue->pool.management_array[0].dequeued.next == 1);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].dequeued.next == 2);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void queue_allocate_2_free_first(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot0;

    assert(pool_queue->pool.num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot0 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot0);
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));

    assert(pool_queue->pool.num_allocated == 2);

    uvisor_pool_queue_dequeue(pool_queue, slot0);
    uvisor_pool_queue_free(pool_queue, slot0);
    assert(pool_queue->pool.first_free == 0);
    assert(pool_queue->pool.num_allocated == 1);
    assert(pool_queue->head == 1);
    assert(pool_queue->tail == 1);

    assert(pool_queue->pool.management_array[0].dequeued.next == 2);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].queued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void queue_allocate_2_free_second(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot1;

    assert(pool_queue->pool.num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    slot1 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot1);

    assert(pool_queue->pool.num_allocated == 2);

    uvisor_pool_queue_dequeue(pool_queue, slot1);
    uvisor_pool_queue_free(pool_queue, slot1);
    assert(pool_queue->pool.first_free == 1);
    assert(pool_queue->pool.num_allocated == 1);
    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 0);

    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].dequeued.next == 2);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void queue_allocate_3_free_second(void)
{
    /* Setup */
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    uvisor_pool_slot_t slot1;

    assert(pool_queue->pool.num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));
    slot1 = uvisor_pool_queue_allocate(pool_queue, timeout_ms);
    /* XXX More asserts here */
    uvisor_pool_queue_enqueue(pool_queue, slot1);
    uvisor_pool_queue_enqueue(pool_queue, uvisor_pool_queue_allocate(pool_queue, timeout_ms));

    assert(pool_queue->pool.num_allocated == 3);

    uvisor_pool_queue_dequeue(pool_queue, slot1);
    uvisor_pool_queue_free(pool_queue, slot1);
    assert(pool_queue->pool.first_free == 1);
    assert(pool_queue->pool.num_allocated == 2);
    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 2);

    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 2);
    assert(pool_queue->pool.management_array[1].dequeued.next == 3);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].queued.prev == 0);
    assert(pool_queue->pool.management_array[2].queued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void queue_allocate_4_free_forward(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();

    uvisor_pool_queue_dequeue(pool_queue, 0);
    uvisor_pool_queue_free(pool_queue, 0);
    assert(pool_queue->pool.first_free == 0);
    assert(pool_queue->pool.num_allocated == 3);
    assert(pool_queue->head == 1);
    assert(pool_queue->tail == 3);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].queued.next == 2);
    assert(pool_queue->pool.management_array[2].queued.prev == 1);
    assert(pool_queue->pool.management_array[2].queued.next == 3);
    assert(pool_queue->pool.management_array[3].queued.prev == 2);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    uvisor_pool_queue_dequeue(pool_queue, 1);
    uvisor_pool_queue_free(pool_queue, 1);
    assert(pool_queue->pool.first_free == 1);
    assert(pool_queue->pool.num_allocated == 2);
    assert(pool_queue->head == 2);
    assert(pool_queue->tail == 3);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].dequeued.next == 0);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[2].queued.next == 3);
    assert(pool_queue->pool.management_array[3].queued.prev == 2);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    uvisor_pool_queue_dequeue(pool_queue, 2);
    uvisor_pool_queue_free(pool_queue, 2);
    assert(pool_queue->pool.first_free == 2);
    assert(pool_queue->pool.num_allocated == 1);
    assert(pool_queue->head == 3);
    assert(pool_queue->tail == 3);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].dequeued.next == 0);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 1);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    uvisor_pool_queue_dequeue(pool_queue, 3);
    uvisor_pool_queue_free(pool_queue, 3);
    assert(pool_queue->pool.first_free == 3);
    assert(pool_queue->pool.num_allocated == 0);
    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].dequeued.next == 0);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 1);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == 2);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void queue_allocate_4_free_backward(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();

    uvisor_pool_queue_dequeue(pool_queue, 3);
    uvisor_pool_queue_free(pool_queue, 3);
    assert(pool_queue->pool.first_free == 3);
    assert(pool_queue->pool.num_allocated == 3);
    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 2);

    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 1);
    assert(pool_queue->pool.management_array[1].queued.prev == 0);
    assert(pool_queue->pool.management_array[1].queued.next == 2);
    assert(pool_queue->pool.management_array[2].queued.prev == 1);
    assert(pool_queue->pool.management_array[2].queued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);

    uvisor_pool_queue_dequeue(pool_queue, 2);
    uvisor_pool_queue_free(pool_queue, 2);
    assert(pool_queue->pool.first_free == 2);
    assert(pool_queue->pool.num_allocated == 2);
    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 1);

    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == 1);
    assert(pool_queue->pool.management_array[1].queued.prev == 0);
    assert(pool_queue->pool.management_array[1].queued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);

    uvisor_pool_queue_dequeue(pool_queue, 1);
    uvisor_pool_queue_free(pool_queue, 1);
    assert(pool_queue->pool.first_free == 1);
    assert(pool_queue->pool.num_allocated == 1);
    assert(pool_queue->head == 0);
    assert(pool_queue->tail == 0);

    assert(pool_queue->pool.management_array[0].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].queued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].dequeued.next == 2);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);

    uvisor_pool_queue_dequeue(pool_queue, 0);
    uvisor_pool_queue_free(pool_queue, 0);
    assert(pool_queue->pool.first_free == 0);
    assert(pool_queue->pool.num_allocated == 0);
    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);

    assert(pool_queue->pool.management_array[0].dequeued.next == 1);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[1].dequeued.next == 2);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[2].dequeued.next == 3);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void queue_allocate_4_dequeue_first_many(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();
    uvisor_pool_slot_t slot;

    slot = uvisor_pool_queue_dequeue_first(pool_queue);
    assert(slot == 0);
    assert(pool_queue->head == 1);
    assert(pool_queue->tail == 3);
    assert(pool_queue->pool.first_free == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.num_allocated == 4);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[1].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].queued.next == 2);
    assert(pool_queue->pool.management_array[2].queued.prev == 1);
    assert(pool_queue->pool.management_array[2].queued.next == 3);
    assert(pool_queue->pool.management_array[3].queued.prev == 2);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    slot = uvisor_pool_queue_dequeue_first(pool_queue);
    assert(slot == 1);
    assert(pool_queue->head == 2);
    assert(pool_queue->tail == 3);
    assert(pool_queue->pool.first_free == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.num_allocated == 4);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[2].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[2].queued.next == 3);
    assert(pool_queue->pool.management_array[3].queued.prev == 2);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    slot = uvisor_pool_queue_dequeue_first(pool_queue);
    assert(slot == 2);
    assert(pool_queue->head == 3);
    assert(pool_queue->tail == 3);
    assert(pool_queue->pool.first_free == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.num_allocated == 4);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[3].queued.prev == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].queued.next == UVISOR_POOL_SLOT_INVALID);

    slot = uvisor_pool_queue_dequeue_first(pool_queue);
    assert(slot == 3);
    assert(pool_queue->head == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->tail == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.first_free == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.num_allocated == 4);

    assert(pool_queue->pool.management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool_queue->pool.management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool_queue->pool.management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
}

static void get_pointer_to_slot(void)
{
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();

    void * p0 = uvisor_pool_pointer_to(&pool_queue->pool, 0);
    void * p1 = uvisor_pool_pointer_to(&pool_queue->pool, 1);
    void * p2 = uvisor_pool_pointer_to(&pool_queue->pool, 2);
    void * p3 = uvisor_pool_pointer_to(&pool_queue->pool, 3);
    void * p4 = uvisor_pool_pointer_to(&pool_queue->pool, 4);
    void * p5 = uvisor_pool_pointer_to(&pool_queue->pool, UVISOR_POOL_SLOT_INVALID);

    assert(p0 == &array[0]);
    assert(p1 == &array[1]);
    assert(p2 == &array[2]);
    assert(p3 == &array[3]);
    assert(p4 == NULL);
    assert(p5 == NULL);
}

typedef struct query_context {
    uint32_t * array;
    size_t called_count;
} query_context_t;

static int query_for_deadbeef(uvisor_pool_slot_t slot, void * context)
{
    query_context_t * query_context = context;

    ++query_context->called_count;
    return query_context->array[slot] == 0xDEADBEEF;
}

static void find_first_empty(void)
{
    uvisor_pool_queue_t * pool_queue = get_queue_for_test();
    query_context_t context;
    context.array = array;
    context.called_count = 0;

    uvisor_pool_slot_t slot = uvisor_pool_queue_find_first(pool_queue, query_for_deadbeef, &context);

    assert(context.called_count == 0);
    assert(slot == UVISOR_POOL_SLOT_INVALID);
}

static void find_first_no_match(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();
    query_context_t context;
    context.array = array;
    context.called_count = 0;

    uvisor_pool_slot_t slot = uvisor_pool_queue_find_first(pool_queue, query_for_deadbeef, &context);

    assert(context.called_count == 4);
    assert(slot == UVISOR_POOL_SLOT_INVALID);
}

static void find_first_match_first(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();
    query_context_t context;
    context.array = array;
    context.called_count = 0;

    /* Place the search term early on in the backing array, as the first
     * element in the queue, but after the first slot. This helps to catch
     * "find first" implementations that scan the backing array one-by-one
     * instead of walking the queue. "find first" should find the first
     * in-queue-order match, not the lowest indexed slot that matches. */
    uvisor_pool_queue_dequeue(pool_queue, 0);
    array[0] = 0xDEADBEEF; /* This shouldn't get found, because it has been dequeued. */
    array[1] = 0xDEADBEEF; /* This should get found. */

    uvisor_pool_slot_t slot = uvisor_pool_queue_find_first(pool_queue, query_for_deadbeef, &context);

    assert(context.called_count == 1);
    assert(slot == 1);
}

static void find_first_match_middle(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();
    query_context_t context;
    context.array = array;
    context.called_count = 0;

    /* Place the search term early on in the backing array, as the second
     * element in the queue, after a hole in the queue. This helps to catch
     * "find first" implementations that scan the backing array one-by-one
     * instead of walking the queue. "find first" should find the first
     * in-queue-order match, not the lowest indexed slot that matches. */
    uvisor_pool_queue_dequeue(pool_queue, 1);
    array[0] = 0x00000000; /* This shouldn't get found, because it doesn't match. */
    array[1] = 0xDEADBEEF; /* This shouldn't get found, because it has been dequeued. */
    array[2] = 0xDEADBEEF; /* This should get found. */

    uvisor_pool_slot_t slot = uvisor_pool_queue_find_first(pool_queue, query_for_deadbeef, &context);

    assert(context.called_count == 2);
    assert(slot == 2);
}

static void find_first_match_last(void)
{
    uvisor_pool_queue_t * pool_queue = queue_allocate_4();
    query_context_t context;
    context.array = array;
    context.called_count = 0;

    array[3] = 0xDEADBEEF;

    uvisor_pool_slot_t slot = uvisor_pool_queue_find_first(pool_queue, query_for_deadbeef, &context);

    assert(context.called_count == 4);
    assert(slot == 3);
}

void uvisor_pool_queue_test(void)
{
    queue_sane_after_init();
    queue_allocate_1();
    queue_allocate_2();
    queue_allocate_3();
    queue_allocate_4();
    queue_allocate_5();

    queue_allocate_0_free_too_many();
    queue_allocate_1_free_1();
    queue_allocate_1_double_free();
    queue_allocate_2_free_first();
    queue_allocate_2_free_second();
    queue_allocate_3_free_second();
    queue_allocate_4_free_forward();
    queue_allocate_4_free_backward();

    queue_allocate_4_dequeue_first_many();

    find_first_empty();
    find_first_no_match();
    find_first_match_first();
    find_first_match_middle();
    find_first_match_last();
}

static void pool_sane_after_init(void)
{
    uint8_t pool_mem[sizeof(uvisor_pool_t) + sizeof(uvisor_pool_queue_entry_t) * NUM_ELEMENTS];
    uvisor_pool_t * pool = (uvisor_pool_t *) pool_mem;

    uint32_t array[4];
    static size_t const stride = sizeof(*array);
    static size_t const num = sizeof(array) / sizeof(*array);
    static size_t const blocking = 0;

    /* Fill with crap */
    memset(pool_queue_mem, 0xA5, sizeof(array));

    uvisor_pool_init(pool, array, stride, num, blocking);

    assert(pool->array == array);
    assert(pool->stride == stride);
    assert(pool->num == num);
    assert(pool->num_allocated == 0);
    assert(pool->first_free == 0);

    /* Verify free list goes from the 0th slot to the nth slot in order. */
    assert(pool->management_array[0].dequeued.next == 1);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_1(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == 0);
    assert(pool->num_allocated == 1);

    assert(pool->first_free == 1);
    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_2(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_allocate(pool, timeout_ms);
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == 1);
    assert(pool->num_allocated == 2);

    assert(pool->first_free == 2);
    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_3(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == 2);
    assert(pool->num_allocated == 3);

    assert(pool->first_free == 3);
    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static uvisor_pool_t * pool_allocate_4(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == 3);
    assert(pool->num_allocated == 4);

    assert(pool->first_free == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);

    return pool;
}

static void pool_allocate_5(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == UVISOR_POOL_SLOT_INVALID);
    assert(pool->num_allocated == 4);

    assert(pool->first_free == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
}

static void pool_allocate_0_free_too_many(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;
    uvisor_pool_slot_t freed;

    assert(pool->num_allocated == 0);

    for (slot = 0; slot < 20; slot++) {
        freed = uvisor_pool_free(pool, slot);
        if (slot < pool->num) {
            /* Is already free */
            assert(freed == UVISOR_POOL_SLOT_IS_FREE);
        } else {
            /* Out of range */
            assert(freed == UVISOR_POOL_SLOT_INVALID);
        }
        assert(pool->first_free == 0);
        assert(pool->num_allocated == 0);

        assert(pool->management_array[0].dequeued.next == 1);
        assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
        assert(pool->management_array[1].dequeued.next == 2);
        assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
        assert(pool->management_array[2].dequeued.next == 3);
        assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
        assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
        assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    }
}

static void pool_allocate_1_free_1(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == 0);
    assert(pool->num_allocated == 1);

    uvisor_pool_free(pool, slot);
    assert(pool->first_free == slot);
    assert(pool->num_allocated == 0);

    assert(pool->management_array[0].dequeued.next == 1);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
}

static void pool_allocate_1_double_free(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot;
    uvisor_pool_slot_t again;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot = uvisor_pool_allocate(pool, timeout_ms);

    assert(slot == 0);
    assert(pool->num_allocated == 1);

    uvisor_pool_free(pool, slot);
    again = uvisor_pool_free(pool, slot);

    assert(again == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->first_free == slot);
    assert(pool->num_allocated == 0);

    assert(pool->management_array[0].dequeued.next == 1);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_2_free_first(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot0;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    slot0 = uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);

    assert(pool->num_allocated == 2);

    uvisor_pool_free(pool, slot0);
    assert(pool->first_free == 0);
    assert(pool->num_allocated == 1);

    assert(pool->management_array[0].dequeued.next == 2);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_2_free_second(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot1;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_allocate(pool, timeout_ms);
    slot1 = uvisor_pool_allocate(pool, timeout_ms);

    assert(pool->num_allocated == 2);

    uvisor_pool_free(pool, slot1);
    assert(pool->first_free == 1);
    assert(pool->num_allocated == 1);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_3_free_second(void)
{
    /* Setup */
    uvisor_pool_t * pool = get_pool_for_test();
    uvisor_pool_slot_t slot1;

    assert(pool->num_allocated == 0);
    static const uint32_t timeout_ms = 0;
    uvisor_pool_allocate(pool, timeout_ms);
    slot1 = uvisor_pool_allocate(pool, timeout_ms);
    uvisor_pool_allocate(pool, timeout_ms);

    assert(pool->num_allocated == 3);

    uvisor_pool_free(pool, slot1);
    assert(pool->first_free == 1);
    assert(pool->num_allocated == 2);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == 3);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_4_free_forward(void)
{
    uvisor_pool_t * pool = pool_allocate_4();

    uvisor_pool_free(pool, 0);
    assert(pool->first_free == 0);
    assert(pool->num_allocated == 3);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);

    uvisor_pool_free(pool, 1);
    assert(pool->first_free == 1);
    assert(pool->num_allocated == 2);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 0);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);

    uvisor_pool_free(pool, 2);
    assert(pool->first_free == 2);
    assert(pool->num_allocated == 1);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 0);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 1);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);

    uvisor_pool_free(pool, 3);
    assert(pool->first_free == 3);
    assert(pool->num_allocated == 0);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 0);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 1);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == 2);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

static void pool_allocate_4_free_backward(void)
{
    uvisor_pool_t * pool = pool_allocate_4();

    uvisor_pool_free(pool, 3);
    assert(pool->first_free == 3);
    assert(pool->num_allocated == 3);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);

    uvisor_pool_free(pool, 2);
    assert(pool->first_free == 2);
    assert(pool->num_allocated == 2);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);

    uvisor_pool_free(pool, 1);
    assert(pool->first_free == 1);
    assert(pool->num_allocated == 1);

    assert(pool->management_array[0].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_DEQUEUED);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);

    uvisor_pool_free(pool, 0);
    assert(pool->first_free == 0);
    assert(pool->num_allocated == 0);

    assert(pool->management_array[0].dequeued.next == 1);
    assert(pool->management_array[0].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[1].dequeued.next == 2);
    assert(pool->management_array[1].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[2].dequeued.next == 3);
    assert(pool->management_array[2].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
    assert(pool->management_array[3].dequeued.next == UVISOR_POOL_SLOT_INVALID);
    assert(pool->management_array[3].dequeued.state == UVISOR_POOL_SLOT_IS_FREE);
}

void uvisor_pool_test(void)
{
    pool_sane_after_init();

    pool_allocate_1();
    pool_allocate_2();
    pool_allocate_3();
    pool_allocate_4();
    pool_allocate_5();

    pool_allocate_0_free_too_many();
    pool_allocate_1_free_1();
    pool_allocate_1_double_free();
    pool_allocate_2_free_first();
    pool_allocate_2_free_second();
    pool_allocate_3_free_second();
    pool_allocate_4_free_forward();
    pool_allocate_4_free_backward();

    get_pointer_to_slot();
}

#endif /* NDEBUG */
