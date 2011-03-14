#ifndef __FB2_OVERLAY_H__
        #define __FB2_OVERLAY_H__ "$Id$"

/*
 * fb2_overlay.h
 *
 * This header file declares the fb2_overlay_t class, which is
 * used to display a YUV overlay on an i.MX51 processor. 
 *
 * When constructed with a specified input widtn and height
 * and output rectangle, this class will attempt to open the 
 * YUV device and configure it as specified.
 *
 * Usage generally involves checking for success (isOpen()),
 * then memcpy'ing to the frame buffer
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2010
 */

class fb2_overlay_t {
public:
	enum {
		NO_TRANSPARENCY = 0xffffffff
	};
        fb2_overlay_t(
                     unsigned outx, unsigned outy,
                     unsigned outw, unsigned outh,
                     unsigned transparency,  	// 0(transparent)..255(opaque) or NO_TRANSPARENCY
		     unsigned color_key,	// > 0xFFFF means none
                     unsigned long outfmt,	// fourcc
		     unsigned which_display=0 );// /dev/fbX
        ~fb2_overlay_t( void );

        bool isOpen( void ) const { return 0 <= fd_ ;}
        int getFd( void ) const { return fd_ ;}

	void *getMem( void ) const { return mem_ ; }
	unsigned getMemSize( void ) const { return memSize_ ; }
private:
        void close(void);

        fb2_overlay_t( fb2_overlay_t const & ); // no copies
        unsigned long 	outfmt_ ;
        int           	fd_ ;
	void 	       *mem_ ;
	unsigned long	memSize_ ;
};

#endif

