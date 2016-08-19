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
#include "api/inc/thread_context.h"
#include "rt_TypeDef.h"
#include "rt_Task.h"

UvisorThreadContext * thread_context(void)
{
    extern P_TCB os_active_TCB;
    OS_TID tsk;
    tsk = rt_tsk_self();
    return (UvisorThreadContext *) os_active_TCB[tsk - 1U].context;
}
