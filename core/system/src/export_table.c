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
#include "api/inc/export_table_exports.h"
#include "api/inc/svc_exports.h"

static void boxes_init(void)
{
    /* Tell uVisor to call the uVisor lib box_init function for each box with
     * each box's uVisor lib config. */

    /* This must be called from unprivileged mode in order for the recursive
     * gateway chaining to work properly. */
    UVISOR_SVC(UVISOR_SVC_ID_BOX_INIT_NEXT, "");
}

/* This table must be located at the end of the uVisor binary so that this
 * table can be exported correctly. Placing this table into the .export_table
 * section locates this table at the end of the uVisor binary. */
__attribute__((section(".export_table")))
const TUvisorExportTable __uvisor_export_table = {
    .magic = UVISOR_EXPORT_MAGIC,
    .version = UVISOR_EXPORT_VERSION,
    .os_event_observer = {
        .version = 0,
        .pre_start = boxes_init,
        .thread_create = 0,
        .thread_destroy = 0,
        .thread_switch = 0,
    },
    .size = sizeof(TUvisorExportTable)
};
