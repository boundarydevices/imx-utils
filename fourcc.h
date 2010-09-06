#ifndef __FOURCC_H__
#define __FOURCC_H__ "$Id$"

/*
 * fourcc.h
 *
 * This header file declares a set of utility routines for handling
 * various graphics formats via their "fourcc" code (a binary string
 * representation).
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include <string.h>

inline char const *fourcc_str(unsigned long fcc){
	static char buf[5];
    memcpy(buf,&fcc,sizeof(buf)-1);
    buf[4] = '\0' ;
	return buf ;
}

inline unsigned fourcc_from_str(char const *fcc){
	unsigned rval = 0 ;
	strncpy((char *)&rval,fcc,sizeof(fcc));
	return rval ;
}

/* 
 * This routine converts and validates the specified fourcc
 * value
 */
bool supported_fourcc(char const *arg, unsigned &fourcc);

/* 
 * Same for binary input
 */
bool supported_fourcc(unsigned fourcc);

/*
 * retrieve the list of supported formats
 */
void supported_fourcc_formats(unsigned const *&values, unsigned &numValues);

/*
 * Return the number of bits per pixel for the supported format. Note that in the
 * case of YUV formats, this is the number of bits of Y, not the total number, 
 * so it can be used in the calculation of fb_var_screeninfo.bits_per_pixel and
 * line length.
 */ 
unsigned bits_per_pixel(unsigned fourcc);

/*
 * Use this to get the size and offsets of the y, u and v portions
 * of a YUV image format and the increment between samples of u and v.
 *
 * Returns false if not a YUV image format.
 */
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
	unsigned &totalsize );

bool isYUV(unsigned fourcc);

#endif

