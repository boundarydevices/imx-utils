#ifndef __IMX_MPEG4_ENCODER_H__
#define __IMX_MPEG4_ENCODER_H__ "$Id$"

/*
 * imx_mpeg4_encoder.h
 *
 * This header file declares the mpeg4_encoder_t class for
 * use in encoding YUV data as MPEG4 using the i.MX51's
 * hardware encoder.
 *
 * The simplest use-case is:
 *
 *	initialize
 *	while (more frames) {
 *		get_bufs();
 *		fill in YUV data
 *		encode()
 *		process output data
 *	}
 *
 * but this class also supports queueing and asynchronous completion
 * of decoding. Using this interface, the simplest use case is
 * something like this:
 *
 *	while (more frames) {
 *		if( get_bufs() ) {
 *		   fill in YUV data
 *		   start_encode()
 *		}
 *		if( encode_complete() ) {
 *		   process output data
 *		}
 *		... poll other devices
 *	}
 *
 * This queued interface allows tagging of each encode operation with
 * an application-defined opaque parameter. It's envisioned that the
 * parameter will provide at least timing information about when the
 * frame of data was received (from a camera).
 *
 * Copyright Boundary Devices, Inc. 2010
 */
extern "C" {
#include <vpu_lib.h>
#include <vpu_io.h>
};

#include "imx_vpu.h"

class mpeg4_encoder_t {
public:
	mpeg4_encoder_t(vpu_t &vpu,
			unsigned width,
			unsigned height,
			unsigned fourcc,
		        unsigned gopSize,
                        struct v4l2_buffer *v4lbuffers,
			unsigned numBuffers,
			unsigned char **buffers);

	bool initialized( void ) const { return initialized_ ; }

	inline unsigned fourcc(void) const { return fourcc_ ; }
	inline unsigned width(void) const { return w_ ; }
	inline unsigned height(void) const { return h_ ; }
	inline unsigned yuvSize(void) const { return imgSize_ ; }

	bool get_bufs( unsigned index, unsigned char *&y, unsigned char *&u, unsigned char *&v );

	// synchronous encode
	bool encode( unsigned index, void const *&outData, unsigned &outLength);

	// get AVC headers
	bool getSPS( void const *&sps, unsigned &len);
	bool getPPS( void const *&pps, unsigned &len);
	~mpeg4_encoder_t(void);
private:
#if 0
	struct frame_buf {
		int addrY;
		int addrCb;
		int addrCr;
		int mvColBuf;
		vpu_mem_desc desc;
	};
#endif

	bool 	  	initialized_ ;
	unsigned	fourcc_ ;
	unsigned  	w_ ;
	unsigned  	h_ ;
	unsigned  	imgSize_ ;
        EncHandle 	handle_ ;
	PhysicalAddress phy_bsbuf_addr; /* Physical bitstream buffer */
	unsigned	virt_bsbuf_addr;		/* Virtual bitstream buffer */
	int 		picwidth;	/* Picture width */
	int 		picheight;	/* Picture height */
	unsigned	fbcount;	/* Total number of framebuffers allocated */
	FrameBuffer	*fb; /* frame buffer base given to encoder */
	unsigned char  **buffers ; /* mmapped */
	unsigned	yoffs ;
	unsigned	uoffs ;
	unsigned	voffs ;
	void	       *spsdata ;
	unsigned 	spslen ;
	void	       *ppsdata ;
	unsigned 	ppslen ;
};

#endif
