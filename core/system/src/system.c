/*
 * Copyright (c) 2013-2016, ARM Limited, All Rights Reserved
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

/* All system IRQs are by default weakly linked to the system default handler */
void UVISOR_ALIAS(isr_default_sys_handler) NonMaskableInt_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) HardFault_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) MemoryManagement_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) BusFault_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) UsageFault_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) SVCall_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) DebugMonitor_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) PendSV_IRQn_Handler(void);
void UVISOR_ALIAS(isr_default_sys_handler) SysTick_IRQn_Handler(void);


void (*stupid_systick_f)(void) = isr_default_sys_handler;
void (*stupid_pendsv_f)(void) = isr_default_sys_handler;

void stupid_systick(void)
{
    stupid_systick_f();
}

void stupid_pendsv(void)
{
    stupid_pendsv_f();
}

/* Default vector table (placed in Flash) */
__attribute__((section(".isr"))) const TIsrVector g_isr_vector[ISR_VECTORS] =
{
    /* Initial stack pointer */
    (TIsrVector) &__stack_top__,

    /* System IRQs */
    &main_entry,                           /* -15 */
    NonMaskableInt_IRQn_Handler,           /* -14 */
    HardFault_IRQn_Handler,                /* -13 */
    MemoryManagement_IRQn_Handler,         /* -12 */
    BusFault_IRQn_Handler,                 /* -11 */
    UsageFault_IRQn_Handler,               /* -10 */
    isr_default_sys_handler,               /* - 9 */
    isr_default_sys_handler,               /* - 8 */
    isr_default_sys_handler,               /* - 7 */
    isr_default_sys_handler,               /* - 6 */
    SVCall_IRQn_Handler,                   /* - 5 */
    DebugMonitor_IRQn_Handler,             /* - 4 */
    isr_default_sys_handler,               /* - 3 */
    stupid_pendsv,                         /* - 2 */
    stupid_systick,                        /* - 1 */

    /* NVIC IRQs */
    /* Note: This is a GCC extension. */
    [NVIC_OFFSET ... (ISR_VECTORS - 1)] = isr_default_handler
};

void UVISOR_NAKED UVISOR_NORETURN isr_default_sys_handler(void)
{
    /* Handle system IRQ with an unprivileged handler. */
    /* XXX Could we have registered an unprivileged SysTick handler??? Would
     * have been convenient for RTX, if so. vmpu_sys_mux_handler doesn't seem
     * to handle PendSV or SysTick... */
    /* Note: The original lr and the MSP are passed to the actual handler */
    asm volatile(
        "mov r0, lr\n"
        "mrs r1, MSP\n"
        "push {lr}\n"
        "blx vmpu_sys_mux_handler\n"
        "pop {pc}\n"
    );
}

void UVISOR_NAKED UVISOR_NORETURN isr_default_handler(void)
{
    /* Handle NVIC IRQ with an unprivileged handler.
     * Serving an IRQn in unprivileged mode is achieved by mean of two SVCalls:
     * The first one de-previliges execution, the second one re-privileges it. */
    /* Note: NONBASETHRDENA (in SCB) must be set to 1 for this to work. */
    asm volatile(
        "svc  %[unvic_in]\n"
        "svc  %[unvic_out]\n"
        "bx   lr\n"
        ::[unvic_in]  "i" ((UVISOR_SVC_ID_UNVIC_IN) & 0xFF),
          [unvic_out] "i" ((UVISOR_SVC_ID_UNVIC_OUT) & 0xFF)
    );
}

typedef void (*TPrivcall)(void*);

/* XXX This only works when thread IDs are compact in representation and are
 * not a 32-bit hash or pointer. */
static uint8_t thread_to_box_map[UVISOR_MAX_THREADS];

static void privcall_thread_alloc(void *ctx)
{
    uint32_t thread_id = (uint32_t)ctx;

    DPRINTF("PRIVCALL new thread %d in box %d\r\n", thread_id, g_active_box);

    /* Allocate resources in uVisor for thread */
    /* Maybe can use our Tier-1 allocator? It's per-process and would prevent a
     * rougue process from stealing the ability to create more threads from
     * other processes when memory runs out. */

    /* Record which box_id this thread is for, so we can know when to change
     * the MPU context. */
    thread_to_box_map[thread_id] = g_active_box;
}

static void privcall_thread_free(void *ctx)
{
    uint32_t thread_id = (uint32_t)ctx;

    /* Free resources in uVisor for thread */
    (void)thread_id;
}

static void privcall_thread_switch(void *ctx)
{
    uint32_t thread_id = (uint32_t)ctx;

    /* Switch to resources in uVisor for thread */

    /* There is one of these for every thread. There is also one of these per
     * box, for handling interrupts. */
    g_svc_cx_current_tid = thread_id;

    uint8_t dst_box = thread_to_box_map[g_svc_cx_current_tid];

    DPRINTF("PRIVCALL switching to thread %d in box %d from box %d\r\n", thread_id, dst_box, g_active_box);

    if (g_active_box != dst_box)
    {
        /* Switch to new MPU context. */
        vmpu_switch(g_active_box, dst_box);
    }
}

static void privcall_version(void *ctx)
{
    uint32_t *version = (uint32_t *)ctx;

    *version = 0;
}

static const TPrivcall privcalls[] =
{
    privcall_version,       // 0
    privcall_thread_alloc,  // 1
    privcall_thread_free,   // 2
    privcall_thread_switch, // 3
};

void privcall_dispatch(size_t num, void *ctx)
{
    privcalls[num](ctx);
}
