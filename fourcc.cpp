/*
 * Module fourcc.cpp
 *
 * This module defines the fourcc utility routines described
 * in fourcc.h
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "fourcc.h"
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

enum colorspace {
	YUV,
	RGB
};

#define PXF_PLANAR		1
#define PXF_PLANAR_UV_W_HALF	2
#define PXF_PLANAR_UV_H_HALF	4
#define PXF_COLORSPACE_YUV	8
#define PXF_PLANAR_PARTIAL	16
#define PXF_PLANAR_V_FIRST	32
#define PXF_COLORSPACE_YV_FIRST 64

struct pixformat_info {
	unsigned 	v4l2_format ;
	char const 	*name ;
	unsigned 	bytes_per_component ;
	enum colorspace	colorspace ;
	unsigned        flags ;
};

static const struct pixformat_info pix_formats[] = {
	{V4L2_PIX_FMT_YUV420,	"YUV420",	1, YUV, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_UV_H_HALF},
	{V4L2_PIX_FMT_YVU420,	"YVU420",	1, YUV, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_UV_H_HALF | PXF_PLANAR_V_FIRST},
	{V4L2_PIX_FMT_NV12,	"NV12",		1, YUV, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_UV_H_HALF | PXF_PLANAR_PARTIAL},
	{V4L2_PIX_FMT_YUV422P,	"YUV422P",	1, YUV, PXF_PLANAR | PXF_PLANAR_UV_W_HALF},
	{v4l2_fourcc('Y','V','1','6'),"YVU422P",	1, YUV, PXF_PLANAR | PXF_PLANAR_UV_W_HALF | PXF_PLANAR_V_FIRST},
	{V4L2_PIX_FMT_SBGGR8,	"SBGGR8",	1, RGB, 0},
	{V4L2_PIX_FMT_SGBRG8,	"SGBRG8",	1, RGB, 0},
	{V4L2_PIX_FMT_SGRBG10,	"SGRBG10",	2, RGB, 0},
	{V4L2_PIX_FMT_SBGGR16,	"SBGGR16",	2, RGB, 0},
	{V4L2_PIX_FMT_RGB565,	"RGB565",	2, RGB, 0},
	{V4L2_PIX_FMT_UYVY,	"UYVY",		2, YUV, 0},
	{V4L2_PIX_FMT_YUYV,	"YUYV",		2, YUV, 0},
	{V4L2_PIX_FMT_RGB24,	"RGB24",	3, RGB, 0},
	{V4L2_PIX_FMT_BGR24,	"BGR24",	3, RGB, 0},
	{V4L2_PIX_FMT_RGB32,	"RGB32",	4, RGB, 0},
	{V4L2_PIX_FMT_BGR32,	"BGR32",	4, RGB, 0},
};

unsigned bits_per_pixel(unsigned fourcc)
{
	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format)
			return pix_formats[i].bytes_per_component*8 ;
	}
	return 0 ;
}

static unsigned const *supported_formats = 0 ;

bool supported_fourcc(char const *arg, unsigned &fourcc){
	fourcc = fourcc_from_str(arg);
	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format)
			return true ;
	}
	return false ;
}

bool supported_fourcc(unsigned fourcc){
	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format)
			return true ;
	}
	return false ;
}

void supported_fourcc_formats(unsigned const *&values, unsigned &numValues)
{
	if( 0 == supported_formats ){
		unsigned *fmts = new unsigned [ARRAY_SIZE(pix_formats)];
		for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
			fmts[i] = pix_formats[i].v4l2_format ;
		}
		supported_formats = (unsigned const *)fmts ;
	}

	values = supported_formats ;
	numValues = ARRAY_SIZE(pix_formats);
}

bool isYUV(unsigned fourcc)
{
	for( unsigned i = 0 ; i < ARRAY_SIZE(pix_formats); i++ ){
		if(fourcc == pix_formats[i].v4l2_format)
			return YUV == pix_formats[i].colorspace ;
	}
	return false ;
}

/*
 * In order to iterate through each of the bytes of Y, U, and V for each supported YUV frame
 * format we need to know each of these for Y, U, and V.
 *
 *		initial offset
 *		row divisor
 *		column divisor
 *		column adder
 *
 * In order to produce these, we'll need the width, height and fourcc value coming in.
 * The following table will allow these calculations based on multipliers, divisors and
 * fixed offsets based on W*H and W (height doesn't matter).
 *
 * The initial offset calculation is this:
 *		initial offset = (imwh_numerator*(W*H)/imwh_denominator) + (imw_numerator*W)/imw_denominator + fixed_offset ;
 *
 * The column divisors and adders are strictly constants.
 *
 */
