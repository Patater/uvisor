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
#include "svc.h"
#include "halt.h"
#include "vmpu.h"
#include "unvic.h"
#include "benchmark.h"
#include "debug.h"

/* these symbols are linked in this scope from the ASM code in __svc_irq and
 * are needed for sanity checks */
UVISOR_EXTERN const uint32_t jump_table_unpriv;
UVISOR_EXTERN const uint32_t jump_table_unpriv_end;
UVISOR_EXTERN const uint32_t jump_table_priv;
UVISOR_EXTERN const uint32_t jump_table_priv_end;

/* default function for not implemented handlers */
void __svc_not_implemented(void)
{
    HALT_ERROR(NOT_IMPLEMENTED, "function not implemented\n\r");
}

/* We can't add the RTX SVC here, because we need to provide a fully linked
 * binary. And we didn't link with RTX yet. We can duplicate the SVC stuff from
 * RTX here, I guess. No, that will need more and more stuff linked until RTX
 * is fully contained within the uVisor binary... Let's add a stupid backdoor
 * to SVC 0 instead. */

void (*stupid_backdoor_f)(void) = __svc_not_implemented;
extern void (*stupid_systick_f)(void);
extern void (*stupid_pendsv_f)(void);

void stupid_backdoor_register(void (*f)(void))
{
    stupid_backdoor_f = f;
}

void stupid_systick_register(void (*f)(void))
{
    stupid_systick_f = f;
}

void stupid_pendsv_register(void (*f)(void))
{
    stupid_pendsv_f = f;
}

/* SVC handlers */
const void *g_svc_vtor_tbl[] = {
    __svc_not_implemented,      //  0
    unvic_isr_set,              //  1
    unvic_isr_get,              //  2
    unvic_irq_enable,           //  3
    unvic_irq_disable,          //  4
    unvic_irq_pending_clr,      //  5
    unvic_irq_pending_set,      //  6
    unvic_irq_pending_get,      //  7
    unvic_irq_priority_set,     //  8
    unvic_irq_priority_get,     //  9
    benchmark_configure,        // 10
    benchmark_start,            // 11
    benchmark_stop,             // 12
    halt_user_error,            // 13
    unvic_irq_level_get,        // 14
    vmpu_box_id_self,           // 15
    vmpu_box_id_caller,         // 16
    vmpu_box_namespace_from_id, // 17
    debug_reboot,               // 18
    /* FIXME: This function will be made automatic when the debug box ACL is
     *        introduced. The initialization will happen at uVisor boot time. */
    debug_register_driver,      // 19
    stupid_backdoor_register,   // 20
    stupid_systick_register,    // 21
    stupid_pendsv_register,     // 22
};

/*******************************************************************************
 *
 * Function name:   __svc_irq
 * Brief:           SVC handlers multiplexer
 *
 * This function is the main SVC IRQ handler. Execution is multiplexed to the
 * proper handler based on the SVC opcode immediate.There are 2 kinds of SVC
 * handler:
 *
 *     1. Regular (unprivileged or privileged)
 * Regular SVC handlers are likely to be mapped to user APIs for unprivileged
 * code. They allow a maximum of 4 32bit arguments and return a single 32bit
 * argument.
 *
 *     2. Secure context (unprivileged) / interrupt (privileged) switch
 * A special SVC handler is given a shortcut to speed up execution. It is used
 * to switch the context between 2 boxes, during normnal execution
 * (unprivileged) or due to an interrupt (privileged). It accepts 4 arguments
 * generated by the asm code below.
 *
 * Note: The implementation of this handler relies on the bit configuration of
 * the 8 bit immediate field of the svc opcode. If this changes (svc_exports.h)
 * then also the handler implementation must change.
 *
 ******************************************************************************/
