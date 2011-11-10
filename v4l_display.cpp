#include "v4l_display.h"
#include <linux/mxc_v4l2.h>
#include <linux/mxcfb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <strings.h>

v4l_display_t::v4l_display_t
        ( unsigned picWidth,
          unsigned picHeight,
          Rect const &window,
	  unsigned numframes )
	: w(picWidth)
	, h(picHeight)
	, ysize(0)
	, ystride(0)
	, uvsize(0)
	, uvstride(0)
	, win(window)
	, nframes(numframes)
	, fd(-1)
	, bufs(0)
	, bufs_avail(0)
	, numQueued(0)
	, streaming(0)
{
	memset(fbs,0,sizeof(fbs));
	ystride = ((picWidth+7)/8)*8 ;
	ysize = h*ystride ;
	uvstride = ystride/2 ;
	uvsize = h*uvstride/2 ;

	int out = 3;
	int fd_fb = open("/dev/fb0", O_RDWR, 0);
	if (fd_fb < 0) {
		printf("unable to open fb0\n");
		return ;
	}

	struct mxcfb_gbl_alpha alpha;
	alpha.alpha = 0;
	alpha.enable = 0;

	int err = ioctl(fd_fb, MXCFB_SET_GBL_ALPHA, &alpha);
	if (err < 0) {
		printf("set alpha blending failed\n");
		return ;
	}

	struct mxcfb_gbl_alpha a ;
	a.enable = 1;
	a.alpha = 255 ;
	err = ioctl(fd_fb,MXCFB_SET_GBL_ALPHA,&a);
	if ( err ) {
		perror( "MXCFB_SET_GBL_ALPHA");
		return ;
	}
	struct mxcfb_color_key key;
	key.enable = 1 ;
	key.color_key = 0 ;
	if (ioctl(fd_fb,MXCFB_SET_CLR_KEY, &key) <0) {
		perror("MXCFB_SET_CLR_KEY error!");
		return ;
	}

	close (fd_fb);
	char v4l_device[32] = "/dev/video16";
	fd = open(v4l_device, O_RDWR|O_NONBLOCK, 0);
	if (fd < 0) {
		printf("unable to open %s\n", v4l_device);
		return;
	}

	err = ioctl(fd, VIDIOC_S_OUTPUT, &out);
	if (err < 0) {
		printf("VIDIOC_S_OUTPUT failed\n");
		close(fd); fd = -1 ;
		return;
	}

	struct v4l2_crop vcrop ;
	memset(&vcrop,0,sizeof(vcrop));
	vcrop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	vcrop.c.top = window.top;
	vcrop.c.left = window.left;
	vcrop.c.width = window.right-window.left;
	vcrop.c.height = window.bottom-window.top;
	err = ioctl(fd, VIDIOC_S_CROP, &vcrop);
	if (err < 0) {
		printf("VIDIOC_S_CROP failed: %u:%u..%ux%u\n",vcrop.c.top,vcrop.c.left,vcrop.c.width,vcrop.c.height);
		close(fd); fd = -1 ;
		return;
	}

	struct v4l2_format fmt ;
	memset(&fmt,0,sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	fmt.fmt.pix.width = w;
	fmt.fmt.pix.height = h;
	fmt.fmt.pix.bytesperline = w;
	err = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (err < 0) {
		printf("VIDIOC_S_FMT failed\n");
		close(fd); fd = -1 ;
		return;
	}

	err = ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (err < 0) {
		printf("VIDIOC_G_FMT failed\n");
		close(fd); fd = -1 ;
		return;
	}

	struct v4l2_requestbuffers reqbuf = {0};
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = nframes;

	err = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	if ((err == 0) && (reqbuf.count == nframes)) {
		bufs = new struct v4l2_buffer [nframes];
		vbufs = new unsigned char *[nframes];
		unsigned i ;
		for (i = 0; i < nframes; i++) {
			struct v4l2_buffer &buffer = bufs[i];
			buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			buffer.memory = V4L2_MEMORY_MMAP;
			buffer.index = i;

			err = ioctl(fd, VIDIOC_QUERYBUF, &buffer);
			if (err < 0) {
				printf("VIDIOC_QUERYBUF, not enough buffers\n");
				close(fd); fd = -1 ;
				return;
			}

			vbufs[i] = (unsigned char *)
				   mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);

			if (vbufs[i] == (unsigned char *)MAP_FAILED) {
				printf("mmap failed\n");
				vbufs[i] = 0 ;
				break;
			}
			memset(vbufs[i],0x80,imgSize());
			bufs_avail |= (1<<i);

			FrameBuffer &fb = fbs[i];
			fb.strideY = w ;
			fb.strideC = w/2 ;
			fb.bufY = buffer.m.offset ;
			fb.bufCb = fb.bufY + ySize();
			fb.bufCr = fb.bufCb + uvSize();
			fb.bufMvCol = 0 ;
		}

		if (nframes == i) {
			return ;
		}
	} else {
		printf("VIDIOC_REQBUFS, not enough buffers: %d/%d/%d\n",err,reqbuf.count,nframes);
	}
	close(fd); fd = -1 ;
}

