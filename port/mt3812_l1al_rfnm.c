/*
 * Copyright 2024-2025 NXP
 */

/*
 * NXP Proprietary. This software is owned or controlled by NXP and may only
 * be used strictly in accordance with the applicable license terms. By expressly accepting
 * such terms or by downloading, installing, activating and/or otherwise using
 * the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms. If you do not agree to
 * be bound by the applicable license terms, then you may not retain,
 * install, activate or otherwise use the software.
 */

/** @file mt3812_l1al_rfnm.c
 *  @brief MT3812 SDK - L1 Abstraction Layer (L1AL)
 *         Dummy L1AL Functions
 *
 *  @date July 29, 2024
 *  @author Dinesh Ladi
 */
 
 /* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include "mt3812_drv.h"
#include "diora_l1al.h"
#include "diora_osal.h"


#define unused(param) param __attribute__ ((unused))

void   *diora_l1al_init (u32 id, struct dcparams* unused(params))/**< Initialize ADC DAC block accesses */
{
    return (void *)0;
}

static int done = 0;

error_t diora_l1al_trx (void * handle, c16 *outbuf, c16 *inbuf, u16 len, u16 chan)
{
    return 0;
}

error_t diora_l1al_trx_hs (void * handle, c16 *outbuf, c16 *inbuf, u16 len, u16 chan)
{
    return 0;
}

