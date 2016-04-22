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
#include "cmsis_os.h"
#include <stdint.h>
#include <stdlib.h> // malloc

/* Symbols exported by the mbed linker script */
UVISOR_EXTERN uint32_t __uvisor_cfgtbl_ptr_start;
UVISOR_EXTERN uint32_t __uvisor_cfgtbl_ptr_end;

/* XXX TODO We should be privileged when we switch between boxes and create
 * threads. */
/* XXX TODO Add a privcall for thread_create that accepts a parameter that
 * specifies a process as owner. Use that to create the new box main threads.
 * Or maybe we can use thread_switch with some bullshit structure and cause a
 * process switch. Or maybe we can tell based on the context pointer given to
 * thread_create which process the thread should be a part of.
 *  Problem:
 *    We want to create a thread from priv code that goes in a specific box.
 *  Solution:
 *    We can use the thread create function which makes threads in
 *    g_active_box, after having influenced the g_active_box in some way to set
 *    it to the specific box. Or, we can make a new function that doesn't use
 *    g_active_box when creating threads.
 *
 *  Problem:
 *    We are adding new API stuffs to read from box configs. Why not just allow
 *    reading from box config directly? Does this couple uvisor-lib too closely
 *    with uvisor? Privileged code, like the RTOS, should be able to read from
 *    box config already. How does it access the memory for box configs? How
 *    does it know where to look?
 */
/* This must be called from privileged code: it reads the uVisor cfgtbl (which
 * is XXX rumored to be protected from reading) and also creates threads in
 * different box contexts than the current box context. This function assumes
 * that no other thread creation will be going on in parallel (that could screw
 * up the thread creation context state).
 */
int box_mains_start(void (* set_thread_creation_context)(int box_id))
{
    int32_t box_id;
    const UvisorBoxConfig ** box_cfgtbl = (const UvisorBoxConfig **)&__uvisor_cfgtbl_ptr_start;

    // XXX TODO Assert that if uVisor is enabled, set_thread_creation_context
    // is non-0. It'd be programmer error if it weren't set, as that'd mean the
    // box main threads wouldn't be created in the proper context.

    box_id = 0;
    for(box_cfgtbl = (const UvisorBoxConfig**) &__uvisor_cfgtbl_ptr_start;
            box_cfgtbl < (const UvisorBoxConfig**) &__uvisor_cfgtbl_ptr_end;
            box_cfgtbl++) {
        const UvisorBoxConfig * box_cfg = *box_cfgtbl;
        // XXX Consider checking for NULL box_cfgtbl here. Shouldn't happen,
        // but could check, I guess. */
        if (box_cfg->main_function != NULL) {
            osThreadDef_t thread_def;
            thread_def.pthread = box_cfg->main_function;
            thread_def.tpriority = box_cfg->main_priority;
            thread_def.stacksize = 1024; //box_cfg->main_stack_size; // XXX TODO
                                                                  // Decide how we want to come up with a stack size here. Probably
                                                                  // needs to use box-specific allocator and not auto-allocate in box 0.
            thread_def.stack_pointer = malloc(thread_def.stacksize); /* XXX TODO Why would we want to
            give g_svc_cx_curr_sp[box_id] here? Did we really want to use the
            process stack for the main thread stack? XXX Memory leak is here,
            but only for main boxes, so nobody cares really. XXX Note that this
            is insecure, as this malloc will use globally writable box 0 heap
            for allocating main thread stacks. */
            if (thread_def.stack_pointer == NULL)
            {
                return -1; /* XXX Use nice Out of memory error here */
            }

            /* Set the thread creation context so that all new threads are
             * created in the box that we want to make the main thread for. */
            if (set_thread_creation_context) {
                /* NOTE: This function will fail with a bus fault if we aren't
                 * privileged. */
                set_thread_creation_context(box_id);
            }

            /* XXX FYI, it is bad practice to call os functions before the os
             * has been initialized. uvisor_lib_init is called before the os
             * has been initialized because we want to make sure the proper
             * cmsis drivers (like thread observer) are in place in case they
             * are needed during OS initialization. We may need to make a
             * uvisor_lib_post_init that is called before the kernel starts
             * (but after it has been initialized).*/

            /* Also, apparently osThreadCreate can't be called from inside an
             * ISR. And I guess we are in some sort of ISR now, as IPSR is
             * 0xB. */
            //osThreadCreate(&thread_def, NULL); XXX
            extern osThreadId   svcThreadCreate  (const osThreadDef_t *thread_def, void *argument, void *context);
            svcThreadCreate(&thread_def, NULL, NULL);
        }

        box_id++;
    }

    /* Reset the thread creation context so that all new threads are created in
     * the g_active_box. */
    if (set_thread_creation_context) {
        set_thread_creation_context(-1);
    }

    return 0;
}