/* FIXME add register clearing */
/* FIXME add support for floating point in context switches */
void UVISOR_NAKED SVCall_IRQn_Handler(void)
{
    asm volatile(
#if 0
    MRS     R0,PSP                  /* Read PSP */
    LDR     R1,[R0,#24]             /* Read Saved PC from Stack */
    LDRB    R1,[R1,#-2]             /* Load SVC Number */
    CBNZ    R1,SVC_User
#endif

        "tst    lr, #4\n"                   // privileged/unprivileged mode
        "it     eq\n"
        "beq    called_from_priv\n"

    /* the code here serves calls from unprivileged code and is mirrored below
     * for the privileged case; only minor changes exists between the two */
    "called_from_unpriv:\n"
        "mrs    r0, PSP\n"                  // stack pointer
        "ldrt   r1, [r0, #24]\n"            // stacked pc
        "add    r1, r1, #-2\n"              // pc at SVC call
        "ldrbt  r2, [r1]\n"                 // SVC immediate
        "cmp    r2, 0\n"                    // If SVC is 0:
        "itt    eq\n"
        "ldreq  r0, =stupid_backdoor_f\n"   //   Run stupid backdoor
        "ldreq  pc, [r0]\n"
        /***********************************************************************
         *  ATTENTION
         ***********************************************************************
         * the handlers hardcoded in the jump table are called here with 3
         * arguments:
         *    r0 - PSP
         *    r1 - pc of SVCall
         *    r2 - immediate value in SVC opcode
         * these arguments are defined by the asm code you are reading; when
         * changing this code make sure the same format is used or changed
         * accordingly
         **********************************************************************/
        "tst    r2, %[svc_mode_mask]\n"             // Check mode: fast/slow.
        "it     eq\n"
        "beq    custom_table_unpriv\n"
        "and    r3, r2, %[svc_fast_index_mask]\n"   // Isolate index for fast table.
        "adr    r12, jump_table_unpriv\n"           // address of jump table
        "ldr    pc, [r12, r3, lsl #2]\n"            // branch to handler
        ".align 4\n"                                // the jump table must be aligned
    "jump_table_unpriv:\n"
        ".word  unvic_svc_cx_out\n"
        ".word  svc_cx_switch_in\n"
        ".word  svc_cx_switch_out\n"
        ".word  vmpu_register_gateway\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
    "jump_table_unpriv_end:\n"

    ".thumb_func\n"                                 // needed for correct referencing
    "custom_table_unpriv:\n"
        /* there is no need to mask the lower 4 bits of the SVC# because
         * custom_table_unpriv is only when SVC# <= 0x0F */
        "cmp    r2, %[svc_vtor_tbl_count]\n"        // check SVC table overflow
        "ite    ls\n"                               // note: this ITE order speeds it up
        "ldrls  r1, =g_svc_vtor_tbl\n"              // fetch handler from table
        "bxhi   lr\n"                               // abort if overflowing SVC table
        "add    r1, r1, r2, lsl #2\n"               // SVC table offset
        "ldr    r1, [r1]\n"                         // SVC handler
        "push   {lr}\n"                             // save lr for later
        "ldr    lr, =svc_thunk_unpriv\n"            // after handler return to thunk
        "push   {r1}\n"                             // save SVC handler to fetch args
        "ldrt   r3, [r0, #12]\n"                    // fetch args (unprivileged)
        "ldrt   r2, [r0, #8]\n"                     // pass args from stack (unpriv)
        "ldrt   r1, [r0, #4]\n"                     // pass args from stack (unpriv)
        "ldrt   r0, [r0, #0]\n"                     // pass args from stack (unpriv)
        "pop    {pc}\n"                             // execute handler (return to thunk)

    ".thumb_func\n"                                 // needed for correct referencing
    "svc_thunk_unpriv:\n"
        "mrs    r1, PSP\n"                          // unpriv stack may have changed
        "strt   r0, [r1]\n"                         // store result on stacked r0
        "pop    {pc}\n"                             // return from SVCall

    "called_from_priv:\n"
        "mrs    r0, MSP\n"                          // stack pointer
        "ldr    r1, [r0, #24]\n"                    // stacked pc
        "add    r1, r1, #-2\n"                      // pc at SVC call
        "ldrb   r2, [r1]\n"                         // SVC immediate
        "cmp    r2, 0\n"                    // If SVC is 0:
        "itt    eq\n"
        "ldreq  r0, =stupid_backdoor_f\n"   //   Run stupid backdoor
        "ldreq  pc, [r0]\n"
        /***********************************************************************
         *  ATTENTION
         ***********************************************************************
         * the handlers hardcoded in the jump table are called here with 3
         * arguments:
         *    r0 - MSP
         *    r1 - pc of SVCall
         *    r2 - immediate value in SVC opcode
         * these arguments are defined by the asm code you are reading; when
         * changing this code make sure the same format is used or changed
         * accordingly
         **********************************************************************/
        "tst    r2, %[svc_mode_mask]\n"             // Check mode: fast/slow.
        "it     eq\n"
        "beq    custom_table_priv\n"
        "and    r3, r2, %[svc_fast_index_mask]\n"   // Isolate index for fast table.
        "adr    r12, jump_table_priv\n"             // address of jump table
        "ldr    pc, [r12, r3, lsl #2]\n"            // branch to handler
        ".align 4\n"                                // the jump table must be aligned
    "jump_table_priv:\n"
        ".word  unvic_svc_cx_in\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
        ".word  __svc_not_implemented\n"
    "jump_table_priv_end:\n"

    ".thumb_func\n"                                 // needed for correct referencing
    "custom_table_priv:\n"
        /* there is no need to mask the lower 4 bits of the SVC# because
         * custom_table_unpriv is only when SVC# <= 0x0F */
        "cmp    r2, %[svc_vtor_tbl_count]\n"        // check SVC table overflow
        "ite    ls\n"                               // note: this ITE order speeds it up
        "ldrls  r1, =g_svc_vtor_tbl\n"              // fetch handler from table
        "bxhi   lr\n"                               // abort if overflowing SVC table
        "add    r1, r1, r2, lsl #2\n"               // SVC table offset
        "ldr    r1, [r1]\n"                         // SVC handler
        "push   {lr}\n"                             // save lr for later
        "ldr    lr, =svc_thunk_priv\n"              // after handler return to thunk
        "push   {r1}\n"                             // save SVC handler to fetch args
        "ldm    r0, {r0-r3}\n"                      // pass args from stack
        "pop    {pc}\n"                             // execute handler (return to thunk)

    ".thumb_func\n"                                 // needed for correct referencing
    "svc_thunk_priv:\n"
        "str    r0, [sp, #4]\n"                     // store result on stacked r0
        "pop    {pc}\n"                             // return from SVCall

        :: [svc_mode_mask]       "I" ((UVISOR_SVC_MODE_MASK) & 0xFF),
           [svc_fast_index_mask] "I" ((UVISOR_SVC_FAST_INDEX_MASK) & 0xFF),
           [svc_vtor_tbl_count]  "i" (UVISOR_ARRAY_COUNT(g_svc_vtor_tbl) - 1)
    );
}

/*******************************************************************************
 *
 * Function name:   svc_init
 * Brief:           SVC initialization
 *
 ******************************************************************************/
void svc_init(void)
{
    /* sanity checks */
    assert((&jump_table_unpriv_end - &jump_table_unpriv) == UVISOR_SVC_FAST_INDEX_MAX);
    assert((&jump_table_priv_end - &jump_table_priv) == UVISOR_SVC_FAST_INDEX_MAX);
    assert(UVISOR_ARRAY_COUNT(g_svc_vtor_tbl) <= UVISOR_SVC_SLOW_INDEX_MAX);
}
