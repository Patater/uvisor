#ifndef BOX_THREAD_H
#define BOX_THREAD_H

class BoxThread : public HeapThread {
public:
    BoxThread(void (*task_main)(void const *argument),
              size_t stack_size,
              unsigned char *stack_pointer);

    virtual ~BoxThread();

    static osStatus threads_init();

private:
    BoxThread(const BoxThread &);

    static BoxThread *threads[UVISOR_MAX_BOXES];
    static size_t index;
};

UVISOR_EXTERN osStatus uvisor_box_threads_init(void);

#endif
