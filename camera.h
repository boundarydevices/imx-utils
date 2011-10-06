#ifndef __CAMERA_H__
	#define __CAMERA_H__ "$Id$"

/*
 * camera.h
 *
 * This header file declares the camera_t class, which is
 * a thin wrapper around an mmap'd V4L2 device that returns
 * frames to the caller for processing.
 * 
 * Note that this class sets the camera file descriptor to
 * non-blocking. Use poll() or select() to wait for a frame.
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include <linux/videodev2.h>
#include <sys/poll.h>

class camera_t {
public:
	enum rotation_e {
		ROTATE_NONE = 0,
		FLIP_VERTICAL = 1,
		FLIP_HORIZONTAL = 2,
		FLIP_BOTH = 3,
		ROTATE_90_RIGHT = 4,
		ROTATE_90_LEFT = 7
	};
	camera_t( char const *devName,
		  unsigned    width,
		  unsigned    height,
		  unsigned    fps,
		  unsigned    pixelformat,
		  rotation_e  rotation = ROTATE_NONE );
	~camera_t(void);

	bool isOpen(void) const { return 0 <= fd_ ;}
	int getFd(void) const { return fd_ ;}

	unsigned getWidth(void) const { return w_ ;}
	unsigned getHeight(void) const { return h_ ;}
	unsigned stride(void) const { return fmt_.fmt.pix.bytesperline ;}
	unsigned imgSize(void) const { return fmt_.fmt.pix.sizeimage ;}
	unsigned numBuffers(void) const { return n_buffers_ ; }
        struct v4l2_buffer *v4l2_Buffers(void) const { return v4l_buffers_ ;}
	unsigned char **getBuffers(void) const { return buffers_ ; }

	// capture interface
	bool startCapture(void);

	// pull frames with this method
	bool grabFrame(void const *&data,int &index);

	// return them with this method
	void returnFrame(void const *data, int index);

	bool stopCapture(void);

	unsigned numRead(void) const { return numRead_ ;}
	unsigned numDropped(void) const { return frame_drops_ ;}
	unsigned lastRead(void) const { return lastRead_ ;}
private:
	int                     fd_ ;
	unsigned const          w_ ;
	unsigned const          h_ ;
	struct pollfd           pfd_ ;
	struct v4l2_format      fmt_ ;
        struct v4l2_buffer 	*v4l_buffers_ ;
	unsigned char	        **buffers_ ;
	unsigned                n_buffers_ ;
	unsigned		buffer_length_ ;
	unsigned        	numRead_ ;
	unsigned                frame_drops_ ;
	unsigned        	lastRead_ ;
};

#endif