struct multiplier {
	unsigned num ;
	unsigned denom ;
};

struct scaler {
	struct multiplier whmult ;
	struct multiplier wmult ;
	unsigned initial_offset ;
	unsigned row_divisor ;
	unsigned column_divisor ;
	unsigned column_adder ;
};

struct yuvOffsets_t {
	unsigned fourcc ;
	struct scaler	yscaler ;
	struct scaler	uscaler ;
	struct scaler	vscaler ;
};

/*
 * To take an example: YUV420P consists of an [W*H] Y plane followed by [W*H/4] U and V planes, so
 * its table entry will be this:
 *
 *	Y:  WH      W    offset rd cd ca	Y:  WH      W    offset rd cd ca 	Y:  WH      W    offset rd cd ca 
 *	  {{0,1}, {0,1},   0,    1, 1, 1}},	  {{1,1}, {0,1},   0,    2, 2, 1}},  	  {{0,1}, {0,1},   0,    2, 2, 1}}, 
 *
 * In English, all of the initial offsets yield zero for Y, the U plane is offset by 1*W*H and the V plane is offset by (5/4)*W*H (1.25).
 * Both row and column divisors are 1 for Y and 2 for both U and V planes.
 * All of the column adders are 1, so adjacent columns are adjacent in memory. 
 *
 * For the YUYV format, each of the W*H and W multipliers will yield zero, the fixed offsets will be 0, 1, and 3 for Y, U, and V and
 * the column adders will be 2, 4, and 4 since the Y values are two bytes apart in memory and U and V are two bytes apart.
 *
 */
static struct yuvOffsets_t const yuvOffsets[] = {
	{ V4L2_PIX_FMT_YUV420,		// YUV 420 planar
		{	{0,1}		//	Y: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	1		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{1,1}		//	U: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	2		//	   row divisor
		,	2		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{5,4}		//	V: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	2		//	   row divisor
		,	2		//	   column divisor
		,	1		//	   column adder
                }
	}
,	{ V4L2_PIX_FMT_YVU420,		// YUV 420 planar
		{	{0,1}		//	Y: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	1		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{5,4}		//	U: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	2		//	   row divisor
		,	2		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{1,1}		//	V: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	2		//	   row divisor
		,	2		//	   column divisor
		,	1		//	   column adder
                }
	}
,	{ V4L2_PIX_FMT_NV12,		// YUV 420 semi-planar
		{	{0,1}		//	Y: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	1		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{1,1}		//	U: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	2		//	   row divisor
		,	2		//	   column divisor
		,	2		//	   column adder
                }
	,	{	{1,1}		//	V: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	1		//	   initial offset
		,	2		//	   row divisor
		,	2		//	   column divisor
		,	2		//	   column adder
                }
	}
,	{ V4L2_PIX_FMT_YUV422P,		// YUV 422 planar
		{	{0,1}		//	Y: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	1		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{1,1}		//	U: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	2		//	   column divisor
		,	1		//	   column adder
                }
	,	{	{3,2}		//	V: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	2		//	   column divisor
		,	1		//	   column adder
                }
	}
,	{ V4L2_PIX_FMT_YUYV,		// YUYV
		{	{0,1}		//	Y: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	1		//	   column divisor
		,	2		//	   column adder
                }
	,	{	{0,1}		//	U: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	1		//	   initial offset
		,	1		//	   row divisor
		,	2		//	   column divisor
		,	4		//	   column adder
                }
	,	{	{0,1}		//	V: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	3		//	   initial offset
		,	1		//	   row divisor
		,	2		//	   column divisor
		,	4		//	   column adder
                }
	}
,	{ V4L2_PIX_FMT_UYVY,		// UYVY
		{	{0,1}		//	Y: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	1		//	   initial offset
		,	1		//	   row divisor
		,	1		//	   column divisor
		,	2		//	   column adder
                }
	,	{	{0,1}		//	U: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	0		//	   initial offset
		,	1		//	   row divisor
		,	2		//	   column divisor
		,	4		//	   column adder
                }
	,	{	{0,1}		//	V: W*H	multiplier
		,	{0,1}		// 	   W multiplier
		,	2		//	   initial offset
		,	1		//	   row divisor
		,	2		//	   column divisor
		,	4		//	   column adder
                }
	}
};

