#include "BoxThread.h"

static size_t BoxThread::index = 0;

BoxThread::BoxThread(void (*task_main)(void const *argument),
                     size_t stack_size,
                     unsigned char *stack_pointer) :
           HeapThread(task_main, 0, 0, stack_size, stack_pointer)
{
    if (index >= UVISOR_MAX_BOXES)
    {
        /* Don't save the box thread in the list of box threads, because we ran
         * out of room. */
        /* XXX TODO Assert Failure */
    }

    threads[index] = this;
    ++index;
}

osStatus BoxThread::threads_init()
{
    for (size_t i = 0; i < sizeof(threads); ++i) {
        if (threads[index]) {
            osStatus status = threads[index]->start(NULL);
            if (status) return status;
        }
    }

    return 0;
}

UVISOR_EXTERN osStatus uvisor_box_init(void)
{
    return BoxThread::threads_init();
}