v4l_display_t::~v4l_display_t (void)
{
	if (0 <= fd){
		if (streaming) {
			printf("calling STREAMOFF\n");
			int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			int err = ioctl(fd, VIDIOC_STREAMOFF, &type);
			if (err < 0) {
				printf("VIDIOC_STREAMOFF failed:%d\n",err);
			}
			streaming = false ;
		}
		if (bufs && vbufs) {
			unsigned i ;
			for (i = 0; i < nframes; i++) {
				if (vbufs[i])
					munmap(vbufs[i], bufs[i].length);
			}
			delete [] vbufs ;
			delete [] bufs ;
		}
		close(fd);
	}
}

void v4l_display_t::pollBufs(void)
{
	while (1) {
		struct v4l2_buffer buffer ; memset(&buffer,0,sizeof(buffer));
		buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffer.memory = V4L2_MEMORY_MMAP;
		int err = ioctl(fd, VIDIOC_DQBUF, &buffer);
		if (err < 0)
			break;

                // printf("phys: 0x%08x, idx %u\n", buffer.m.offset, buffer.index);
		assert (buffer.index < nframes);
		assert (buffer.m.offset = bufs[buffer.index].m.offset);
		unsigned mask = (1<<buffer.index);
		assert (0 == (bufs_avail&mask));
		bufs_avail |= (1<<buffer.index);
		numQueued-- ;
	}
}

bool v4l_display_t::getBuf (unsigned &idx)
{
	idx = 0 ;
	pollBufs();
	if (0 != bufs_avail) {
		idx = ffs(bufs_avail);
		assert (idx);
		idx-- ;
		bufs_avail &= ~(1<<idx);
		assert (idx < nframes);
		return true ;
	}
	else
		return false ;
}
void v4l_display_t::putBuf (unsigned idx)
{
	assert (idx < nframes);
	gettimeofday(&bufs[idx].timestamp,0);
	int err = ioctl(fd, VIDIOC_QBUF, &bufs[idx]);
	if (err < 0) {
		printf("VIDIOC_QBUF failed\n");
	} else {
		bufs_avail &= ~(1<<idx);
		numQueued++ ;
		if (!streaming && (1 < numQueued)) {
			int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			err = ioctl(fd, VIDIOC_STREAMON, &type);
			if (err < 0) {
				printf("VIDIOC_STREAMON failed:%d\n",err);
			} else {
				streaming = true ;
			}
		}
	}
}

void v4l_display_t::getFrameBuffers( FrameBuffer *&bufs, unsigned &count)
{
	bufs = fbs ;
	count = nframes ;
}

#ifdef V4L_DISPLAY_MODULETEST

#include <sys/time.h>
#include <signal.h>
#include <sys/poll.h>
#include "tickMs.h"

static bool volatile die = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

int main (int argc, char const * const argv[])
{
	unsigned w = 800 ;
	unsigned h = 600 ;

	Rect window = {0};
	if (1 < argc) {
		unsigned x,y ;
		if (4 == sscanf(argv[1],"%ux%u+%u+%u", &w,&h,&x,&y)) {
			window.top = y ;
			window.left = x ;
			window.right = x+w ;
			window.bottom = y+h ;
		} else {
			fprintf (stderr, "invalid spec: use WxH+x+y\n");
			return -1 ;
		}
	}  else {
		window.right = w ;
		window.bottom = h ;
	}
	signal(SIGINT, ctrlcHandler);
	while (!die) {
                v4l_display_t display(w,h,window,3);
		if (display.initialized()) {
			printf("display initialized\n" );
			unsigned char i = 128 ;
			long long start = tickMs();
			while (i) {
				unsigned idx ;
				if (display.getBuf(idx)) {
					memset(display.getY(idx),i++,display.ySize());
					display.putBuf(idx);
					printf("%d", idx); fflush(stdout);
				} else {
					struct pollfd pfd ;
					pfd.fd = display.getFd();
					pfd.events = POLLIN ;
					if (0 == poll(&pfd,1,1000)) {
						printf("no buffers\n");
						return -1 ;
					}
				}
			}
			long long end = tickMs();
			printf("\n");
			unsigned long ms = end-start;
			printf("128 frames in %lu ms (%lu fps)\n", ms,(128*1000)/ms);
		} else {
			printf("error initializing display\n");
			break;
		}
	}
	return 0 ;
}

#endif
