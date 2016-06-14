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
#include "context.h"
#include "halt.h"
#include "linker.h"
#include "vmpu.h"

/** Flag used to remember whether the box initialization has happened
 * @internal
 */
static bool g_box_init_happened;

/** Counter of boxes that have already been initialized
 * @internal
 */
static uint8_t g_box_init_counter;

/** Cached stack pointer of the first SVCall in the initialization chain
 * @internal
 * The stack pointer of box 0 is cached so we do not need to re-compute it and
 * re-verify it at every step in the chain.
 */
static uint32_t g_box_init_box0_sp;

/** Thunk function for the box init initialization
 * @internal
 * Since the box initialization is implemented as a recursive gateway that
 * iterates over all the secure boxes, this thunk is the same SVCall as the
 * actual API that the user calls from uvisor-lib. The actual SVCall handler
 * will check at which stage of the recursion we are.
 */
static void UVISOR_NAKED box_init_thunk(void)
{
    UVISOR_SVC(UVISOR_SVC_ID_BOX_INIT_NEXT, "");
}

/** Initialize all boxes that require it by running the default box init.
 *  handler.
 * @internal
 * This function is implemented as a wrapper, needed to make sure that the lr
 * register doesn't get polluted. The actual function is \ref
 * box_init_context_switch_next.
 */
void UVISOR_NAKED box_init_next(uint32_t src_svc_sp)
{
    /* According to the ARM ABI, r0 will have the following value when this
     * function is called:
     *   r0 = src_svc_sp */
    asm volatile (
        "ldr    r1, =g_active_box\n"            /* active box_p = &g_active_box */
        "ldr    r1, [r1]\n"                     /* active_box = *active_box_p; */
        "cmp    r1, 0\n"                        /* if (active_box == 0) { */
        "it     eq\n"                           /*     Restore the callee-saved registers from the MSP (privileged). */
        "pusheq {r4 - r11}\n"                   /* } */
        "push   {lr}\n"                         /* Preserve the lr register. */
        "bl     box_init_context_switch_next\n" /* box_init_context_switch_next(src_svc_sp) */
        "pop    {lr}\n"                         /* Restore the lr register. */
        "ldr    r1, =g_active_box\n"            /* active box_p = &g_active_box */
        "ldr    r1, [r1]\n"                     /* active_box = *active_box_p; */
        "cmp    r1, 0\n"                        /* if (active_box == 0) { */
        "it     eq\n"                           /*     Restore the callee-saved registers from the MSP (privileged). */
        "pusheq {r4 - r11}\n"                   /* } */
        "bx     lr\n"                           /* Return. */
        /* The box init handler will be executed after this. */
        :: "r" (src_svc_sp)
    );
}

/** Perform a chain of context switches to initialize all secure boxes.
 * @internal
 * This function implements a recursive chain of context switches, where all
 * destination contexts execute the same default handler, registered in flash.
 * The default handler receives only one argument from the uVisor, which is the
 * \ref UvisorBoxConfig.custom_config pointer. The default handler can parse the
 * pointed-to data the way it wants; it is implementation-specific.
 * @note This function can only be run once.
 * @note Only boxes different from box 0 can be initialized. If an
 * initialization handler is provided for box 0 it will be ignored.
 * @param src_svc_sp[in]    Unprivileged stack pointer at the time of the
 *                          SVCall
 */
void box_init_context_switch_next(uint32_t src_svc_sp)
{
    /* Check if this is the first time the box initialization procedure is
     * requested. */
    if (g_box_init_happened) {
        HALT_ERROR(NOT_ALLOWED, "The box initialization routine can only be executed once.");
    }

    /* If there is only one box (which is box 0) return immediately. Box 0 is
     * not allowed to have a box initialization function. */
    if (g_vmpu_box_count == 1) {
        g_box_init_happened = true;
        return;
    }

    /* If the currently active box is not the one we expected from our running
     * counter, then the recursive chain of context switches has been broken. */
    if (g_active_box != g_box_init_counter) {
        HALT_ERROR(NOT_ALLOWED, "The current box is inconsistent with the box initialization order.");
    }

    /* Get the handler from the table of exported handlers. */
    uint32_t const dst_fn = (uint32_t) __uvisor_config.lib_box_init;
    if (dst_fn == 0) {
        HALT_ERROR(NOT_ALLOWED, "The box initialization procedure requires an unprivileged handler to be specified.");
    }
    if (!vmpu_public_flash_addr(dst_fn)) {
        HALT_ERROR(PERMISSION_DENIED, "The box initialization handler must be in public flash.");
    }

    /* Find the next box to switch to, if any.
     * The box needs to have a box-specific configuration pointer. If it does
     * not, the box is skipped. */
    uint8_t dst_id = g_box_init_counter + 1;
    UvisorBoxConfig const * * box_cfg = (UvisorBoxConfig const * *) __uvisor_config.cfgtbl_ptr_start;
    const void * lib_config = NULL;
    while (dst_id < g_vmpu_box_count) {
        lib_config = box_cfg[dst_id]->lib_config;
        if (lib_config != NULL) {
            break;
        } else {
            dst_id++;
        }
    }

    /* The stack pointer provided to us as an input comes from an SVC
     * exception. We must check that it corresponds to a full frame that the
     * source box can access. */
    /* This function halts if it finds an error. */
    uint32_t src_sp = context_validate_exc_sf(src_svc_sp);
    if (g_box_init_counter == 0) {
        g_box_init_box0_sp = src_sp;
    } else {
        /* If this is not the first context switch in the chain, then we first
         * need to return from the previous one. */
        context_discard_exc_sf(g_active_box, src_sp);
        context_switch_out(CONTEXT_SWITCH_FUNCTION_GATEWAY);
    }

    /* Recursion termination condition.
     * If all boxes have been initialized, we can perform the very last context
     * switch-out and return to the original caller. */
    if (dst_id >= g_vmpu_box_count) {
        g_box_init_happened = true;
        return;
    }

    /* If the code gets to this point, then we still have one or more boxes to
     * initialize. In addition, we can assume that the box-specific
     * configuration pointer is not NULL. */

    /* The values of dst_id and box_cfg after the loop above represent the box
     * to initialize. */
    g_box_init_counter = dst_id;

    /* Forge a stack frame for the destination box. */
    uint32_t xpsr = ((uint32_t *) g_box_init_box0_sp)[7];
    uint32_t dst_sp = context_forge_exc_sf(g_box_init_box0_sp, dst_id, dst_fn, (uint32_t) box_init_thunk, xpsr, 0);

    /* We pass the custom box configuration pointer to the default box
     * initialization handler. */
    if (!vmpu_public_flash_addr((uint32_t) lib_config)) {
        HALT_ERROR(PERMISSION_DENIED, "The custom box configuration must point to data in flash.");
    }
    ((uint32_t *) dst_sp)[0] = (uint32_t) lib_config;

    /* Perform the context switch to the destination box.
     * This context switch will update the internal context state, so that the
     * destination handler can be pre-empted if needed. */
    /* This function halts if it finds an error. */
    context_switch_in(CONTEXT_SWITCH_FUNCTION_GATEWAY, dst_id, g_box_init_box0_sp, dst_sp);
}
