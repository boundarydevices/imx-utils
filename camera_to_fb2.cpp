/*
 * Progra v4l_camera.cpp
 *
 * This program is a test combination of the camera_t
 * class and the v4l_overlay_t class.
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "fb2_overlay.h"
#include "camera.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "fourcc.h"
#include <signal.h>

#ifndef ANDROID
#include "imx_vpu.h"
#include "imx_mjpeg_encoder.h"
#endif

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

bool doCopy = true ;
static char const *fileName = 0 ;
static bool saveJPEG = false ;

class stringSplit_t {
public:
        enum {
                MAXPARTS = 16 
        };

        stringSplit_t( char *line );

        unsigned getCount(void){ return count_ ;}
        char const *getPtr(unsigned idx){ return ptrs_ [idx];}

private:
        stringSplit_t(stringSplit_t const &);

        unsigned count_ ;
        char    *ptrs_[MAXPARTS];
};

stringSplit_t::stringSplit_t( char *line )
: count_( 0 )
{
        while ( (count_ < MAXPARTS) && (0 != *line) ) {
                ptrs_[count_++] = line++ ;
                while ( isgraph(*line) )
                        line++ ;
                if ( *line ) {
                        *line++ = 0 ;
                        while ( isspace(*line) )
                                line++ ;
                }
                else
                        break;
        }
}

static bool getFraction(char const *cFrac, unsigned max, unsigned &offs ){
        unsigned numerator ;
        if ( isdigit(*cFrac) ) {
                numerator = 0 ;
                while (isdigit(*cFrac)) {
                        numerator *= 10 ;
                        numerator += (*cFrac-'0');
                        cFrac++ ;
                }
        }
        else
                numerator = 1 ;

        if ( '/' == *cFrac ) {
                cFrac++ ;
                unsigned denominator = 0 ;
                while ( isdigit(*cFrac) ) {
                        denominator *= 10 ;
                        denominator += (*cFrac-'0');
                        cFrac++ ;
                }
                if ( denominator && (numerator <= denominator)) {
                        offs = (max*numerator)/denominator ;
                        return true ;
                }
        }
        else if ( '\0' == *cFrac ) {
                offs = numerator ;
                return true ;
        }

        return false ;
}

static void trimCtrl(char *buf){
        char *tail = buf+strlen(buf);
        // trim trailing <CR> if needed
        while ( tail > buf ) {
                --tail ;
                if ( iscntrl(*tail) ) {
                        *tail = '\0' ;
                }
                else
                        break;
        }
}

#include <linux/fb.h>
#include <sys/ioctl.h>
#include "cameraParams.h"

static bool volatile doExit = false ;

static void process_command(char *cmd,fb2_overlay_t *&overlay,cameraParams_t &params)
{
        trimCtrl(cmd);
        stringSplit_t split(cmd);
        if ( 0 < split.getCount() ) {
                switch (tolower(split.getPtr(0)[0])) {
                        case 'c': {
                            doCopy = !doCopy ;
                            printf( "%scopying frames to overlay\n", doCopy ? "" : "not " );
                            break;
                        }
                        case 'f': {
                                        struct fb_var_screeninfo variable_info;
                                        int err = ioctl( overlay->getFd(), FBIOGET_VSCREENINFO, &variable_info );
                                        if ( 0 == err ) {
                                                variable_info.yoffset = (0 != variable_info.yoffset) ? 0 : variable_info.yres ;
                                                err = ioctl( overlay->getFd(), FBIOPAN_DISPLAY, &variable_info );
                                                if ( 0 == err ) {
                                                        printf( "flipped to offset %d\n", variable_info.yoffset );
                                                }
                                                else
                                                        perror( "FBIOPAN_DISPLAY" );
                                        }
                                        else
                                                perror( "FBIOGET_VSCREENINFO");
                                        break;
                                }
                        case 'y': {
                                        if ( 1 < split.getCount()) {
                                                unsigned yval = strtoul(split.getPtr(1),0,0);
                                                unsigned start = 0 ;
                                                unsigned end = overlay->getMemSize() ;
                                                if ( 2 < split.getCount() ) {
                                                        if ( !getFraction(split.getPtr(2),overlay->getMemSize(),start) ) {
                                                                fprintf(stderr, "Invalid fraction %s\n", split.getPtr(2));
                                                                break;
                                                        }
                                                        if ( 3 < split.getCount() ) {
                                                                if ( !getFraction(split.getPtr(3),overlay->getMemSize(),end) ) {
                                                                        fprintf(stderr, "Invalid fraction %s\n", split.getPtr(3));
                                                                        break;
                                                                }
                                                        }
                                                }
                                                if ( (end > start) && (end <= overlay->getMemSize()) ) {
                                                        printf( "set y buffer [%u..%u] out of %u to %u (0x%x) here\n", start, end, overlay->getMemSize(), yval, yval );
                                                        memset(((char *)overlay->getMem())+start,yval,end-start);
                                                }
                                        }
                                        else
                                                fprintf(stderr, "Usage: y yval [start [end]]\n" );
                                        break;
                                }
                        case 's': {
				if (1 < split.getCount()) {
					saveJPEG = false ;
					fileName = strdup(split.getPtr(1));
				}
				break;
			}
                        case 'j': {
				if (1 < split.getCount()) {
					saveJPEG = true ;
					fileName = strdup(split.getPtr(1));
				}
				break;
			}
                        case 'x': {
				close(overlay->getFd());
				doExit = true ;
				break;
			}
                        case 'r': {
				delete overlay ;
				unsigned color_key ;
				if (!params.getPreviewColorKey(color_key))
					color_key = 0xFFFFFF ;
				overlay = new fb2_overlay_t
						(params.getPreviewX(),
						 params.getPreviewY(),
						 params.getPreviewWidth(),
						 params.getPreviewHeight(),
						 params.getPreviewTransparency(),
						 color_key,
						 params.getCameraFourcc());
				break;
			}
                        case '?': {
                                        printf( "available commands:\n"
                                                "\tf	- flip buffers\n" 
                                                "\tc	- toggle copy\n" 
                                                "\ty yval [start [end]] - set y buffer(s) to specified value\n" 
                                                "\ts filename - save raw data to filename\n" 
                                                "\tj filename - save JPEG data to filename\n" 
                                                "\tr 	- reopen display\n"
                                                "\n"
                                                "most start and end positions can be specified in fractions.\n" 
                                                "	/2 or 1/2 is halfway into buffer or memory\n" 
                                                "	/4 or 1/4 is a quarter of the way into buffer or memory\n" 
                                              );
                                }
                }
        }
}

#include <sys/poll.h>
#include "tickMs.h"
#include <assert.h>

class yuvAccess_t {
public:
	yuvAccess_t(unsigned fourcc, unsigned w, unsigned h, void *mem);
	~yuvAccess_t(){}

	bool initialized(void){ return 0 != yuv ; }

	unsigned char &y(unsigned x,unsigned y);
	unsigned char &u(unsigned x,unsigned y);
	unsigned char &v(unsigned x,unsigned y);

private:
	unsigned char *yuv ;
	unsigned const width ;
	unsigned const height ;
	unsigned ysize;
	unsigned yoffs;
	unsigned yadder;
	unsigned uvsize;
	unsigned uvrowdiv;
	unsigned uvcoldiv;
	unsigned uoffs; 
	unsigned voffs; 
	unsigned uvadder;
	unsigned totalsize;
	unsigned uvStride ;
};

yuvAccess_t::yuvAccess_t(unsigned fourcc, unsigned w, unsigned h, void *mem)
	: yuv((unsigned char *)mem)
	, width(w)
	, height(h)
{
	if (fourccOffsets(fourcc,w,h,ysize,yoffs,yadder,uvsize,uvrowdiv,uvcoldiv,uoffs,voffs,uvadder,totalsize)) {
		uvStride = width/uvcoldiv ;
	} else
		yuv = 0 ;
}

unsigned char &yuvAccess_t::y(unsigned x,unsigned y)
{
	assert(x<width);
	assert(y<height);
	unsigned offset = yoffs+((y*width)+x)*yadder;
	assert(offset < totalsize);
	return yuv[offset];
}

unsigned char &yuvAccess_t::u(unsigned x,unsigned y)
{
	assert(x<width);
	assert(y<height);
	x /= uvcoldiv ;
	y /= uvrowdiv ;
	unsigned offset = uoffs+((y*uvStride)+x)*uvadder;
	if(offset < totalsize) {
		return yuv[offset];
	} else {
		printf( "%s:Invalid offset %u > max %u for [%u:%u]: uoffs %u, width %u, uvrowdiv %u, uvcoldiv %u, uvadder %u\n", __func__, offset, totalsize, x, y, uoffs, width, uvrowdiv, uvcoldiv, uvadder);
		return yuv[0];
	}
}

unsigned char &yuvAccess_t::v(unsigned x,unsigned y)
{
	assert(x<width);
	assert(y<height);
	x /= uvcoldiv ;
	y /= uvrowdiv ;
	unsigned offset = voffs+((y*uvStride)+x)*uvadder;
	if(offset < totalsize) {
		return yuv[offset];
	} else {
		printf( "%s:Invalid offset %u > max %u for [%u:%u]: voffs %u, width %u, uvrowdiv %u, uvcoldiv %u, uvadder %u\n", __func__, offset, totalsize, x, y, voffs, width, uvrowdiv, uvcoldiv, uvadder);
		return yuv[0];
	}
}

#ifdef ANDROID
extern "C" {
	void memcopy(void *dest,void const *src,unsigned bytes);
};
#define	MEMCOPY memcopy
#else
#define	MEMCOPY memcpy
#endif

static void phys_to_fb2
	( void const     *cameraMem,
	  unsigned	  cameraMemSize,
	  fb2_overlay_t  &overlay,
	  cameraParams_t &params )
{
	if ((params.getCameraWidth() == params.getPreviewWidth())
		   &&
		   (params.getCameraHeight() == params.getPreviewHeight())
		   &&
		   (params.getCameraFourcc() == params.getPreviewFourcc())) {
		MEMCOPY(overlay.getMem(),cameraMem,cameraMemSize);
	} else if (V4L2_PIX_FMT_YUYV == params.getCameraFourcc()) {
		unsigned camera_bpl = params.getCameraWidth() * 2 ;
		unsigned char const *cameraIn = (unsigned char *)cameraMem ;
		unsigned char *fbOut = (unsigned char *)overlay.getMem();
		unsigned fb_bpl = 2*params.getPreviewWidth();
		unsigned maxWidth = params.getPreviewWidth()>params.getCameraWidth() ? params.getCameraWidth() : params.getPreviewWidth();
		unsigned hskip = ((2*params.getPreviewWidth()) <= params.getCameraWidth()) ? params.getCameraWidth()/params.getPreviewWidth() : 0 ;
		unsigned vskip = (params.getPreviewHeight() < params.getCameraHeight())
				? (params.getCameraHeight()+params.getPreviewHeight()-1) / params.getPreviewHeight() 
				: 1 ;
		for( unsigned y = 0 ; y < params.getCameraHeight(); y += vskip ){
			if((y/vskip) >= params.getPreviewHeight())
				break;
			if(hskip) {
				for( unsigned mpix = 0 ; mpix*2 < params.getCameraWidth(); mpix++ ){
					unsigned inoffs = mpix*4*hskip ;
					unsigned outoffs = mpix*4 ;
					memcpy(fbOut+outoffs,cameraIn+inoffs,4); // one macropix
				}
			} else
				memcpy(fbOut,cameraIn,2*maxWidth);
	
			cameraIn += vskip*camera_bpl ;
			fbOut += fb_bpl ;
		}
	} else {
		yuvAccess_t yuvRead(params.getCameraFourcc(),params.getCameraWidth(),params.getCameraHeight(),(void *)cameraMem);
		yuvAccess_t yuvWrite(params.getPreviewFourcc(),params.getPreviewWidth(),params.getPreviewHeight(),overlay.getMem());
		unsigned hskip = ((2*params.getPreviewWidth()) <= params.getCameraWidth()) ? params.getCameraWidth()/params.getPreviewWidth() : 1 ;
		unsigned vskip = (params.getPreviewHeight() < params.getCameraHeight())
				? (params.getCameraHeight()+params.getPreviewHeight()-1) / params.getPreviewHeight() 
				: 1 ;
		for (unsigned iny = 0 ; iny < params.getCameraHeight(); iny += vskip) {
			unsigned outy = iny/vskip ;
			if (outy >= params.getPreviewHeight())
				break;
			for (unsigned inx = 0 ; inx < params.getCameraWidth(); inx += hskip ) {
				unsigned outx = inx/hskip ;
				if (outx >= params.getPreviewWidth())
					break;
				yuvWrite.y(outx,outy) = yuvRead.y(inx,iny);
				yuvWrite.u(outx,outy) = yuvRead.u(inx,iny);
				yuvWrite.v(outx,outy) = yuvRead.v(inx,iny);
			}
		}
	}
}

static void ctrlcHandler( int signo )
{
	printf( "<ctrl-c>(%d)\r\n", signo );
	doExit = true ;
}


int main( int argc, char const **argv ) {
#ifndef ANDROID
	vpu_t vpu ;
        mjpeg_encoder_t *encoder = 0 ;
#endif
	cameraParams_t params(argc,argv);
	params.dump();
	unsigned color_key ;
	if (!params.getPreviewColorKey(color_key))
		color_key = 0xFFFFFF ;

	signal( SIGINT, ctrlcHandler );
	signal( SIGHUP, ctrlcHandler );
	printf("installed int handler\n");
        printf( "format %s\n", fourcc_str(params.getCameraFourcc()));
        fb2_overlay_t *overlay = new fb2_overlay_t
			(params.getPreviewX(),
			 params.getPreviewY(),
			 params.getPreviewWidth(),
			 params.getPreviewHeight(),
			 params.getPreviewTransparency(),
			 color_key,
			 params.getCameraFourcc());
        if ( overlay->isOpen() ) {
                printf( "overlay opened successfully: %p/%u\n", overlay->getMem(), overlay->getMemSize() );
                camera_t camera("/dev/video0",params.getCameraWidth(),
				params.getCameraHeight(),params.getCameraFPS(),
				params.getCameraFourcc());
                if (camera.isOpen()) {
                        printf( "camera opened successfully\n");
                        if ( camera.startCapture() ) {
                                printf( "camera streaming started successfully\n");
                                printf( "cameraSize %u, overlaySize %u\n", camera.imgSize(), overlay->getMemSize() );
                                unsigned long frameCount = 0 ;
                                unsigned totalFrames = 0 ;
                                unsigned outDrops = 0 ;
                                long long start = tickMs();
                                while (!doExit) {
                                        void const *camera_frame ;
                                        int index ;
                                        if ( camera.grabFrame(camera_frame,index) ) {
						if ((0 != fileName) 
						    || 
						    ((int)totalFrames == params.getSaveFrameNumber())) {
							fileName = (0 == fileName) ? "/tmp/camera.out" : fileName ;
							printf( "saving %u bytes of img %u to %s\n", camera.imgSize(), index, fileName );
							FILE *fOut = fopen( fileName, "wb" );
							if( fOut ) {
								if (0 == saveJPEG) {
									fwrite(camera_frame,1,camera.imgSize(),fOut);
								} else {
#ifndef ANDROID
									if (0 == encoder) {
										encoder = new mjpeg_encoder_t(
												vpu,
												params.getCameraWidth(),
												params.getCameraHeight(),
												params.getCameraFourcc(),
												camera.getFd(),
												camera.numBuffers(),
												camera.getBuffers()
										);
									}
									if (encoder && encoder->initialized()) {
										void const *outData ;
										unsigned    outLength ;
										if( encoder->encode(index, outData,outLength) ){
											fwrite(outData,1,outLength,fOut);
											printf( "JPEG data saved\n" );
										} else
											perror( "encode error");
									} else
#endif
										perror( "invalid MJPEG encoder\n");
								}
								fclose(fOut);
								printf("done\n");
								fflush(stdout);
							} else
								perror("/tmp/camera.out" );
							fileName = 0 ;
						}
                                                ++totalFrames ;
                                                ++frameCount ;
						phys_to_fb2(camera_frame,camera.imgSize(),*overlay,params);
                                                camera.returnFrame(camera_frame,index);
                                        }
//                                        if (isatty(0)) {
                                                struct pollfd fds[1];
                                                fds[0].fd = fileno(stdin); // STDIN
                                                fds[0].events = POLLIN|POLLERR;
                                                int numReady = poll(fds,1,0);
                                                if ( 0 < numReady ) {
                                                        char inBuf[512];
                                                        if ( fgets(inBuf,sizeof(inBuf),stdin) ) {
                                                                trimCtrl(inBuf);
                                                                process_command(inBuf, overlay,params);
                                                                long long elapsed = tickMs()-start;
                                                                if ( 0LL == elapsed )
                                                                        elapsed = 1 ;
								unsigned whole_fps = (frameCount*1000)/elapsed ;
								unsigned frac_fps = ((frameCount*1000000)/elapsed)%1000 ;
                                                                printf( "%lu frames, start %llu, elapsed %llu %u.%03u fps. %u dropped\n", 
									frameCount, start, elapsed, whole_fps, frac_fps, camera.numDropped() );
                                                                frameCount = 0 ; start = tickMs();
                                                        }
                                                        else {
                                                                printf( "[eof]\n");
                                                                break;
                                                        }
                                                }
//                                        }
                                }
                        }
                        else
                                fprintf(stderr, "Error starting capture\n" );
                }
                else
                        fprintf(stderr, "Error opening camera\n" );
        }
        else
                fprintf(stderr, "Error opening v4l output\n" );
        return 0 ;
}

