/*
 * Module imx_mjpeg_encoder.cpp
 *
 * This module defines the methods of the mjpeg_encoder_t class
 * as declared in imx_mjpeg_encoder.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "imx_mjpeg_encoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#define DEBUGPRINT
#include "debugPrint.h"
#include "fourcc.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>

#define STREAM_BUF_SIZE		0x80000

static unsigned char lumaDcBits[16] = {
0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static unsigned char lumaDcValue[16] = {
0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
0x08, 0x09, 0x0A, 0x0B, 0x00, 0x00, 0x00, 0x00,
};
static unsigned char lumaAcBits[16] = {
0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,
};
static unsigned char lumaAcValue[168] = {
0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
0xF9, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static unsigned char chromaDcBits[16] = {
0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static unsigned char chromaDcValue[16] = {
0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
0x08, 0x09, 0x0A, 0x0B, 0x00, 0x00, 0x00, 0x00,
};
static unsigned char chromaAcBits[16] = {
0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
};
static unsigned char chromaAcValue[168] = {
0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
0xF9, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static unsigned char lumaQ[64] = {
0x0C, 0x08, 0x08, 0x08, 0x09, 0x08, 0x0C, 0x09,
0x09, 0x0C, 0x11, 0x0B, 0x0A, 0x0B, 0x11, 0x15,
0x0F, 0x0C, 0x0C, 0x0F, 0x15, 0x18, 0x13, 0x13,
0x15, 0x13, 0x13, 0x18, 0x11, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
static unsigned char chromaBQ[64] = {
0x0D, 0x0B, 0x0B, 0x0D, 0x0E, 0x0D, 0x10, 0x0E,
0x0E, 0x10, 0x14, 0x0E, 0x0E, 0x0E, 0x14, 0x14,
0x0E, 0x0E, 0x0E, 0x0E, 0x14, 0x11, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x11, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
static unsigned char chromaRQ[64] = {
0x0D, 0x0B, 0x0B, 0x0D, 0x0E, 0x0D, 0x10, 0x0E,
0x0E, 0x10, 0x14, 0x0E, 0x0E, 0x0E, 0x14, 0x14,
0x0E, 0x0E, 0x0E, 0x0E, 0x14, 0x11, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x11, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
static unsigned char lumaQ2[64] = {
0x06, 0x04, 0x04, 0x04, 0x05, 0x04, 0x06, 0x05,
0x05, 0x06, 0x09, 0x06, 0x05, 0x06, 0x09, 0x0B,
0x08, 0x06, 0x06, 0x08, 0x0B, 0x0C, 0x0A, 0x0A,
0x0B, 0x0A, 0x0A, 0x0C, 0x10, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x10, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
static unsigned char chromaBQ2[64] = {
0x07, 0x07, 0x07, 0x0D, 0x0C, 0x0D, 0x18, 0x10,
0x10, 0x18, 0x14, 0x0E, 0x0E, 0x0E, 0x14, 0x14,
0x0E, 0x0E, 0x0E, 0x0E, 0x14, 0x11, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x11, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};
static unsigned char chromaRQ2[64] = {
0x07, 0x07, 0x07, 0x0D, 0x0C, 0x0D, 0x18, 0x10,
0x10, 0x18, 0x14, 0x0E, 0x0E, 0x0E, 0x14, 0x14,
0x0E, 0x0E, 0x0E, 0x0E, 0x14, 0x11, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x11, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x11, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
};

mjpeg_encoder_t::mjpeg_encoder_t(
	vpu_t &vpu,
	unsigned w,
	unsigned h,
	unsigned fourcc,
	int 	 fdCamera,
	unsigned numBuffers,
	unsigned char **cameraBuffers)
	: initialized_(false)
	, fourcc_(fourcc)
	, w_(w)
	, h_(h)
	, imgSize_(0)
	, handle_(0)
	, buffers(cameraBuffers)
	, yoffs(0)
	, uoffs(0)
	, voffs(0)
{
	if( (0 == w) || (0 == h) ) {
		fprintf(stderr, "Invalid w or h (%ux%u)\n", w, h );
		return ;
	}
	fprintf(stderr, "%s: %ux%u - %u buffers\n", __func__, w_, h_, numBuffers );
	vpu_mem_desc mem_desc = {0};

	unsigned ysize ;
	unsigned yadder ;
	unsigned uvsize ;
	unsigned uvrowdiv ;
	unsigned uvcoldiv ;
	unsigned uvadder ;
	unsigned totalsize ;
	if( !fourccOffsets(fourcc,w,h,ysize,yoffs,yadder,uvsize,uvrowdiv,uvcoldiv,uoffs,voffs,uvadder,totalsize) ){
		fprintf(stderr, "Invalid fourcc 0x%x\n", fourcc);
		return ;
	}
	imgSize_ = totalsize ;

printf( "%s: fourcc offsets %u/%u/%u, adders %u/%u\n", __func__, yoffs, uoffs,voffs, yadder,uvadder);
printf( "%s: sizes %u/%u: %u\n", __func__, ysize, uvsize, totalsize);

	/* get physical contigous bit stream buffer */
	mem_desc.size = STREAM_BUF_SIZE;
	if (0 != IOGetPhyMem(&mem_desc)) {
		fprintf(stderr,"Unable to obtain physical memory\n");
		return ;
	}

	/* mmap that physical buffer */
	virt_bsbuf_addr = IOGetVirtMem(&mem_desc);
	if (virt_bsbuf_addr <= 0) {
		fprintf(stderr,"Unable to map physical memory\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}

	debugPrint( "stream buffer: %u bytes: phys 0x%lx, cpu 0x%lx, virt 0x%lx (%x)\n",
		    mem_desc.size, mem_desc.phy_addr, mem_desc.cpu_addr, mem_desc.virt_uaddr, virt_bsbuf_addr );

	phy_bsbuf_addr = mem_desc.phy_addr;

	EncOpenParam encop = {0};
	/* Fill up parameters for encoding */
	encop.bitstreamBuffer = phy_bsbuf_addr;
	encop.bitstreamBufferSize = STREAM_BUF_SIZE;
	encop.bitstreamFormat = STD_MJPG ;

	encop.picWidth = picwidth = w;
	encop.picHeight = picheight = h;

	/*Note: Frame rate cannot be less than 15fps per H.263 spec */
	encop.frameRateInfo = 30;
	encop.bitRate = 0 ;
	encop.gopSize = 1 ;
	encop.slicemode.sliceMode = 0;	/* 0: 1 slice per picture; 1: Multiple slices per picture */
	encop.slicemode.sliceSizeMode = 0; /* 0: silceSize defined by bits; 1: sliceSize defined by MB number*/
	encop.slicemode.sliceSize = 4000;  /* Size of a slice in bits or MB numbers */

	encop.initialDelay = 0;
	encop.vbvBufferSize = 0;        /* 0 = ignore 8 */
	encop.intraRefresh = 0;
	encop.sliceReport = 0;
	encop.mbReport = 0;
	encop.mbQpReport = 0;
	encop.rcIntraQp = -1;
	encop.userQpMax = 0;
	encop.userGamma = (Uint32)(0.75*32768);         /*  (0*32768 <= gamma <= 1*32768) */
	encop.RcIntervalMode= 1;        /* 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level */
	encop.MbInterval = 0;

	if (uvsize == ysize/2) {
	    encop.EncStdParam.mjpgParam.mjpg_sourceFormat = 1 ; // YUV422 horizontal
	} else if (uvsize == ysize/4) {
	    encop.EncStdParam.mjpgParam.mjpg_sourceFormat = 0 ; // YUV420
	} else
		printf( "%s: unknown input format: %u/%u\n", __func__,ysize,uvsize );
printf( "%s: mjpg_source format %d\n", __func__, encop.EncStdParam.mjpgParam.mjpg_sourceFormat);
	encop.ringBufferEnable = 0;
	encop.dynamicAllocEnable = 0;
	encop.chromaInterleave = (uoffs < voffs) ? (uoffs+uvsize > voffs)
						 : (voffs+uvsize > uoffs);
printf( "%s: chroma interleaved: %d\n", __func__, encop.chromaInterleave);
	Uint8 *qMatTable = encop.EncStdParam.mjpgParam.mjpg_qMatTable = (Uint8*)calloc(192,1);
	if (qMatTable == NULL) {
		fprintf(stderr,"Failed to allocate qMatTable\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}

	/* Rearrange and insert pre-defined Q-matrix to deticated variable. */
	for(int i = 0; i < 64; i += 4)
	{
		qMatTable[i] = lumaQ2[i + 3];
		qMatTable[i + 1] = lumaQ2[i + 2];
		qMatTable[i + 2] = lumaQ2[i + 1];
		qMatTable[i + 3] = lumaQ2[i];
	}

	for(int i = 64; i < 128; i += 4)
	{
		qMatTable[i] = chromaBQ2[i + 3 - 64];
		qMatTable[i + 1] = chromaBQ2[i + 2 - 64];
		qMatTable[i + 2] = chromaBQ2[i + 1 - 64];
		qMatTable[i + 3] = chromaBQ2[i - 64];
	}

	for(int i = 128; i < 192; i += 4)
	{
		qMatTable[i] = chromaRQ2[i + 3 - 128];
		qMatTable[i + 1] = chromaRQ2[i + 2 - 128];
		qMatTable[i + 2] = chromaRQ2[i + 1 - 128];
		qMatTable[i + 3] = chromaRQ2[i - 128];
	}

	unsigned char *huffTable = encop.EncStdParam.mjpgParam.mjpg_hufTable = (Uint8*)calloc(432,1);
	if (huffTable == NULL) {
		free(qMatTable);
		fprintf(stderr,"Failed to allocate huffTable\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}

debugPrint("allocated qMat and huff tables\n");
	/* Don't consider user defined hufftable this time */
	/* Rearrange and insert pre-defined Huffman table to deticated variable. */
	for(int i = 0; i < 16; i += 4)
	{
		huffTable[i] = lumaDcBits[i + 3];
		huffTable[i + 1] = lumaDcBits[i + 2];
		huffTable[i + 2] = lumaDcBits[i + 1];
		huffTable[i + 3] = lumaDcBits[i];
	}
	for(int i = 16; i < 32 ; i += 4)
	{
		huffTable[i] = lumaDcValue[i + 3 - 16];
		huffTable[i + 1] = lumaDcValue[i + 2 - 16];
		huffTable[i + 2] = lumaDcValue[i + 1 - 16];
		huffTable[i + 3] = lumaDcValue[i - 16];
	}
	for(int i = 32; i < 48; i += 4)
	{
		huffTable[i] = lumaAcBits[i + 3 - 32];
		huffTable[i + 1] = lumaAcBits[i + 2 - 32];
		huffTable[i + 2] = lumaAcBits[i + 1 - 32];
		huffTable[i + 3] = lumaAcBits[i - 32];
	}
	for(int i = 48; i < 216; i += 4)
	{
		huffTable[i] = lumaAcValue[i + 3 - 48];
		huffTable[i + 1] = lumaAcValue[i + 2 - 48];
		huffTable[i + 2] = lumaAcValue[i + 1 - 48];
		huffTable[i + 3] = lumaAcValue[i - 48];
	}
	for(int i = 216; i < 232; i += 4)
	{
		huffTable[i] = chromaDcBits[i + 3 - 216];
		huffTable[i + 1] = chromaDcBits[i + 2 - 216];
		huffTable[i + 2] = chromaDcBits[i + 1 - 216];
		huffTable[i + 3] = chromaDcBits[i - 216];
	}
	for(int i = 232; i < 248; i += 4)
	{
		huffTable[i] = chromaDcValue[i + 3 - 232];
		huffTable[i + 1] = chromaDcValue[i + 2 - 232];
		huffTable[i + 2] = chromaDcValue[i + 1 - 232];
		huffTable[i + 3] = chromaDcValue[i - 232];
	}
	for(int i = 248; i < 264; i += 4)
	{
		huffTable[i] = chromaAcBits[i + 3 - 248];
		huffTable[i + 1] = chromaAcBits[i + 2 - 248];
		huffTable[i + 2] = chromaAcBits[i + 1 - 248];
		huffTable[i + 3] = chromaAcBits[i - 248];
	}
	for(int i = 264; i < 432; i += 4)
	{
		huffTable[i] = chromaAcValue[i + 3 - 264];
		huffTable[i + 1] = chromaAcValue[i + 2 - 264];
		huffTable[i + 2] = chromaAcValue[i + 1 - 264];
		huffTable[i + 3] = chromaAcValue[i - 264];
	}

	debugPrint("check open params\n");

	if (encop.bitstreamBuffer % 4) {	/* not 4-bit aligned */
		printf( "--> bitstreamBuffer %lx\n", encop.bitstreamBuffer);
	}
	if (encop.bitstreamBufferSize % 1024 ||
	    encop.bitstreamBufferSize < 1024 ||
	    encop.bitstreamBufferSize > 16383 * 1024) {
		printf( "--> bitstreamBufferSize %ld\n", encop.bitstreamBufferSize);
	}
	if (encop.bitstreamFormat != STD_MPEG4 &&
	    encop.bitstreamFormat != STD_H263 &&
	    encop.bitstreamFormat != STD_AVC &&
	    encop.bitstreamFormat != STD_MJPG) {
		printf( "--> bitstreamFormat %x\n", encop.bitstreamFormat);
	}
	if (encop.bitRate > 32767 || encop.bitRate < 0) {
		printf( "--> bitrate %d\n", encop.bitRate);
	}
	if (encop.bitRate != 0 && encop.initialDelay > 32767) {
		printf( "--> bitrate %d, initial delay %d\n", encop.bitRate, encop.initialDelay);
	}
	if (encop.bitRate != 0 && encop.initialDelay != 0 &&
	    encop.vbvBufferSize < 0) {
		printf( "--> bitrate %d, initial delay %d, vbvBufferSize %d\n", encop.bitRate, encop.initialDelay, encop.vbvBufferSize );
	}
	if (encop.gopSize > 60) {
		printf( "--> gopSize %d\n", encop.gopSize );
	}
	if (encop.slicemode.sliceMode != 0 && encop.slicemode.sliceMode != 1) {
		printf( "--> sliceMode %d\n", encop.slicemode.sliceMode );
	}
	if (encop.slicemode.sliceMode == 1) {
		if (encop.slicemode.sliceSizeMode != 0 &&
		    encop.slicemode.sliceSizeMode != 1) {
			printf( "--> slicemode.sliceSizeMode %d\n", encop.slicemode.sliceSizeMode );
		}
		if (encop.slicemode.sliceSize == 0) {
			printf( "--> slicemode.sliceSize %d\n", encop.slicemode.sliceSize );
		}
	}
	if (cpu_is_mx27()) {
		if (encop.sliceReport != 0 && encop.sliceReport != 1) {
			printf( "--> sliceReport %d\n", encop.sliceReport );
		}
		if (encop.mbReport != 0 && encop.mbReport != 1) {
			printf( "--> mbReport %d\n", encop.mbReport );
		}
	}
	if (encop.intraRefresh < 0 || encop.intraRefresh >=
	    (encop.picWidth * encop.picHeight / 256)) {
		debugPrint( "--> intraRefresh %d, width %d, height %d\n", encop.intraRefresh, encop.picWidth, encop.picHeight );
	}

	debugPrint( "format %d, %ux%u\n", encop.bitstreamFormat, encop.picWidth, encop.picHeight );

	if (encop.picWidth < 32 || encop.picHeight < 16) {
	debugPrint( "bad size\n");
	}

debugPrint( "opening encoder\n" );

	RetCode ret = vpu_EncOpen(&handle_, &encop);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"Encoder open failed %d\n", ret);
		IOFreePhyMem(&mem_desc);
		return ;
	}

debugPrint( "encoder initialized\n" );

	SearchRamParam search_pa = {0};
	iram_t iram;
	int ram_size;

	memset(&iram, 0, sizeof(iram_t));
	ram_size = ((picwidth + 15) & ~15) * 36 + 2048;
	IOGetIramBase(&iram);
	if ((iram.end - iram.start) < ram_size) {
		debugPrint("vpu iram is less than needed: %u..%u/%u\n", iram.start,iram.end,ram_size);
		debugPrint("NOT Using IRAM for ME\n" );
	} else {
		/* Allocate max iram for vpu encoder search ram*/
		ram_size = iram.end - iram.start;
		search_pa.searchRamAddr = iram.start;
		search_pa.SearchRamSize = ram_size;
		debugPrint( "search iram %u..%u for %u bytes\n", iram.start, iram.end, ram_size );
		ret = vpu_EncGiveCommand(handle_, ENC_SET_SEARCHRAM_PARAM, &search_pa);
		if (ret != RETCODE_SUCCESS) {
			fprintf(stderr, "Encoder SET_SEARCHRAM_PARAM failed\n");
			IOFreePhyMem(&mem_desc);
			return ;
		}
		else {
			debugPrint("Using IRAM for ME\n" );
		}
	}

	debugPrint( "get initial info\n");
	EncInitialInfo initinfo = {0};
	ret = vpu_EncGetInitialInfo(handle_, &initinfo);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"Encoder GetInitialInfo failed\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}

	debugPrint( "have initial info\n" );

	fbcount = numBuffers ;
	int stride = ((picwidth + 15) & ~15)*((0 != encop.EncStdParam.mjpgParam.mjpg_sourceFormat)+1);

	fb = (FrameBuffer *)calloc(fbcount, sizeof(FrameBuffer));
	if (fb == NULL) {
		fprintf(stderr,"Failed to allocate fb\n");
		IOFreePhyMem(&mem_desc);
		return ;
	}
debugPrint( "allocated FrameBuffer fb: %p\n", fb );

	for (int i = 0; i < fbcount; i++) {
		struct v4l2_buffer buf ; memset(&buf,0,sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i ;

		if (0 > ioctl (fdCamera, VIDIOC_QUERYBUF, &buf)) {
			perror ("VIDIOC_QUERYBUF");
			free(fb);
			IOFreePhyMem(&mem_desc);
			return ;
		}
		fb[i].bufY = buf.m.offset+yoffs;
		fb[i].bufCb = buf.m.offset+uoffs;
		fb[i].bufCr = buf.m.offset+voffs;
	}
debugPrint( "registering frame buffer\n" );
	ret = vpu_EncRegisterFrameBuffer(handle_, fb, fbcount, stride, stride);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"Register frame buffer failed\n");
		free(fb);
		IOFreePhyMem(&mem_desc);
		return ;
	}
	else
		debugPrint( "registered frame buffer\n" );

debugPrint( "%u frame buffers allocated and registered\n", fbcount );

debugPrint( "freeing huffTable and qMatTable\n" );
if (huffTable)
	free(huffTable);
if (qMatTable)
	free(qMatTable);
	initialized_ = true ;
debugPrint("Done with %s\n", __func__ );
}

mjpeg_encoder_t::~mjpeg_encoder_t(void) {

	debugPrint( "closing encoder\n" );
	RetCode rc = vpu_EncClose(handle_);
	if( RETCODE_SUCCESS != rc )
		fprintf(stderr, "Error %d closing encoder\n", rc );
	else {
		debugPrint( "encoder closed\n" );
	}
}

bool mjpeg_encoder_t::get_bufs( unsigned index, unsigned char *&y, unsigned char *&u, unsigned char *&v )
{
	unsigned char *base = buffers[index];
	y = base + yoffs ;
	u = base + uoffs ;
	v = base + voffs ;
	return true ;
}

static void clampY(unsigned char *y,unsigned numbytes){
	while (numbytes--) {
		unsigned yval = *y ;
		*y++ = 16+((220*yval)>>8);
	}
}

bool mjpeg_encoder_t::encode(unsigned index, void const *&outData, unsigned &outLength)
{
	EncParam  enc_param = {0};
	unsigned char *y, *u, *v ;
	if (get_bufs(index,y,u,v)) {
		clampY(y,uoffs);
	}
	enc_param.sourceFrame = &fb[index];
	enc_param.quantParam = 23;
	enc_param.forceIPicture = 0;
	enc_param.skipPicture = 0;
	RetCode ret = vpu_EncStartOneFrame(handle_, &enc_param);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"vpu_EncStartOneFrame failed Err code:%d\n",
								ret);
		return false ;
	}

	while (vpu_IsBusy()) {
		vpu_WaitForInt(30);
		if(vpu_IsBusy()){
			debugPrint( "busy\n");
		}
	}

	EncOutputInfo outinfo = {0};
	ret = vpu_EncGetOutputInfo(handle_, &outinfo);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"vpu_EncGetOutputInfo failed Err code: %d\n",
								ret);
		return false ;
	}

	outData = (void *)(virt_bsbuf_addr + outinfo.bitstreamBuffer - phy_bsbuf_addr);
	outLength = outinfo.bitstreamSize ;
	return true ;
}
