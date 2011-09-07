#ifndef __CAMERAPARAMS_H__
#define __CAMERAPARAMS_H__ "$Id$"

/*
 * cameraParams.h
 *
 * This header file declares the cameraParams_t class, which wraps 
 * a set of parameters for camera reading, preview, and encoding.
 *
 * It's mostly used by test programs that share a requirement for
 * options to control:
 *
 *		input width, height, color-space, and rotation
 *		preview width, height, position, transparency, and color-blending
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "camera.h"

class cameraParams_t {
public:
	cameraParams_t( int &argc, char const **&argv );
	~cameraParams_t( void ){}

	unsigned getCameraWidth(void) const { return inwidth ; }
	unsigned getCameraHeight(void) const { return inheight ; }
	camera_t::rotation_e getCameraRotation(void) const { return rotation ; }
	char const *getCameraDeviceName(void) const { return cameraDevName ; }
	unsigned getCameraFPS(void) const { return fps ; }
	unsigned getCameraFourcc(void) const { return fourcc ; }

	unsigned getGOP(void) const { return gopSize ; }

	unsigned getPreviewX(void) const { return x ; }
	unsigned getPreviewY(void) const { return y ; }
	unsigned getPreviewWidth(void) const { return outwidth ; }
	unsigned getPreviewHeight(void) const { return outheight ; }
	unsigned getPreviewFourcc(void) const { return fourcc ; }
	unsigned getPreviewTransparency(void) const { return transparency ; }
	char const *getPreviewDeviceName(void) const { return cameraDevName ; }
	bool getPreviewColorKey(unsigned &rgb16) const { rgb16=color_key ; return 0xFFFF >= color_key ; }

	int getSaveFrameNumber(void) const { return saveFrame ; }
	int getIterations(void) const { return iterations ; }

	// returns 0 for no broadcast
	unsigned getBroadcastAddr(void) const { return broadcastAddr ; }
	unsigned short getBroadcastPort(void) const { return broadcastPort ; }

	void dump(void) const ;
private:
	unsigned inwidth ;
	unsigned inheight ;
	camera_t::rotation_e rotation ;
	unsigned fps ;
	unsigned fourcc ;
	unsigned gopSize ;
	unsigned x ;
	unsigned y ;
	unsigned outwidth ;
	unsigned outheight ;
	unsigned transparency ;
	unsigned color_key ;
	char const *cameraDevName ;
	char const *previewDevName ;
	int saveFrame ;
	int iterations ;
        unsigned broadcastAddr ;
	unsigned short broadcastPort ;
};

#endif

