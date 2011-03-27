/*
 * fbSet.cpp
 *
 * Simple illustration of access to the Frame Buffer
 * via mmap.
 *
 * Copyright Boundary Devices, 2005
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include "fourcc.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      int const fd = open( argv[1], O_RDWR );
      if( 0 <= fd )
      {
         fcntl( fd, F_SETFD, FD_CLOEXEC );
         struct fb_fix_screeninfo fixed_info;
         int err = ioctl( fd, FBIOGET_FSCREENINFO, &fixed_info);
         if( 0 == err )
         {
            printf( "id %s\n", fixed_info.id );
            printf( "smem_start 0x%lx\n", fixed_info.smem_start );
            printf( "smem_len   %u\n", fixed_info.smem_len );
            printf( "type       %u\n", fixed_info.type );
            printf( "type_aux   %u\n", fixed_info.type_aux );
            printf( "visual     %u\n", fixed_info.visual );
            printf( "xpan       %u\n", fixed_info.xpanstep );
            printf( "ypan       %u\n", fixed_info.ypanstep );
            printf( "ywrap      %u\n", fixed_info.ywrapstep );
            printf( "line_len   %u\n", fixed_info.line_length );
            printf( "mmio_start %lu\n", fixed_info.mmio_start );
            printf( "mmio_len   %u\n", fixed_info.mmio_len );
            printf( "accel      %u\n", fixed_info.accel );
            struct fb_var_screeninfo variable_info;

          err = ioctl( fd, FBIOGET_VSCREENINFO, &variable_info );
            if( 0 == err )
            {
               printf( "xres              = %u\n", variable_info.xres );            //  visible resolution
               printf( "yres              = %u\n", variable_info.yres );
               printf( "xres_virtual      = %u\n", variable_info.xres_virtual );        //  virtual resolution
               printf( "yres_virtual      = %u\n", variable_info.yres_virtual );
               printf( "xoffset           = %u\n", variable_info.xoffset );         //  offset from virtual to visible
               printf( "yoffset           = %u\n", variable_info.yoffset );         //  resolution
               printf( "bits_per_pixel    = %u\n", variable_info.bits_per_pixel );      //  guess what
               printf( "grayscale         = %u\n", variable_info.grayscale );       //  != 0 Graylevels instead of colors

               printf( "red               = offs %u, len %u, msbr %u\n",
                       variable_info.red.offset,
                       variable_info.red.length,
                       variable_info.red.msb_right );
               printf( "green             = offs %u, len %u, msbr %u\n",
                       variable_info.green.offset,
                       variable_info.green.length,
                       variable_info.green.msb_right );
               printf( "blue              = offs %u, len %u, msbr %u\n",
                       variable_info.blue.offset,
                       variable_info.blue.length,
                       variable_info.blue.msb_right );

               printf( "nonstd            = 0x%x - %s\n", variable_info.nonstd, fourcc_str(variable_info.nonstd) );          //  != 0 Non standard pixel format
               printf( "activate          = %u\n", variable_info.activate );            //  see FB_ACTIVATE_*
               printf( "height            = %u\n", variable_info.height );          //  height of picture in mm
               printf( "width             = %u\n", variable_info.width );           //  width of picture in mm
               printf( "accel_flags       = %u\n", variable_info.accel_flags );     //  acceleration flags (hints)
               printf( "pixclock          = %u\n", variable_info.pixclock );            //  pixel clock in ps (pico seconds)
               printf( "left_margin       = %u\n", variable_info.left_margin );     //  time from sync to picture
               printf( "right_margin      = %u\n", variable_info.right_margin );        //  time from picture to sync
               printf( "upper_margin      = %u\n", variable_info.upper_margin );        //  time from sync to picture
               printf( "lower_margin      = %u\n", variable_info.lower_margin );
               printf( "hsync_len         = %u\n", variable_info.hsync_len );		//  length of horizontal sync
               printf( "vsync_len         = %u\n", variable_info.vsync_len );		//  length of vertical sync
               printf( "sync              = %u\n", variable_info.sync );			//  see FB_SYNC_*
               printf( "vmode             = %u\n", variable_info.vmode );			//  see FB_VMODE_*

               printf( "%u x %u --> %u bytes\n",
                       variable_info.xres, variable_info.yres, fixed_info.smem_len );

	       if( 2 < argc ){
		       unsigned long rgb = strtoul( argv[2], 0, 0 );
		       void *mem = mmap( 0, fixed_info.smem_len, PROT_WRITE|PROT_WRITE,
					 MAP_SHARED, fd, 0 );
		       if( MAP_FAILED != mem )
		       {
			  unsigned char red   = (unsigned char)(rgb>>16);
			  unsigned char green = (unsigned char)(rgb>>8);
			  unsigned char blue  = (unsigned char)(rgb);
			  unsigned short rgb16 = ((unsigned short)(red>>(8-variable_info.red.length)) << 11)       // 5 bits of red
					       | ((unsigned short)(green>>(8-variable_info.green.length)) << 5)    // 6 bits of green
					       | ((unsigned short)(blue>>(8-variable_info.blue.length)));          // 5 bits of blue
			  unsigned long doubled = ( ((unsigned long)rgb16) << 16 )
						| rgb16 ;
			  memset( mem, doubled, fixed_info.smem_len );
		       }
		       else
			  perror( "mmap fb" );
	       }
            }
            else
               perror( "VSCREENINFO" );
         }
         else
            perror( "FSCREENINFO" );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: fbSet /dev/fb0 0xFFFFFF\n" );
   return 0 ;
}
