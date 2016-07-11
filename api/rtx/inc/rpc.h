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
#ifndef __UVISOR_API_RPC_H__
#define __UVISOR_API_RPC_H__

#include "cmsis_os.h"

/* XXX When should this be rpc_exports.h and when should it be just rpc.h? */

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(*x))

/* <http://stackoverflow.com/questions/3385515/static-assert-in-c> */
#define CTASTR2(pre, post) pre ## post
#define CTASTR(pre, post) CTASTR2(pre, post)
#define STATIC_ASSERT(cond, msg) \
    typedef struct { int CTASTR(static_assertion_failed_, msg) : !!(cond); } \
            CTASTR(static_assertion_failed_, __COUNTER__)

/* This is the size (in bytes) of the internal queue type. */
#ifdef CMSIS_OS_RTX
#define RESULT_QUEUE_SIZE 38
#define CALLEE_QUEUE_SIZE 24
#else
#error "Unknown how big a the RPC queues should be for this OS"
#endif

/* XXX Move to Box Cfg */
#define UVISOR_BOX(box_name) (&box_name##_cfg_ptr)

/* udf imm16 */
/* UDF ARMv7M ARM A7.7.191 */
/* 111 1;0 111;1111; <imm4>; 1 01 0; <imm12> (Encoding T2) */
#define UDF_OPCODE(imm16) \
    ((uint32_t) (0xA000F7F0U | (((uint32_t) (imm16) & 0xFFFU) << 16U) | (((uint32_t) (imm16) & 0xF000U) >> 12U)))

#if defined(__thumb__) && defined(__thumb2__)
/* The magics are chosen to be one of the explicitly undefined instructions. */
#define UVISOR_RPC_GATEWAY_MAGIC_ASYNC UDF_OPCODE(0x07C2)
#define UVISOR_RPC_GATEWAY_MAGIC_SYNC  UDF_OPCODE(0x07C3)
#else
#error "Unsupported instruction set. The ARM Thumb-2 instruction set must be supported."
#endif /* __thumb__ && __thumb2__ */

/* ldr pc, [pc, #<label - instr + 4>] */
/* LDR (immediate) ARMv7M ARM A7.7.42 */
/* 1111;1 00 0; 0 10 1; <Rn - 1111>; <Rt - 1111>; <imm12> (Encoding T3) */
#define LDR_PC_PC_IMM_OPCODE(instr, label) \
    ((uint32_t) (0xF000F8DFU | ((((uint32_t) (label) - ((uint32_t) (instr) + 4U)) & 0xFFFU) << 16U)))

/** Synchronous RPC Gateway
 *
 * This macro declares a new function pointer (with no name mangling) named
 * `gw_name` to perform a remote procedure call (RPC) to the target function
 * given by `fn_name`. RPCs are assembled into a read-only flash structure that
 * is read and validated by uVisor before performing the operation.
 *
 * Create function with following signature:
 * UVISOR_EXTERN fn_type fn_name(uint32_t a, uint32_t b);
 *
 * @param box_name[in] The name of the source box as decalred in
 *                     `UVISOR_BOX_CONFIG`
 * @param gw_name[in]  The new function pointer for performing RPC
 * @param fn_name[in]  The function being designated as an RPC target
 * @param fn_type[in]  The type of the function being designated as an RPC
 *                     target
 */
