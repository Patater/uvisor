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

#ifndef __PAGE_ALLOCATOR_CONFIG_H__
#define __PAGE_ALLOCATOR_CONFIG_H__
/* This file can be compiled externally to provide the page allocator algorithm
 * for devices NOT supported by uVisor. For this purpose this file is copied as
 * is into the target build folder and compiled by the target build system. */

/* We can only protect a small number of pages efficiently, so there should be
 * a relatively low limit to the number of pages.
 * By default a maximum of 16 pages are allowed. This can only be overwritten
 * by the porting engineer for the current platform. */
#ifndef UVISOR_PAGE_MAX_COUNT
#define UVISOR_PAGE_MAX_COUNT ((uint32_t) 16)
#endif
/* The number of pages is decided by the page size. A small page size leads to
 * a lot of pages, however, number of pages is capped for efficiency.
 * Furthermore, when allocating large continous memory, a too small page size
 * will lead to allocation failures. This can only be overwritten
 * by the porting engineer for the current platform. */
#ifndef UVISOR_PAGE_SIZE_MINIMUM
#define UVISOR_PAGE_SIZE_MINIMUM ((uint32_t) 1024)
#endif

/* The page box_id is the box id which is 8-bit large. */
typedef uint8_t page_owner_t;
/* Define a unused value for the page table. */
#define UVISOR_PAGE_UNUSED ((page_owner_t) (-1))

/** Sets the page bit in the page map array.
 * @param map   an array of `uint32_t` containing the page map
 * @param page  the index of the page to be set
 */
static inline void page_allocator_map_set(uint32_t * const map, uint8_t page)
{
    map[page / 32] |= (1UL << (page % 32));
}

/** Clears the page bit in the page map array.
 * @param map   an array of `uint32_t` containing the page map
 * @param page  the index of the page to be set
 */
static inline void page_allocator_map_clear(uint32_t * const map, uint8_t page)
{
    map[page / 32] &= ~(1UL << (page % 32));
}

/** Check if the page bit is set int the page map array.
 * @param map   an array of `uint32_t` containing the page map
 * @param page  the index of the page to be set
 * @retval 0    if page bit is not set
 * @retval !0   if page bit is set
 */
static inline uint32_t page_allocator_map_get(const uint32_t * const map, uint8_t page)
{
    return (map[page / 32] & (1UL << (page % 32)));
}

#endif /* __PAGE_ALLOCATOR_CONFIG_H__ */
