/*
 * Module camera.cpp
 *
 * This module defines the methods of the camera_t class
 * as declared in camera.h
 *
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2010
 */


#include "camera.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <stdint.h>
//#include <linux/mxc_v4l2.h>
#include "fourcc.h"

// #define DEBUGPRINT
#include "debugPrint.h"

static int xioctl(int fd, int request, void *arg)
{
	int r;
//		printf("%s: request %x\n", __func__, request);
//		fflush (stdout);

	do {
		r = ioctl (fd, request, arg);
	} while (-1 == r && EINTR == errno);
//		printf("%s: result %x\n", __func__, r);
//		fflush (stdout);
//		usleep(500);
//		printf("%s: return %d\n", __func__, r);
	return r;
}

camera_t::camera_t
( char const *devName,
  unsigned    width,
  unsigned    height,
  unsigned    fps,
  unsigned    pixelformat,
  rotation_e  rotation )
: fd_( -1 )
, w_(width)
, h_(height)
, v4l_buffers_(0)
, buffers_(0)
, n_buffers_(0)
, buffer_length_(0)
, numRead_(0)
, frame_drops_(0)
, lastRead_(0xffffffff)
{
	struct stat st;

	if (-1 == stat (devName, &st)) {
		ERRMSG( "Cannot identify '%s': %d, %s\n",
			devName, errno, strerror (errno));
		return ;
	}

	if (!S_ISCHR (st.st_mode)) {
		ERRMSG( "%s is no device\n", devName );
		return ;
	}

	fd_ = open (devName, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (0 > fd_) {
		ERRMSG( "Cannot open '%s': %d, %s\n",
			devName, errno, strerror (errno));
		return ;
	}

	struct v4l2_capability cap;
	if (-1 == xioctl (fd_, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			ERRMSG( "%s is no V4L2 device\n", devName);
		}
		else {
			ERRMSG( "VIDIOC_QUERYCAP:%m");
		}
		goto bail ;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		ERRMSG( "%s is no video capture device\n", devName);
		goto bail ;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		ERRMSG( "%s does not support streaming i/o\n", devName);
		goto bail ;
	}

	int input ;
	int r;

	input = isYUV(pixelformat) ? 0 : 1 ;

	/* Select video input, video standard and tune here. */
	r = xioctl (fd_, VIDIOC_S_INPUT, &input);
	if (r) {
		ERRMSG( "%s does not support input#%i, ret=0x%x\n", devName, input, r);
//		goto bail ;
	}

	struct v4l2_cropcap cropcap ; memset(&cropcap,0,sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl (fd_, VIDIOC_CROPCAP, &cropcap)) {
		struct v4l2_crop crop ; memset(&crop,0,sizeof(crop));
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl (fd_, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	}
	else {
		/* Errors ignored. */
	}

	memset(&fmt_,0,sizeof(fmt_));

	fmt_.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt_.fmt.pix.width       = width ;
	fmt_.fmt.pix.height      = height ;
	fmt_.fmt.pix.pixelformat = pixelformat ;

	if (-1 == xioctl (fd_, VIDIOC_S_FMT, &fmt_)) {
		perror("VIDIOC_S_FMT");
		goto bail ;
	}

	ERRMSG("%s: set pixel format %s, sizeimage == %u\n", __func__, fourcc_str(fmt_.fmt.pix.pixelformat), fmt_.fmt.pix.sizeimage);

	if ( (width != fmt_.fmt.pix.width)
	     ||
	     (height != fmt_.fmt.pix.height) ) {
		ERRMSG( "%ux%u not supported: %ux%u\n", width, height,fmt_.fmt.pix.width, fmt_.fmt.pix.height);
		goto bail ;
	}

	ERRMSG("%s: size: %ux%u\n", __func__, fmt_.fmt.pix.width, fmt_.fmt.pix.height);

/*
	struct v4l2_control rotate_control ; memset(&rotate_control,0,sizeof(rotate_control));
	rotate_control.id = V4L2_CID_MXC_ROT ;
	rotate_control.value = rotation ;
	if ( 0 > xioctl(fd_, VIDIOC_S_CTRL, &rotate_control) ) {
		perror( "VIDIOC_S_CTRL(rotation)");
		goto bail ;
	}
*/
	struct v4l2_streamparm stream_parm;

	if (-1 == xioctl (fd_, VIDIOC_G_PARM, &stream_parm)) {
		perror("VIDIOC_G_PARM");
//		goto bail ;
	}

	stream_parm.parm.capture.timeperframe.numerator = 1;
	stream_parm.parm.capture.timeperframe.denominator = (fps? fps : 30);
	if (-1 == xioctl (fd_, VIDIOC_S_PARM, &stream_parm))
		perror ("VIDIOC_S_PARM");

	struct v4l2_requestbuffers req ; memset(&req,0,sizeof(req));

	req.count       = 4;
	req.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory      = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd_, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			ERRMSG( "%s does not support memory mapping\n", devName);
		}
		else {
			perror("VIDIOC_REQBUFS");
		}
		goto bail ;
	}

	if (req.count < 2) {
		ERRMSG( "Insufficient buffer memory on %s\n", devName);
		goto bail ;
	}

        v4l_buffers_ = (struct v4l2_buffer *)calloc (req.count, sizeof(v4l_buffers_[0]));
	if (!v4l_buffers_) {
		ERRMSG( "Out of memory\n");
		goto bail ;
	}

	buffers_ = (unsigned char **)calloc (req.count, sizeof (buffers_[0]));

	if (!buffers_) {
		ERRMSG( "Out of memory\n");
		goto bail ;
	}

	for (n_buffers_ = 0; n_buffers_ < req.count; ++n_buffers_) {
		struct v4l2_buffer &buf = v4l_buffers_[n_buffers_];
		memset(&buf,0,sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers_ ;

		if (-1 == xioctl (fd_, VIDIOC_QUERYBUF, &buf)) {
			perror ("VIDIOC_QUERYBUF");
			goto bail; 
		}
ERRMSG("%s: buffer lengths %u/%u\n", __func__, buffer_length_,buf.length);
		assert((0 == buffer_length_)||(buf.length == buffer_length_)); // only handle single buffer size
                buffer_length_ = buf.length;
		buffers_[n_buffers_] = (unsigned char *)
		mmap (NULL /* start anywhere */,
		      buf.length,
		      PROT_READ | PROT_WRITE /* required */,
		      MAP_SHARED /* recommended */,
		      fd_, buf.m.offset);

		if (MAP_FAILED == buffers_[n_buffers_]) {
			perror("mmap");
			goto bail ;
		}

		memset(buffers_[n_buffers_], 0, fmt_.fmt.pix.sizeimage);
		if (fmt_.fmt.pix.sizeimage > buf.length)
			ERRMSG("camera_imgsize=%x but buf.length=%x\n", fmt_.fmt.pix.sizeimage, buf.length);
	}
	pfd_.fd = fd_ ;
	pfd_.events = POLLIN ;

	return ;

bail:
	close(fd_); fd_ = -1 ;

}

camera_t::~camera_t(void) {
	if ( buffers_ ) {
		while ( 0 < n_buffers_ ) {
			munmap(buffers_[n_buffers_-1],buffer_length_);
			--n_buffers_ ;
		}
		free(buffers_);
		buffers_ = 0 ;
	}
	if (v4l_buffers_) {
		free(v4l_buffers_);
                v4l_buffers_ = 0 ;
	}
	if (isOpen()) {
		close(fd_);
		fd_ = -1 ;
	}
}

// capture interface
bool camera_t::startCapture(void)
{
	if ( !isOpen() )
		return false ;

	unsigned int i;

	for (i = 0; i < n_buffers_; ++i) {
		struct v4l2_buffer buf ; memset(&buf,0,sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;
		buf.m.offset    = 0;

		if (-1 == xioctl (fd_, VIDIOC_QUERYBUF, &buf)) {
			perror ("VIDIOC_QUERYBUF");
			return false; 
		}
		if (-1 == xioctl (fd_, VIDIOC_QBUF, &buf)) {
			perror ("VIDIOC_QBUF");
			return false ;
		}
		else {
			debugPrint( "queued buffer %u\n", i );
		}
	}

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (fd_, VIDIOC_STREAMON, &type)) {
		perror ("VIDIOC_STREAMON");
		return false ;
	}
	else {
		debugPrint( "streaming started\n" );
	}
	return true ;
}

bool camera_t::grabFrame(void const *&data,int &index) {

	int timeout = 100 ; // 1/10 second max 
	index = -1 ;
	while (1) {
		int numReady = poll(&pfd_, 1, timeout);
		if ( 0 < numReady ) {
			debugPrint( "%s: %d fds ready\n", __func__, numReady );
			timeout = 0 ;
			struct v4l2_buffer buf ;
			memset(&buf,0,sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			int rv ;
			if (0 == (rv = xioctl (fd_, VIDIOC_DQBUF, &buf))) {
				++ numRead_ ;
				if (0 != (rv = xioctl (fd_, VIDIOC_QUERYBUF, &buf))) {
					fprintf(stderr, "QUERYBUF:%d:%m\n", rv);
				}
				if (0 <= index) {
					ERRMSG("camera frame drop\n");
					++frame_drops_ ;
					returnFrame(data,index);
				}
				assert (buf.index < n_buffers_);
				data = buffers_[buf.index];
				index = buf.index ;
				debugPrint( "DQ index %u: %p\n", index, data );
				lastRead_ = index ;
				break;
			}
			else if ((errno != EAGAIN)&&(errno != EINTR)) {
				ERRMSG("VIDIOC_DQBUF");
			}
			else {
				if (EAGAIN != errno)
					ERRMSG("%s: rv %d, errno %d\n", __func__, rv, errno);
			}
		}
		break; // continue from middle
	}
	debugPrint( "%s: returning %d\n", __func__,(0 <= index));
	return (0 <= index);
}

void camera_t::returnFrame(void const *data, int index) {
	struct v4l2_buffer buf ;
	memset(&buf,0,sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index  = index ;
	if (0 != xioctl (fd_, VIDIOC_QBUF, &buf))
		perror("VIDIOC_QBUF");
	else {
		// debugPrint( "returned frame %p/%d\n", data, index );
	}
}

bool camera_t::stopCapture(void){
	if ( !isOpen() )
		return false ;

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (fd_, VIDIOC_STREAMOFF, &type)) {
		perror ("VIDIOC_STREAMOFF");
		return false ;
	}
	else {
		debugPrint( "capture stopped\n" );
	}

	return true ;
}

#ifdef STANDALONE_CAMERA

#include <ctype.h>
#include "tickMs.h"
#include <sys/poll.h>
#include "cameraParams.h"
#include <signal.h>

bool volatile die = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>(%d)\r\n", signo );
   die = true ;
}

int main(int argc, char const **argv) {
	int rval = -1 ;
	cameraParams_t params(argc,argv);
	long long start = tickMs();
	signal( SIGINT, ctrlcHandler );
	camera_t camera(params.getCameraDeviceName(),
			params.getCameraWidth(),
			params.getCameraHeight(),
			params.getCameraFPS(),
			params.getCameraFourcc(),
			params.getCameraRotation());
	long long end = tickMs();
	if ( camera.isOpen() ) {
		printf( "camera opened in %llu ms\n",end-start);
		start = tickMs();
		if ( camera.startCapture() ) {
			end = tickMs();
			printf( "started capture in %llu ms\n", end-start);

			long long startCapture = end ;
			long long maxGrab = 0, maxRelease = 0 ;
			unsigned numFrames = 0 ;
			for ( int i = 0 ; !die && ((0 > params.getIterations()) || (i < params.getIterations())) ; i++ ) {
				start = tickMs();
				void const *data ;
				int         index ;
				while ( !(die || camera.grabFrame(data,index)) )
					;
				end = tickMs();
				if(die)
					break;
				++numFrames ;
				long long elapsed = end-start ;
				debugPrint( "frame %p:%d, %llu ms\n", data, index, elapsed );
				if ( elapsed > maxGrab )
					maxGrab = elapsed ;
				if(numFrames == params.getSaveFrameNumber()){
					char const outFileName[] = {
                                                "/tmp/camera.out"
					};
					FILE *fOut = fopen(outFileName, "wb");
					if(fOut){
						fwrite(data,camera.imgSize(),1,fOut);
						fclose(fOut);
						printf( "saved frame to %s\n", outFileName);
					}
					else
						perror(outFileName);
				}
				start = tickMs();
				camera.returnFrame(data,index);
				end = tickMs();
				elapsed = end-start ;
				if ( elapsed > maxRelease )
					maxRelease = elapsed ;
			}

			long long endCapture = start = tickMs();
			if ( camera.stopCapture() ) {
				end=tickMs();
				printf( "closed capture in %llu ms\n", end-start);
				printf( "maxGrab: %llu ms, maxRelease: %llu ms\n", maxGrab, maxRelease );
				unsigned long elapsed = (endCapture-startCapture);
				printf( "%u frames in %lu ms (%u fps)\n", numFrames, elapsed, (numFrames*1000)/elapsed );
				rval = 0 ;
			}
			else
				ERRMSG( "Error stopping capture\n" );
		}
		else
			ERRMSG( "Error starting capture\n" );
	}

	return rval ;
}

#endif