#define UVISOR_BOX_RPC_GATEWAY_SYNC(box_name, gw_name, fn_name, fn_type, ...) \
    STATIC_ASSERT(sizeof(fn_type) <= sizeof(uint32_t), gw_name##_return_type_too_big); \
    UVISOR_BOX_RPC_GATEWAY_ARG_CHECK(gw_name, __VA_ARGS__) \
    UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER(fn_name, __VA_ARGS__) \
    /* Instanstiate the gateway. This gets resolved at link-time. */ \
    UVISOR_EXTERN TRPCGateway const gw_name##_rpc_gateway = { \
        .ldr_pc   = LDR_PC_PC_IMM_OPCODE(__UVISOR_OFFSETOF(TRPCGateway, ldr_pc), \
                                         __UVISOR_OFFSETOF(TRPCGateway, function)), \
        .magic    = UVISOR_RPC_GATEWAY_MAGIC_SYNC, \
        .box_ptr  = (uint32_t) UVISOR_BOX(box_name), \
        .function = (uint32_t) __sgw_sync_##fn_name, \
    }; \
    \
    /* Pointer to the gateway we just created. The pointer is located in a
     * discoverable linker section. */ \
    __attribute__((section(".keep.uvisor.rpc_gateway_ptr"))) \
    static uint32_t const gw_name##_rpc_gateway_ptr = (uint32_t) &gw_name##_rpc_gateway; \
    \
    /* Declare the actual gateway. */ \
    UVISOR_EXTERN fn_type (*gw_name)(__VA_ARGS__) __attribute__((alias(UVISOR_TO_STRING(gw_name##_rpc_gateway))));

/** Asynchronous RPC Gateway
 *
 * This macro declares a new function pointer (with no name mangling) named
 * `gw_name` to perform a remote procedure call (RPC) to the target function
 * given by `fn_name`. RPCs are assembled into a read-only flash structure that
 * is read and validated by uVisor before performing the operation.
 *
 * Create function with following signature:
 * UVISOR_EXTERN int gw_name(uvisor_rpc_result_t *, uint32_t a, uint32_t b);
 *
 * @param box_name[in] The name of the source box as decalred in
 *                     `UVISOR_BOX_CONFIG`
 * @param gw_name[in]  The new function pointer for performing RPC
 * @param fn_name[in]  The function being designated as an RPC target
 * @param fn_type[in]  The type of the function being designated as an RPC
 *                     target
 */
#define UVISOR_BOX_RPC_GATEWAY_ASYNC(box_name, gw_name, fn_name, fn_type, ...) \
    STATIC_ASSERT(sizeof(fn_type) <= sizeof(uint32_t), gw_name##_return_type_too_big); \
    UVISOR_BOX_RPC_GATEWAY_ARG_CHECK(gw_name, __VA_ARGS__) \
    UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER(fn_name, __VA_ARGS__) \
    /* Instanstiate the gateway. This gets resolved at link-time. */ \
    UVISOR_EXTERN TRPCGateway const gw_name##_rpc_gateway = { \
        .ldr_pc   = LDR_PC_PC_IMM_OPCODE(__UVISOR_OFFSETOF(TRPCGateway, ldr_pc), \
                                         __UVISOR_OFFSETOF(TRPCGateway, function)), \
        .magic    = UVISOR_RPC_GATEWAY_MAGIC_ASYNC, \
        .box_ptr  = (uint32_t) UVISOR_BOX(box_name), \
        .function = (uint32_t) _sgw_async_##fn_name, \
    }; \
    \
    /* Pointer to the gateway we just created. The pointer is located in a
     * discoverable linker section. */ \
    __attribute__((section(".keep.uvisor.rpc_gateway_ptr"))) \
    static uint32_t const gw_name##_rpc_gateway_ptr = (uint32_t) &gw_name##_rpc_gateway; \
    \
    /* Declare the actual gateway. */ \
    UVISOR_EXTERN int (*gw_name)(uvisor_rpc_result_t *, __VA_ARGS__) __attribute__((alias(UVISOR_TO_STRING(gw_name##_rpc_gateway))));

#define UVISOR_BOX_RPC_GATEWAY_ARG_CHECK(gw_name, ...) \
    __UVISOR_BOX_MACRO(__VA_ARGS__, UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_4, \
                                    UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_3, \
                                    UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_2, \
                                    UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_1, \
                                    UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_0)(gw_name, __VA_ARGS__)

#define UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_0(gw_name)

#define UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_1(gw_name, p0_type) \
    STATIC_ASSERT(sizeof(p0_type) <= sizeof(uint32_t), gw_name##_param_0_too_big);

#define UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_2(gw_name, p0_type, p1_type) \
    STATIC_ASSERT(sizeof(p0_type) <= sizeof(uint32_t), gw_name##_param_0_too_big); \
    STATIC_ASSERT(sizeof(p1_type) <= sizeof(uint32_t), gw_name##_param_1_too_big);

#define UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_3(gw_name, p0_type, p1_type, p2_type) \
    STATIC_ASSERT(sizeof(p0_type) <= sizeof(uint32_t), gw_name##_param_0_too_big); \
    STATIC_ASSERT(sizeof(p1_type) <= sizeof(uint32_t), gw_name##_param_1_too_big); \
    STATIC_ASSERT(sizeof(p2_type) <= sizeof(uint32_t), gw_name##_param_2_too_big);

#define UVISOR_BOX_RPC_GATEWAY_ARG_CHECK_4(gw_name, p0_type, p1_type, p2_type, p3_type) \
    STATIC_ASSERT(sizeof(p0_type) <= sizeof(uint32_t), gw_name##_param_0_too_big); \
    STATIC_ASSERT(sizeof(p1_type) <= sizeof(uint32_t), gw_name##_param_1_too_big); \
    STATIC_ASSERT(sizeof(p2_type) <= sizeof(uint32_t), gw_name##_param_2_too_big); \
    STATIC_ASSERT(sizeof(p3_type) <= sizeof(uint32_t), gw_name##_param_3_too_big);

#define UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER(fn_name, ...) \
    __UVISOR_BOX_MACRO(__VA_ARGS__, UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_4, \
                                    UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_3, \
                                    UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_2, \
                                    UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_1, \
                                    UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_0)(fn_name, __VA_ARGS__)

#define UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_0(fn_name, ...) \
    static uint32_t __sgw_sync_##fn_name(void) \
    { \
        /* XXX Note that p0,p1,p2,p3 may have garbage in them if not all params
         * are specified by the fn_params. */ \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall(0, 0, 0, 0, fp); \
    }

#define UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_1(fn_name, ...) \
    static uint32_t __sgw_sync_##fn_name(uint32_t p0) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall(p0, 0, 0, 0, fp); \
    }

#define UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_2(fn_name, ...) \
    static uint32_t __sgw_sync_##fn_name(uint32_t p0, uint32_t p1) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall(p0, p1, 0, 0, fp); \
    }

#define UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_3(fn_name, ...) \
    static uint32_t __sgw_sync_##fn_name(uint32_t p0, uint32_t p1, uint32_t p2) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall(p0, p1, p2, 0, fp); \
    }

#define UVISOR_BOX_RPC_GATEWAY_SYNC_CALLER_4(fn_name, ...) \
    static uint32_t __sgw_sync_##fn_name(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall(p0, p1, p2, p3, fp); \
    }

#define UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER(fn_name, ...) \
    __UVISOR_BOX_MACRO(__VA_ARGS__, UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_4, \
                                    UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_3, \
                                    UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_2, \
                                    UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_1, \
                                    UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_0)(fn_name, __VA_ARGS__)

#define UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_0(fn_name, ...) \
    static uint32_t _sgw_async_##fn_name(uvisor_rpc_result_t * result) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall_async(0, 0, 0, 0, fp, result); \
    }

#define UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_1(fn_name, ...) \
    static uint32_t _sgw_async_##fn_name(uvisor_rpc_result_t * result, uint32_t p0) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall_async(p0, 0, 0, 0, fp, result); \
    }

#define UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_2(fn_name, ...) \
    static uint32_t _sgw_async_##fn_name(uvisor_rpc_result_t * result, uint32_t p0, uint32_t p1) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall_async(p0, p1, 0, 0, fp, result); \
    }

#define UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_3(fn_name, ...) \
    static uint32_t _sgw_async_##fn_name(uvisor_rpc_result_t * result, uint32_t p0, uint32_t p1, uint32_t p2) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall_async(p0, p1, p2, 0, fp, result); \
    }

#define UVISOR_BOX_RPC_GATEWAY_ASYNC_CALLER_4(fn_name, ...) \
    static uint32_t _sgw_async_##fn_name(uvisor_rpc_result_t * result, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3) \
    { \
        TFN_Ptr fp = (TFN_Ptr) fn_name; \
        return rpc_fncall_async(p0, p1, p2, p3, fp, result); \
    }

/** RPC gateway structure
 *
 * This struct is packed because we must ensure that the `ldr_pc` field has no
 * padding before itself and will be located at a valid instruction location,
 * and that the `function` field is at a pre-determined offset from the
 * `ldr_pc` field.
 */
typedef struct {
    uint32_t ldr_pc;
    uint32_t magic;
    uint32_t box_ptr;
    uint32_t function; /* It's like a pretend literal pool. */
} UVISOR_PACKED __attribute__((aligned(4))) TRPCGateway;

typedef uint32_t (*TFN_Ptr)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef int (*TFN_RPC_Callback)(int);

/* Private for use only by uVisor lib, but the size of this type must be known
 * publicly for the UVISOR_BOX_RPC_DECL macro to work. */
typedef struct {
    /* XXX TODO Think about what memory is accessible when and by whom. uVisor
     * has to manage moving memory around between contexts. We want to minimize
     * SVCs to uVisor, but we probably need one SVC to initiate the call (send
     * parameters to callee) and one to finish it (send back return code to
     * caller), and then there are also a bunch of RTOS SVCs. */

    /* XXX These are set by the caller, read by the callee */
    uint32_t p0;
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;

    TFN_Ptr fn;

    /* The caller reads from this this queue. The callee writes to this queue.
     * */
    osMessageQId result_q_id;
} uvisor_rpc_message_t;

/* This is the object to wait on for a result. */
typedef struct {
    uint8_t _internal_data[RESULT_QUEUE_SIZE];
} uvisor_rpc_result_t;

/* This is the queue to wait on for incoming RPC. */
typedef struct {
    uint8_t _internal_data[CALLEE_QUEUE_SIZE];
} uvisor_rpc_callee_queue_t;

/* This synchronous function is easy-mode. It gets you talkin' with the other
 * box, but not in a mutally distrustful fashion: you have to trust that the
 * target box will eventually return. */
UVISOR_EXTERN uint32_t rpc_fncall(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn);

/* Return a message queue id that the caller can wait on to get a result.
 * XXX This is pure mechanism: the caller can choose to make a thread to wait
 * on result or use any thread they want whenever they feel like. We, as
 * implementers of RPC, don't make the decision for the caller. The decision is
 * best made where all the information is available to make the decision. If we
 * need to wrap that with an abstraction to make the user not have to think,
 * then we'll provide a separate wrapper to do so, but goal #1 is a stable and
 * functional API. (An API with too much policy in it is bound to be unstable,
 * as users request different policies.) */
/* Notice that we don't even need to specify any sort of timeout stuff here.
 * All that policy is decided by the user. */
UVISOR_EXTERN int rpc_fncall_async(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const TFN_Ptr fn,
                                   uvisor_rpc_result_t * queue);

UVISOR_EXTERN int rpc_fncall_waitfor(uvisor_rpc_callee_queue_t * queue, uint32_t timeout_ms);

/* XXX TODO Re-word this.
 * Wait for the result of a previously started asynchronous rpc. After this
 * call, ret will contain the return value of the rpc. The return value of this
 * function may indicate that there was a timeout with non-zero. */
UVISOR_EXTERN int rpc_fncall_wait(uvisor_rpc_result_t * result, uint32_t timeout_ms, uint32_t * ret);

UVISOR_EXTERN size_t rpc_pool_size_for_callee_queue(size_t max_num_items);
UVISOR_EXTERN int rpc_init_result(uvisor_rpc_result_t * queue);
UVISOR_EXTERN int rpc_init_callee_queue(uvisor_rpc_callee_queue_t * queue, uint8_t * pool, size_t pool_size,
                                        size_t max_num_items,
                                        const TFN_Ptr fn_ptr_array[], size_t fn_count);


/* XXX Giant Hacks below! */
UVISOR_EXTERN void * malloc_0(size_t bytes);
UVISOR_EXTERN void free_0(void * ptr);

#endif
