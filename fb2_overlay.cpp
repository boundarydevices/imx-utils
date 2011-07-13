/*
 * Module fb2_overlay.cpp
 *
 * This module defines the methods of the fb2_overlay_t
 * class as declared in fb2_overlay.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "fb2_overlay.h"
#include <linux/videodev2.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/mxcfb.h>
#include <sys/errno.h>
#include "fourcc.h"

#define DEBUGPRINT
#include "debugPrint.h"

#ifndef ANDROID
#define DEVNAME "/dev/fb2"
#else
#define DEVNAME "/dev/graphics/fb2"
#endif

fb2_overlay_t::fb2_overlay_t(unsigned outx, unsigned outy,
                             unsigned outw, unsigned outh,
                             unsigned transparency, 
			     unsigned color_key,
                             unsigned long outformat,
			     unsigned which_display )
: fd_(open(DEVNAME,O_RDWR|O_NONBLOCK))
, mem_(MAP_FAILED)
, memSize_(0)
{
        if ( 0 > fd_ ) {
                ERRMSG(DEVNAME);
                return ;
        }
        fcntl( fd_, F_SETFD, FD_CLOEXEC );
        struct fb_fix_screeninfo fixed_info;
        int err = ioctl( fd_, FBIOGET_FSCREENINFO, &fixed_info);
        if ( 0 == err ) {
                struct fb_var_screeninfo variable_info;

                err = ioctl( fd_, FBIOGET_VSCREENINFO, &variable_info );
                if ( 0 == err ) {
                        if ((outw != variable_info.xres) 
                            || 
                            (outh != variable_info.yres)
                            ||
                            (outformat != variable_info.nonstd)) {
                                variable_info.xres = variable_info.xres_virtual = outw ;
                                variable_info.yres = outh ;
                                variable_info.yres_virtual = outh*2 ;
                                variable_info.nonstd = outformat ;
                                variable_info.bits_per_pixel = bits_per_pixel(outformat);
                                err = ioctl( fd_, FBIOPUT_VSCREENINFO, &variable_info );
                                if (err) {
                                        perror( "FBIOPUT_VSCREENINFO");
                                        close();
                                        return ;
                                }
                        } // need to change output size
                        struct mxcfb_pos pos ;
                        pos.x = outx ;
                        pos.y = outy ;
                        err = ioctl(fd_,MXCFB_SET_OVERLAY_POS, &pos);
                        if (err) {
                                perror("MXCFB_SET_OVERLAY_POS");
                                close(); 
                                return ;
                        }

                        struct mxcfb_gbl_alpha a ;
                        a.enable = (NO_TRANSPARENCY != transparency);
                        a.alpha = transparency ;
                        int err = ioctl(fd_,MXCFB_SET_GBL_ALPHA,&a);
                        if ( err ) {
                                perror( "MXCFB_SET_GBL_ALPHA");
                                close();
                                return ;
                        }
                        struct mxcfb_color_key key;       
                        key.enable = 0xFFFF >= color_key ;
                        key.color_key = color_key;
                        if (ioctl(fd_,MXCFB_SET_CLR_KEY, &key) <0) {
                                perror("MXCFB_SET_CLR_KEY error!");
                                ::close(fd_);
                                close();
                                return ;
                        }
                        
                        memSize_ = fixed_info.smem_len ;
                        mem_ = mmap( 0, fixed_info.smem_len, PROT_WRITE|PROT_WRITE, MAP_SHARED, fd_, 0 );
                        if ( MAP_FAILED != mem_ ) {
				unsigned value ;
                                printf( "mapped %u (0x%lx) bytes\n", fixed_info.smem_len, memSize_ );
                                err = ioctl( fd_, FBIOBLANK, VESA_NO_BLANKING );
                                if ( err ) {
                                        perror("FBIOBLANK");
                                        close();
                                        return ;
                                }
				if (ioctl(fd_,MXCFB_GET_FB_IPU_CHAN, &value) <0)
					perror("MXCFB_GET_FB_IPU_CHAN error!");
				else
					printf( "MXCFB_GET_FB_IPU_CHAN: %x\n", value );
				if (ioctl(fd_,MXCFB_GET_FB_IPU_DI, &value) <0)
					perror("MXCFB_GET_FB_IPU_DI error!");
				else
					printf( "MXCFB_GET_FB_IPU_DI: %x\n", value );
				if (ioctl(fd_,MXCFB_GET_DIFMT, &value) <0)
					perror("MXCFB_GET_DIFMT error!");
				else
					printf( "MXCFB_GET_DIFMT: %x\n", value );
				memset(mem_, 0x80, fixed_info.smem_len);
                        }
                        else
                                perror( "VSCREENINFO" );
                }
                else
                        perror( "FSCREENINFO" );
        }
        else
                perror(DEVNAME);
}

fb2_overlay_t::~fb2_overlay_t( void )
{
        if (isOpen())
                close();
}

void fb2_overlay_t::close( void ){
        if ( 0 <= fd_ ) {
                int err = ioctl( fd_, FBIOBLANK, VESA_POWERDOWN );
                if ( err )
                        perror("FBIOBLANK");
                if ( MAP_FAILED != mem_ ) {
                        err = munmap(mem_, memSize_);
                        if ( err )
                                perror( "munmap");
                }
                ::close(fd_);
                fd_ = -1 ;
        }
}

#ifdef OVERLAY_MODULETEST

#include <ctype.h>

unsigned x = 0 ; 
unsigned y = 0 ;
unsigned outw = 480 ;
unsigned outh = 272 ;
unsigned alpha = 0 ;
unsigned format = V4L2_PIX_FMT_NV12 ;
unsigned which_display = 0 ;
char const *inFile = 0 ;

static void parseArgs( int &argc, char const **argv )
{
        for ( int arg = 1 ; arg < argc ; arg++ ) {
                if ( '-' == *argv[arg] ) {
                        char const *param = argv[arg]+1 ;
                        char const cmdchar = tolower(*param);
                        if ( 'i' == cmdchar ) {
                            inFile = param+1 ;
                            printf( "input file is <%s>\n", inFile );
                        }
                        else if ( 'o' == cmdchar ) {
                                char const second = tolower(param[1]);
                                if ('w' == second) {
                                        outw = strtoul(param+2,0,0);
                                }
                                else if ('h'==second) {
                                        outh = strtoul(param+2,0,0);
                                }
                                else
                                        printf( "unknown output option %c\n",second);
                        }
                        else if ( 'x' == cmdchar ) {
                                x = strtoul(param+1,0,0);
                        }
                        else if ( 'y' == cmdchar ) {
                                y = strtoul(param+1,0,0);
                        }
                        else if (('a' == cmdchar)||('t' == cmdchar)) {
                                alpha  = strtoul(param+1,0,0);
                        }
                        else if ('d' == cmdchar){
                                which_display  = strtoul(param+1,0,0);
                        }
                        else if ( 'f' == cmdchar ) {
                                unsigned fcc ; 
                                if(supported_fourcc(param+1,fcc)){
                                    format = fcc ;
                                } else {
                                    fprintf(stderr, "Invalid format %s\n", param+1 );
                                    fprintf(stderr, "supported formats include:\n" );
                                    unsigned const *formats ; unsigned num_formats ;
                                    supported_fourcc_formats(formats,num_formats);
                                    while( num_formats-- ){
                                        fprintf(stderr, "\t%s\n", fourcc_str(*formats));
                                        formats++ ;
                                    }
                                    exit(1);
                                }
                        }
                        else
                                printf( "unknown option %s\n", param );

                        // pull from argument list
                        for ( int j = arg+1 ; j < argc ; j++ ) {
                                argv[j-1] = argv[j];
                        }
                        --arg ;
                        --argc ;
                }
        }
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

        #define V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G') /*  8  GRGR.. BGBG.. */

int main( int argc, char const **argv ) {
        parseArgs(argc,argv);

        printf( "%ux%u on /dev/fb%u\n", outw, outh, which_display );

        fb2_overlay_t overlay(x,y,outw,outh,alpha,format,which_display); // V4L2_PIX_FMT_SGRBG8 ; // 
        if ( overlay.isOpen() ) {
                printf( "opened successfully: mem=%p/%u\n", overlay.getMem(), overlay.getMemSize() );
                unsigned char val = 0 ;
                while(1){
                    memset(overlay.getMem(), val++, overlay.getMemSize());
                }
        }
        else
                ERRMSG( "Error opening v4l output\n" );
        return 0 ;
}
#endif
