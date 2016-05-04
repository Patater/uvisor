
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <reent.h>

#include "uvisor-lib/uvisor-lib.h"
#include "cmsis_os.h"

#define DPRINTF(...) {};
/* Use printf with caution inside malloc: printf may allocate memory itself,
   so using printf in malloc may lead to recursive calls! */
/* #define DPRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__) */

extern UvisorBoxIndex *const __uvisor_ps;
extern void uvisor_malloc_init(void);

static int is_kernel_initialized() {
    static uint8_t kernel_running = 0;
    return (kernel_running || (kernel_running = osKernelRunning()));
}

static int init_allocator()
{
    int ret = 0;
#if !(defined(UVISOR_PRESENT) && (UVISOR_PRESENT == 1))
    if (__uvisor_ps == NULL) uvisor_malloc_init();
#else
    if (__uvisor_ps == NULL) return -1;
#endif

    if (__uvisor_ps->mutex_id == NULL && is_kernel_initialized()) {
        memset(__uvisor_ps->mutex_data, 0, sizeof(__uvisor_ps->mutex_data));
        /* point the mutex pointer to the data */
        __uvisor_ps->mutex = &(__uvisor_ps->mutex_data);
        /* create mutex if not already done */
        __uvisor_ps->mutex_id = osMutexCreate((osMutexDef_t *)&(__uvisor_ps->mutex));
        /* mutex failed to be created */
        if (__uvisor_ps->mutex_id == NULL) return -1;
    }

    if (__uvisor_ps->active_heap == NULL) {
        /* we need to initialize the process heap */
        if (__uvisor_ps->process_heap != NULL) {
            /* lock the mutex during initialization */
            if (is_kernel_initialized()) osMutexWait(__uvisor_ps->mutex_id, osWaitForever);
            /* initialize the process heap */
            UvisorAllocator allocator = uvisor_allocator_create_with_pool(
                __uvisor_ps->process_heap,
                __uvisor_ps->process_heap_size);
            /* set the allocator */
            ret = allocator ? 0 : -1;
            __uvisor_ps->active_heap = allocator;
            /* release the mutex */
            if (is_kernel_initialized()) osMutexRelease(__uvisor_ps->mutex_id);
        }
        else {
            DPRINTF("uvisor_allocator: No process heap available!\n");
            ret = -1;
        }
    }
    return ret;
}

/* public API */
UvisorAllocator uvisor_get_allocator(void)
{
    if (init_allocator()) return NULL;
    return __uvisor_ps->active_heap;
}

#define OP_MALLOC  0
#define OP_REALLOC 1
#define OP_FREE    2

static void *memory(void *ptr, size_t size, void **heap, int operation)
{
    /* buffer the return value */
    void *ret = NULL;
    /* initialize allocator */
    if (init_allocator()) return NULL;
    /* check if we need to aquire the mutex */
    int mutexed = (is_kernel_initialized() && (*heap == __uvisor_ps->process_heap));

    /* aquire the mutex if required */
    if (mutexed) osMutexWait(__uvisor_ps->mutex_id, osWaitForever);
    /* perform the required operation */
    switch(operation)
    {
        case OP_MALLOC:
            ret = uvisor_malloc(*heap, size);
            break;
        case OP_REALLOC:
            ret = uvisor_realloc(*heap, ptr, size);
            break;
        case OP_FREE:
            uvisor_free(*heap, ptr);
            break;
        default:
            break;
    }
    /* release the mutex if required */
    if (mutexed) osMutexRelease(__uvisor_ps->mutex_id);
    return ret;
}

/* wrapped memory management functions */
#if defined (__CC_ARM)
void *$Sub$$_malloc_r(struct _reent*r, size_t size) {
    return memory(r, size, &(__uvisor_ps->active_heap), OP_MALLOC);
}
void *$Sub$$_realloc_r(struct _reent*r, void *ptr, size_t size) {
    (void)r;
    return memory(ptr, size, &(__uvisor_ps->active_heap), OP_REALLOC);
}
void $Sub$$_free_r(struct _reent*r, void *ptr) {
    (void)r;
    memory(ptr, 0, &(__uvisor_ps->active_heap), OP_FREE);
}
#elif defined (__GNUC__)
void *__wrap__malloc_r(struct _reent*r, size_t size) {
    return memory(r, size, &(__uvisor_ps->active_heap), OP_MALLOC);
}
void *__wrap__realloc_r(struct _reent*r, void *ptr, size_t size) {
    (void)r;
    return memory(ptr, size, &(__uvisor_ps->active_heap), OP_REALLOC);
}
void __wrap__free_r(struct _reent*r, void *ptr) {
    (void)r;
    memory(ptr, 0, &(__uvisor_ps->active_heap), OP_FREE);
}
#elif defined (__ICCARM__)
#   warning "Using uVisor allocator is not available for IARCC. Falling back to newlib allocator."
#endif
