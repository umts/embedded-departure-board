/*
 * Copyright (c) 2019-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/***********  WARNING: This is an auto-generated file. Do not edit!  ***********/

#ifndef __TFM_SPM_DB_FUNC_INC__
#define __TFM_SPM_DB_FUNC_INC__

#include "array.h"
#include "psa_manifest/sid.h"

/**************************************************************************/
/** IRQ count per partition */
/**************************************************************************/
#define TFM_PARTITION_TFM_SP_CRYPTO_IRQ_COUNT 0

#define TFM_PARTITION_TFM_SP_PLATFORM_IRQ_COUNT 0

/**************************************************************************/
/** Declarations of partition init functions */
/**************************************************************************/
extern void tfm_crypto_init(void);
extern void platform_sp_init(void);
/**************************************************************************/
/** Context stacks for IRQ handling */
/**************************************************************************/
/* The max size of the context stack can be calculated as a function of the IRQ
 * count of the secure partition:
 *
 * max_stack_size = intr_ctx_size + (IRQ_CNT * (intr_ctx_size + hndl_ctx_size))
 *
 * where:
 *   intr_ctx: Frame pushed when the partition is interrupted
 *   hndl_ctx: Frame pushed when the partition is handling an interrupt
 */
static uint32_t ns_interrupt_ctx_stack[
        sizeof(struct interrupted_ctx_stack_frame_t) / sizeof(uint32_t)];

static uint32_t tfm_core_interrupt_ctx_stack[
        sizeof(struct interrupted_ctx_stack_frame_t) / sizeof(uint32_t)];

static uint32_t ctx_stack_TFM_SP_CRYPTO[
        (sizeof(struct interrupted_ctx_stack_frame_t) +
            (TFM_PARTITION_TFM_SP_CRYPTO_IRQ_COUNT) * (
                sizeof(struct interrupted_ctx_stack_frame_t) +
                sizeof(struct handler_ctx_stack_frame_t)
        )) / sizeof(uint32_t)];

static uint32_t ctx_stack_TFM_SP_PLATFORM[
        (sizeof(struct interrupted_ctx_stack_frame_t) +
            (TFM_PARTITION_TFM_SP_PLATFORM_IRQ_COUNT) * (
                sizeof(struct interrupted_ctx_stack_frame_t) +
                sizeof(struct handler_ctx_stack_frame_t)
        )) / sizeof(uint32_t)];

uint32_t *ctx_stack_list[] =
{
    ns_interrupt_ctx_stack,
    tfm_core_interrupt_ctx_stack,
    ctx_stack_TFM_SP_CRYPTO,
    ctx_stack_TFM_SP_PLATFORM,
};

/**************************************************************************/
/** Dependencies array for Secure Partition */
/**************************************************************************/
static int32_t dependencies_TFM_SP_CRYPTO[] =
{
    TFM_INTERNAL_TRUSTED_STORAGE_SERVICE_SID,
};

/**************************************************************************/
/** The static data of the partition list */
/**************************************************************************/
const struct spm_partition_static_data_t static_data_list[] =
{
    {
        .partition_id         = TFM_SP_NON_SECURE_ID,
        .partition_flags      = 0,
    },

    {
        .partition_id         = TFM_SP_CORE_ID,
        .partition_flags      = SPM_PART_FLAG_APP_ROT | SPM_PART_FLAG_PSA_ROT,
    },

    {
        .partition_id         = TFM_SP_CRYPTO,
        .partition_flags      = 0
                              | SPM_PART_FLAG_PSA_ROT | SPM_PART_FLAG_APP_ROT
                              ,
        .partition_priority   = TFM_PRIORITY(NORMAL),
        .partition_init       = tfm_crypto_init,
        .dependencies_num     = 1,
        .p_dependencies       = dependencies_TFM_SP_CRYPTO,
    },

    {
        .partition_id         = TFM_SP_PLATFORM,
        .partition_flags      = 0
                              | SPM_PART_FLAG_PSA_ROT | SPM_PART_FLAG_APP_ROT
                              ,
        .partition_priority   = TFM_PRIORITY(NORMAL),
        .partition_init       = platform_sp_init,
        .dependencies_num     = 0,
        .p_dependencies       = NULL,
    },

};

/**************************************************************************/
/** The platform data of the partition list */
/**************************************************************************/
const struct platform_data_t **platform_data_list_list[] =
{
    NULL,
    NULL,

    NULL,

    NULL,

};

/**************************************************************************/
/** The partition list for the DB */
/**************************************************************************/
static struct spm_partition_desc_t partition_list [] =
{
    {{0}}, /* placeholder for Non-secure internal partition */
    {{0}},   /* placeholder for TF-M Core internal partition */

    /* -----------------------------------------------------------------------*/
    /* - Partition DB record for TFM_SP_CRYPTO */
    /* -----------------------------------------------------------------------*/
    {
    /* Runtime data */
        .runtime_data             = {0},
        .static_data              = NULL,
        .platform_data_list       = NULL,
    },
    /* -----------------------------------------------------------------------*/
    /* - Partition DB record for TFM_SP_PLATFORM */
    /* -----------------------------------------------------------------------*/
    {
    /* Runtime data */
        .runtime_data             = {0},
        .static_data              = NULL,
        .platform_data_list       = NULL,
    },
};

struct spm_partition_db_t g_spm_partition_db = {
    .is_init = 0,
    .partition_count = ARRAY_SIZE(partition_list),
    .running_partition_idx = 0,
    .partitions = partition_list,
};

#endif /* __TFM_SPM_DB_FUNC_INC__ */
