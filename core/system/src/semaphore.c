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
#include "semaphore.h"

int semaphore_init(UvisorSemaphore * semaphore, int32_t count)
{
    /* TODO If privileged mode, die because uVisor programmer error */
    //HALT_ERROR(NOT_ALLOWED, "Semaphores must be initialized from outside of uVisor.");
    return __uvisor_config.lib_hooks->semaphore_init(semaphore, count);
}

int semaphore_pend(UvisorSemaphore * semaphore, uint32_t timeout_ms)
{
    /* TODO If privileged mode, die because uVisor programmer error*/
    //HALT_ERROR(NOT_ALLOWED, "Semaphores can't be pended upon from inside uVisor.");
    return __uvisor_config.lib_hooks->semaphore_pend(semaphore, timeout_ms);
}

int semaphore_post(UvisorSemaphore * semaphore) {
/* XXX Cant' use the RAM-located hook for this, because this code will also be
 * called unpriv. We can add an if statement here, or call the flash based one
 * */
    return __uvisor_config.priv_sys_hooks->priv_uvisor_semaphore_post(semaphore);
    //return g_priv_sys_hooks.priv_uvisor_semaphore_post(semaphore); // Nope
}
