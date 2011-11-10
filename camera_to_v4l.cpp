/*
 * Progra v4l_camera.cpp
 *
 * This program is a test combination of the camera_t
 * class and the v4l_overlay_t class.
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "v4l_display.h"
#include "camera.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "fourcc.h"
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef ANDROID
#include "imx_vpu.h"
#include "imx_mjpeg_encoder.h"
#include "imx_h264_encoder.h"
#endif

#define ARRAY_SIZE(__arr) (sizeof(__arr)/sizeof(__arr[0]))

bool doCopy = true ;
static char const *fileName = 0 ;
static char *udpDest = 0 ;
sockaddr_in dest ;
static bool saveJPEG = false ;
static bool saveYUV = false ;
static bool saveH264 = false ;

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

static void process_command(char *cmd,v4l_display_t *&overlay,cameraParams_t &params)
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
                        case 's': {
				if (1 < split.getCount()) {
					saveYUV = true ;
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
                        case 'v': {
				if (1 < split.getCount()) {
					saveH264 = true ;
					fileName = strdup(split.getPtr(1));
				}
				break;
			}
                        case 'u': {
				if (1 < split.getCount()) {
					saveH264 = true ;
					udpDest = strdup(split.getPtr(1));
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
				Rect window ;
				window.top  = params.getPreviewX();
				window.left = params.getPreviewY();
				window.right  = params.getPreviewX()+params.getPreviewWidth();
				window.bottom = params.getPreviewY()+params.getPreviewHeight();
				overlay = new v4l_display_t
						( params.getCameraWidth(),
						  params.getCameraHeight(),
						  window, 6 );
				break;
			}
                        case '?': {
                                        printf( "available commands:\n"
                                                "\tf	- flip buffers\n" 
                                                "\tc	- toggle copy\n" 
                                                "\ty yval [start [end]] - set y buffer(s) to specified value\n" 
                                                "\ts filename - save raw data to filename\n" 
                                                "\tj filename - save JPEG data to filename\n" 
                                                "\tv filename - save H264 video to filename\n" 
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
	  v4l_display_t  &overlay,
	  cameraParams_t &params )
{
	unsigned idx ;
	if (overlay.getBuf(idx)) {
		memcpy (overlay.getY(idx), cameraMem, cameraMemSize);
		overlay.putBuf(idx);
	} else
		printf("%s: no bufs\n", __PRETTY_FUNCTION__ );
}

static void ctrlcHandler( int signo )
{
	printf( "<ctrl-c>(%d)\r\n", signo );
	doExit = true ;
}


int main( int argc, char const **argv ) {
#ifndef ANDROID
	vpu_t vpu ;
        mjpeg_encoder_t *jpeg_encoder = 0 ;
	h264_encoder_t *h264_encoder = 0 ;
#endif
	cameraParams_t params(argc,argv);
	params.dump();
	unsigned color_key ;
	if (!params.getPreviewColorKey(color_key))
		color_key = 0xFFFFFF ;
	FILE *fOut = 0 ;

	signal( SIGINT, ctrlcHandler );
	signal( SIGHUP, ctrlcHandler );
	printf("Updated version includes video support\n");
        printf( "format %s\n", fourcc_str(params.getCameraFourcc()));
        Rect window ;
	window.top  = params.getPreviewX();
	window.left = params.getPreviewY();
	window.right  = params.getPreviewX()+params.getPreviewWidth();
	window.bottom = params.getPreviewY()+params.getPreviewHeight();
        v4l_display_t *overlay = new v4l_display_t
		( params.getCameraWidth(),
                  params.getCameraHeight(),
                  window, 6 );
        if ( overlay->initialized() ) {
                printf( "overlay opened successfully\n");
                camera_t camera("/dev/video0",params.getCameraWidth(),
				params.getCameraHeight(),params.getCameraFPS(),
				params.getCameraFourcc(),
				params.getCameraRotation());
                if (camera.isOpen()) {
                        printf( "camera opened successfully\n");
                        if ( camera.startCapture() ) {
                                printf( "camera streaming started successfully\n");
                                printf( "cameraSize %u, overlaySize %u\n", camera.imgSize(), overlay->imgSize() );
                                unsigned long frameCount = 0 ;
                                unsigned totalFrames = 0 ;
                                unsigned outDrops = 0 ;
                                long long start = tickMs();
				int sockFd = -1 ;
                                while (!doExit) {
					if (overlay)
						overlay->pollBufs();

                                        void const *camera_frame ;
                                        int index ;
                                        if ( camera.grabFrame(camera_frame,index) ) {
#ifndef ANDROID
						if (saveH264) {
							h264_encoder = new h264_encoder_t(vpu,
											  params.getCameraWidth(),
											  params.getCameraHeight(),
											  params.getCameraFourcc(),
											  params.getGOP(),
											  camera.v4l2_Buffers(),
											  camera.numBuffers(),
											  camera.getBuffers());
							saveH264 = false ;
						}
#endif
						if (0 != udpDest) {
							char *port = strchr(udpDest,':');
							if (port) {
								*port++ = '\0' ;
								dest.sin_family = AF_INET ;
                                                                struct in_addr targetIP ;
                                                                inet_aton(udpDest, &targetIP);
								dest.sin_addr = targetIP ;
                                                                dest.sin_port = strtoul(port,0,0);
								sockFd = socket (AF_INET, SOCK_DGRAM, 0);
								if (0 <= sockFd) {
									int doit = 1 ;
									int result = setsockopt (sockFd, SOL_SOCKET, SO_BROADCAST, &doit, sizeof(doit));
									if( 0 != result )
										perror ("SO_BROADCAST");
								} else {
									perror ("socket");
								}
							} else {
								printf ("invalid ip/port. use form 192.168.0.100:0x2020\n");
							}
							free ((void *)udpDest);
							udpDest = 0 ;
						}
						if ((0 != fileName) 
						    || 
						    ((int)totalFrames == params.getSaveFrameNumber())) {
							fileName = (0 == fileName) ? "/tmp/camera.out" : fileName ;
							printf( "saving %u bytes of img %u to %s\n", camera.imgSize(), index, fileName );
							if (0 == fOut)
								fOut = fopen( fileName, "wb" );
							if( fOut ) {
								if (saveJPEG) {
#ifndef ANDROID
									if (0 == jpeg_encoder) {
										jpeg_encoder = new mjpeg_encoder_t(
												vpu,
												params.getCameraWidth(),
												params.getCameraHeight(),
												params.getCameraFourcc(),
												camera.getFd(),
												camera.numBuffers(),
												camera.getBuffers()
										);
									}
									if (jpeg_encoder && jpeg_encoder->initialized()) {
										void const *outData ;
										unsigned    outLength ;
										if( jpeg_encoder->encode(index, outData,outLength) ){
											saveJPEG = false ;
											fwrite(outData,1,outLength,fOut);
											printf( "JPEG data saved\n" );
										} else
											perror( "encode error");
									} else
#endif
										perror( "invalid MJPEG jpeg_encoder\n");
								}
								else if (saveYUV){
									fwrite(camera_frame,1,camera.imgSize(),fOut);
								}
								if (saveJPEG || saveYUV) {
									fclose(fOut);
									printf("done\n");
									fflush(stdout);
									saveJPEG = saveYUV = false ;
								}
							} else
								perror("/tmp/camera.out" );
							fileName = 0 ;
#ifndef ANDROID
						}
						if (h264_encoder) {
							saveH264 = false ;
							void const *outData ;
							unsigned    outLength ;
							bool iframe ;
							if (h264_encoder->encode(index,outData,outLength,iframe)) {
								if (iframe) {
									void const *spsdata ;
									unsigned sps_len ;
									void const *ppsdata ;
									unsigned pps_len ;
									if (h264_encoder->getSPS(spsdata,sps_len)
									    &&
									    h264_encoder->getPPS(ppsdata,pps_len)) {
										if (fOut) {
                                                                                        fwrite (spsdata,1,sps_len,fOut);
                                                                                        fwrite (ppsdata,1,pps_len,fOut);
										}
										if (0 <= sockFd) {
											if (0 <= sockFd) {
												int sent = sendto (sockFd,spsdata,sps_len,0,
                                                                                                                   (struct sockaddr *)&dest, sizeof(dest));
												if (sent != sps_len)
													perror ("send(sps)");
												sent = sendto (sockFd,ppsdata,pps_len,0,
                                                                                                                   (struct sockaddr *)&dest, sizeof(dest));
												if (sent != pps_len)
													perror ("send(pps)");
											}
										}
									}
								}
								if (fOut) {
									fwrite (outData,1,outLength,fOut);
								}
								if ((0 <= sockFd)&&(1<outLength)) {
									--outLength ;
									int sent = sendto (sockFd,(char *)outData+1,outLength,0,
											   (struct sockaddr *)&dest, sizeof(dest));
									if (sent != outLength)
										perror ("send(data)");
								}
								if (0) { // 0 == outLength) {
									printf("\n%u bytes\n", outLength);
								}
							} else
								fprintf (stderr, "encode error(%d): %p/%u\n", index,outData,outLength);
#endif
						}
                                                ++totalFrames ;
                                                ++frameCount ;
						phys_to_fb2(camera_frame,camera.imgSize(),*overlay,params);
                                                camera.returnFrame(camera_frame,index);
                                        }
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
	if (fOut) {
		fclose(fOut);
	}
	delete overlay ;
        return 0 ;
}

