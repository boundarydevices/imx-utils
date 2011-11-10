#ifndef __V4L_DISPLAY_H__
#define __V4L_DISPLAY_H__

#include <stdint.h>
#include <linux/videodev.h>
extern "C" {
#include <vpu_lib.h>
#include <vpu_io.h>
};

class v4l_display_t {
public:
	v4l_display_t
		( unsigned picWidth,
                  unsigned picHeight,
                  Rect const &window,
		  unsigned numframes );
	~v4l_display_t (void);

	bool initialized (void) const { return 0 <= fd ; }
	unsigned numBufs (void) const { return nframes ; }

	unsigned imgSize(void)const { return ySize()+2*uvSize(); }
	unsigned ySize(void) const { return ysize ; }
	unsigned yStride(void) const { return ystride ; }

	unsigned uvSize(void) const { return uvsize ; }
	unsigned uvStride(void) const { return uvstride ; }

	void pollBufs(void);

	bool getBuf (unsigned &idx);
	void putBuf (unsigned idx);
	void *getY(unsigned idx) const { return vbufs[idx]; }
	void getFrameBuffers( FrameBuffer *&fbs, unsigned &count);

	int getFd (void) const { return fd ; }
private:
        v4l_display_t (v4l_display_t const &); // no copies
	enum {
		MAXFBS = 16
	};
	unsigned 	w ;
	unsigned 	h ;
	unsigned	ysize ;
	unsigned	ystride ;
	unsigned	uvsize ;
	unsigned	uvstride ;
	Rect		win ;
	unsigned	nframes ;
        FrameBuffer 	fbs[MAXFBS];
	int 	    	fd ;
        struct v4l2_buffer *bufs ;
	unsigned char  **vbufs ;
	unsigned	bufs_avail ;
	unsigned	numQueued ;
	bool		streaming ;
};

#endif
