/*
 * Module imx_vpu.cpp
 *
 * This module defines the methods of the vpu_t class
 * as declared in imx_vpu.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */


#include "imx_vpu.h"
#include <stdio.h>
extern "C" {
#include <vpu_lib.h>
#include <vpu_io.h>
};

vpu_t::vpu_t(void)
	: worked_( false )
{
        RetCode rc = vpu_Init(0);
	worked_ = (RETCODE_SUCCESS == rc);
	if( !worked_ )
                fprintf(stderr, "Error %d initializing VPU\n", rc );
}

vpu_t::~vpu_t(void)
{
	if( worked_ ) {
                vpu_UnInit();
	}
}