bool fourccOffsets(
	unsigned fourcc,
	unsigned width,
	unsigned height,
	unsigned &ysize,
	unsigned &yoffs,
	unsigned &yadder,
	unsigned &uvsize,
	unsigned &uvrowdiv,
	unsigned &uvcoldiv,
	unsigned &uoffs,
	unsigned &voffs,
	unsigned &uvadder,
	unsigned &totalsize )
{
	unsigned WH = width*height;
	for (unsigned i = 0 ; i < ARRAY_SIZE(yuvOffsets); i++) {
		struct yuvOffsets_t const &entry = yuvOffsets[i];
		if (entry.fourcc==fourcc) {
			if ((entry.uscaler.row_divisor == entry.vscaler.row_divisor)
			    &&
			    (entry.uscaler.column_divisor == entry.vscaler.column_divisor)
			    &&
			    (entry.uscaler.column_adder == entry.vscaler.column_adder)) {
				ysize=(WH/entry.yscaler.row_divisor/entry.yscaler.column_divisor);
				uvsize=(WH/entry.uscaler.row_divisor/entry.uscaler.column_divisor);
				totalsize = ysize+2*uvsize;
				yadder = entry.yscaler.column_adder ;
				uvadder = entry.uscaler.column_adder ;
				uvrowdiv = entry.uscaler.row_divisor ;
				uvcoldiv = entry.uscaler.column_divisor ;
				yoffs = (WH*entry.yscaler.whmult.num)/entry.yscaler.whmult.denom
				      + (width*entry.yscaler.wmult.num)/entry.yscaler.wmult.denom
				      + entry.yscaler.initial_offset ;
				uoffs = (WH*entry.uscaler.whmult.num)/entry.uscaler.whmult.denom
				      + (width*entry.uscaler.wmult.num)/entry.uscaler.wmult.denom
				      + entry.uscaler.initial_offset ;
				voffs = (WH*entry.vscaler.whmult.num)/entry.vscaler.whmult.denom
				      + (width*entry.vscaler.wmult.num)/entry.vscaler.wmult.denom
				      + entry.vscaler.initial_offset ;
				return true ;
			} // this API can't handle different U and V sizes
			else {
				fprintf(stderr, "%s: Invalid fourcc for this API\n", __func__ );
				break;
			}
		}
	}
	return false ;
}

#ifdef STANDALONE_FOURCC
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char const * const argv[]){
    if (1 < argc) {
        for( int arg = 1 ; arg < argc ; arg++ ){
	    unsigned binary = 0 ;
	    bool supported = false ;
            char const *param = argv[arg];
            if( '0' == *param ){
                binary = strtoul(param,0,0);
                supported = supported_fourcc(binary);
                printf( "fourcc(%s:0x%08x) == '%s'%s\n", param, binary, fourcc_str(binary), supported ? " supported" : " not supported");
            } else {
		binary = fourcc_from_str(param);
		supported = supported_fourcc(binary);
                printf( "fourcc(%s) == 0x%08x%s\n", param, binary, supported ? " supported" : " not supported");
	    }
	    if(supported){
			unsigned ysize ;
			unsigned yoffs ;
			unsigned yadder ;
			unsigned uvsize ;
			unsigned uvrowdiv ;
			unsigned uvcoldiv ;
			unsigned uoffs ; 
			unsigned voffs ; 
			unsigned uvadder ;
			unsigned totalsize ;
			if( fourccOffsets(binary,320,240,ysize,yoffs,yadder,uvsize,uvrowdiv,uvcoldiv,uoffs,voffs,uvadder,totalsize) ){
				printf( "\tYUV format. For 320x240 surface:\n" 
					"	ySize %u\n"
					"	yOffs %u\n"
					"	yAdder %u\n"
					"	uvSize %u\n"
					"	uvRowDiv %u\n"
					"	uvColDiv %u\n"
					"	uOffs %u\n"
					"	vOffs %u\n"
					"	uvAdder %u\n"
					"	totalsize %u\n"
					, ysize, yoffs, yadder, uvsize, uvrowdiv, uvcoldiv, uoffs, voffs, uvadder, totalsize
					);
			}
			else
				printf( "RGB format\n" );
            }
        }
    }
    else
        fprintf(stderr, "Usage: %s 0xvalue or STRG\n", argv[0]);

    return 0 ;
}
#endif
